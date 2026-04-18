//===- YSXDeadRegisterDefinitions.cpp - Replace dead defs w/ zero reg --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//
// This pass rewrites Rd to x0 for instrs whose return values are unused.
//
//===---------------------------------------------------------------------===//

#include "YSX.h"
#include "YSXSubtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LiveDebugVariables.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveStacks.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;
#define DEBUG_TYPE "ysx-dead-defs"
#define YSX_DEAD_REG_DEF_NAME "RISC-V Dead register definitions"

STATISTIC(NumDeadDefsReplaced, "Number of dead definitions replaced");

namespace {
class YSXDeadRegisterDefinitions : public MachineFunctionPass {
public:
  static char ID;

  YSXDeadRegisterDefinitions() : MachineFunctionPass(ID) {}
  bool runOnMachineFunction(MachineFunction &MF) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<LiveIntervalsWrapperPass>();
    AU.addPreserved<LiveIntervalsWrapperPass>();
    AU.addRequired<LiveIntervalsWrapperPass>();
    AU.addPreserved<SlotIndexesWrapperPass>();
    AU.addPreserved<LiveDebugVariablesWrapperLegacy>();
    AU.addPreserved<LiveStacksWrapperLegacy>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return YSX_DEAD_REG_DEF_NAME; }
};
} // end anonymous namespace

char YSXDeadRegisterDefinitions::ID = 0;
INITIALIZE_PASS(YSXDeadRegisterDefinitions, DEBUG_TYPE,
                YSX_DEAD_REG_DEF_NAME, false, false)

FunctionPass *llvm::createYSXDeadRegisterDefinitionsPass() {
  return new YSXDeadRegisterDefinitions();
}

bool YSXDeadRegisterDefinitions::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  LiveIntervals &LIS = getAnalysis<LiveIntervalsWrapperPass>().getLIS();
  LLVM_DEBUG(dbgs() << "***** YSXDeadRegisterDefinitions *****\n");

  bool MadeChange = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // We only handle non-computational instructions since some NOP encodings
      // are reserved for HINT instructions.
      const MCInstrDesc &Desc = MI.getDesc();
      if (!Desc.mayLoad() && !Desc.mayStore() &&
          !Desc.hasUnmodeledSideEffects() &&
          MI.getOpcode() != YSX::PseudoVSETVLI &&
          MI.getOpcode() != YSX::PseudoVSETIVLI)
        continue;
      for (int I = 0, E = Desc.getNumDefs(); I != E; ++I) {
        MachineOperand &MO = MI.getOperand(I);
        if (!MO.isReg() || !MO.isDef() || MO.isEarlyClobber())
          continue;
        // Be careful not to change the register if it's a tied operand.
        if (MI.isRegTiedToUseOperand(I)) {
          LLVM_DEBUG(dbgs() << "    Ignoring, def is tied operand.\n");
          continue;
        }
        Register Reg = MO.getReg();
        if (!Reg.isVirtual() || !MO.isDead())
          continue;
        LLVM_DEBUG(dbgs() << "    Dead def operand #" << I << " in:\n      ";
                   MI.print(dbgs()));
        Register X0Reg;
        const TargetRegisterClass *RC = TII->getRegClass(Desc, I);
        if (RC && RC->contains(YSX::X0)) {
          X0Reg = YSX::X0;
        } else if (RC && RC->contains(YSX::X0_W)) {
          X0Reg = YSX::X0_W;
        } else if (RC && RC->contains(YSX::X0_H)) {
          X0Reg = YSX::X0_H;
        } else if (RC && RC->contains(YSX::X0_Pair)) {
          X0Reg = YSX::X0_Pair;
        } else {
          LLVM_DEBUG(dbgs() << "    Ignoring, register is not a GPR.\n");
          continue;
        }
        assert(LIS.hasInterval(Reg));
        LIS.removeInterval(Reg);
        MO.setReg(X0Reg);
        LLVM_DEBUG(dbgs() << "    Replacing with zero register. New:\n      ";
                   MI.print(dbgs()));
        ++NumDeadDefsReplaced;
        MadeChange = true;
      }
    }
  }

  return MadeChange;
}
