//=- YSXRedundantCopyElimination.cpp - Remove useless copy for RISC-V -----=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass removes unnecessary zero copies in BBs that are targets of
// beqz/bnez instructions. For instance, the copy instruction in the code below
// can be removed because the beqz jumps to BB#2 when a0 is zero.
//  BB#1:
//    beqz %a0, <BB#2>
//  BB#2:
//    %a0 = COPY %x0
//
// This pass also recognizes Xqcibi branch-immediate forms when compared
// against non-zero immediates.
//
// This pass should be run after register allocation and is based on the
// earliest versions of AArch64RedundantCopyElimination.
//
// FIXME: Support compare with non-zero immediates where the immediate is stored
// in a register.
//
//===----------------------------------------------------------------------===//

#include "YSX.h"
#include "YSXInstrInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

#define DEBUG_TYPE "ysx-copyelim"

STATISTIC(NumCopiesRemoved, "Number of copies removed.");

namespace {
class YSXRedundantCopyElimination : public MachineFunctionPass {
  const MachineRegisterInfo *MRI;
  const TargetRegisterInfo *TRI;
  const TargetInstrInfo *TII;

public:
  static char ID;
  YSXRedundantCopyElimination() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;
  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }

  StringRef getPassName() const override {
    return "RISC-V Redundant Copy Elimination";
  }

private:
  bool optimizeBlock(MachineBasicBlock &MBB);
};

} // end anonymous namespace

char YSXRedundantCopyElimination::ID = 0;

INITIALIZE_PASS(YSXRedundantCopyElimination, "ysx-copyelim",
                "RISC-V Redundant Copy Elimination", false, false)

static bool
guaranteesZeroRegInBlock(MachineBasicBlock &MBB,
                         const SmallVectorImpl<MachineOperand> &Cond,
                         MachineBasicBlock *TBB) {
  assert(Cond.size() == 3 && "Unexpected number of operands");
  assert(TBB != nullptr && "Expected branch target basic block");
  auto Opc = Cond[0].getImm();
  if (Opc == YSX::BEQ && Cond[2].isReg() && Cond[2].getReg() == YSX::X0 &&
      TBB == &MBB)
    return true;
  if (Opc == YSX::BNE && Cond[2].isReg() && Cond[2].getReg() == YSX::X0 &&
      TBB != &MBB)
    return true;
  return false;
}

static bool
guaranteesRegEqualsImmInBlock(MachineBasicBlock &MBB,
                              const SmallVectorImpl<MachineOperand> &Cond,
                              MachineBasicBlock *TBB) {
  assert(Cond.size() == 3 && "Unexpected number of operands");
  assert(TBB != nullptr && "Expected branch target basic block");
  auto Opc = Cond[0].getImm();
  if ((Opc == YSX::QC_BEQI || Opc == YSX::QC_E_BEQI ||
       Opc == YSX::NDS_BEQC || Opc == YSX::BEQI) &&
      Cond[2].isImm() && Cond[2].getImm() != 0 && TBB == &MBB)
    return true;
  if ((Opc == YSX::QC_BNEI || Opc == YSX::QC_E_BNEI ||
       Opc == YSX::NDS_BNEC || Opc == YSX::BNEI) &&
      Cond[2].isImm() && Cond[2].getImm() != 0 && TBB != &MBB)
    return true;
  return false;
}

bool YSXRedundantCopyElimination::optimizeBlock(MachineBasicBlock &MBB) {
  // Check if the current basic block has a single predecessor.
  if (MBB.pred_size() != 1)
    return false;

  // Check if the predecessor has two successors, implying the block ends in a
  // conditional branch.
  MachineBasicBlock *PredMBB = *MBB.pred_begin();
  if (PredMBB->succ_size() != 2)
    return false;

  MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
  SmallVector<MachineOperand, 3> Cond;
  if (TII->analyzeBranch(*PredMBB, TBB, FBB, Cond, /*AllowModify*/ false) ||
      Cond.empty())
    return false;

  Register TargetReg = Cond[1].getReg();

  if (!TargetReg)
    return false;

  bool IsZeroCopy = guaranteesZeroRegInBlock(MBB, Cond, TBB);

  if (!IsZeroCopy && !guaranteesRegEqualsImmInBlock(MBB, Cond, TBB))
    return false;

  bool Changed = false;
  MachineBasicBlock::iterator LastChange = MBB.begin();
  // Remove redundant Copy instructions unless TargetReg is modified.
  for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end(); I != E;) {
    MachineInstr *MI = &*I;
    ++I;
    bool RemoveMI = false;
    if (IsZeroCopy) {
      if (MI->isCopy() && MI->getOperand(0).isReg() &&
          MI->getOperand(1).isReg()) {
        Register DefReg = MI->getOperand(0).getReg();
        Register SrcReg = MI->getOperand(1).getReg();

        if (SrcReg == YSX::X0 && !MRI->isReserved(DefReg) &&
            TargetReg == DefReg)
          RemoveMI = true;
      }
    } else {
      // Xqcibi, XAndesPref and Zibi compare with non-zero immediate:
      // remove redundant addi rd,x0,imm or qc.li rd,imm as applicable.
      if (MI->getOpcode() == YSX::ADDI && MI->getOperand(0).isReg() &&
          MI->getOperand(1).isReg() && MI->getOperand(2).isImm()) {
        Register DefReg = MI->getOperand(0).getReg();
        Register SrcReg = MI->getOperand(1).getReg();
        int64_t Imm = MI->getOperand(2).getImm();
        if (SrcReg == YSX::X0 && !MRI->isReserved(DefReg) &&
            TargetReg == DefReg && Imm == Cond[2].getImm())
          RemoveMI = true;
      } else if (MI->getOpcode() == YSX::QC_LI && MI->getOperand(0).isReg() &&
                 MI->getOperand(1).isImm()) {
        Register DefReg = MI->getOperand(0).getReg();
        int64_t Imm = MI->getOperand(1).getImm();
        if (!MRI->isReserved(DefReg) && TargetReg == DefReg &&
            Imm == Cond[2].getImm())
          RemoveMI = true;
      }
    }

    if (RemoveMI) {
      LLVM_DEBUG(dbgs() << "Remove redundant Copy: ");
      LLVM_DEBUG(MI->print(dbgs()));

      MI->eraseFromParent();
      Changed = true;
      LastChange = I;
      ++NumCopiesRemoved;
      continue;
    }

    if (MI->modifiesRegister(TargetReg, TRI))
      break;
  }

  if (!Changed)
    return false;

  MachineBasicBlock::iterator CondBr = PredMBB->getFirstTerminator();
  assert((CondBr->getOpcode() == YSX::BEQ ||
          CondBr->getOpcode() == YSX::BNE ||
          CondBr->getOpcode() == YSX::BEQI ||
          CondBr->getOpcode() == YSX::BNEI ||
          CondBr->getOpcode() == YSX::QC_BEQI ||
          CondBr->getOpcode() == YSX::QC_BNEI ||
          CondBr->getOpcode() == YSX::QC_E_BEQI ||
          CondBr->getOpcode() == YSX::QC_E_BNEI ||
          CondBr->getOpcode() == YSX::NDS_BEQC ||
          CondBr->getOpcode() == YSX::NDS_BNEC) &&
         "Unexpected opcode");
  assert(CondBr->getOperand(0).getReg() == TargetReg && "Unexpected register");

  // Otherwise, we have to fixup the use-def chain, starting with the
  // BEQ(I)/BNE(I). Conservatively mark as much as we can live.
  CondBr->clearRegisterKills(TargetReg, TRI);

  // Add newly used reg to the block's live-in list if it isn't there already.
  if (!MBB.isLiveIn(TargetReg))
    MBB.addLiveIn(TargetReg);

  // Clear any kills of TargetReg between CondBr and the last removed COPY.
  for (MachineInstr &MMI : make_range(MBB.begin(), LastChange))
    MMI.clearRegisterKills(TargetReg, TRI);

  return true;
}

bool YSXRedundantCopyElimination::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  TII = MF.getSubtarget().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  MRI = &MF.getRegInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF)
    Changed |= optimizeBlock(MBB);

  return Changed;
}

FunctionPass *llvm::createYSXRedundantCopyEliminationPass() {
  return new YSXRedundantCopyElimination();
}
