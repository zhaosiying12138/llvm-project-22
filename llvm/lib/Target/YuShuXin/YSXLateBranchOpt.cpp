//===-- YSXLateBranchOpt.cpp - Late Stage Branch Optimization -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file provides RISC-V specific target optimizations, currently it's
/// limited to convert conditional branches into unconditional branches when
/// the condition can be statically evaluated.
///
//===----------------------------------------------------------------------===//

#include "YSXInstrInfo.h"
#include "YSXSubtarget.h"

using namespace llvm;

#define YSX_LATE_BRANCH_OPT_NAME "RISC-V Late Branch Optimisation Pass"

namespace {

struct YSXLateBranchOpt : public MachineFunctionPass {
  static char ID;

  YSXLateBranchOpt() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override { return YSX_LATE_BRANCH_OPT_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &Fn) override;

private:
  bool runOnBasicBlock(MachineBasicBlock &MBB) const;

  const YSXInstrInfo *RII = nullptr;
};
} // namespace

char YSXLateBranchOpt::ID = 0;
INITIALIZE_PASS(YSXLateBranchOpt, "ysx-late-branch-opt",
                YSX_LATE_BRANCH_OPT_NAME, false, false)

bool YSXLateBranchOpt::runOnBasicBlock(MachineBasicBlock &MBB) const {
  MachineBasicBlock *TBB, *FBB;
  SmallVector<MachineOperand, 4> Cond;
  if (RII->analyzeBranch(MBB, TBB, FBB, Cond, /*AllowModify=*/false))
    return false;

  if (!TBB || Cond.size() != 3)
    return false;

  YSXCC::CondCode CC = YSXInstrInfo::getCondFromBranchOpc(Cond[0].getImm());
  assert(CC != YSXCC::COND_INVALID);

  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  // Try and convert a conditional branch that can be evaluated statically
  // into an unconditional branch.
  int64_t C0, C1;
  if (!YSXInstrInfo::isFromLoadImm(MRI, Cond[1], C0) ||
      !YSXInstrInfo::isFromLoadImm(MRI, Cond[2], C1))
    return false;

  MachineBasicBlock *Folded =
      YSXInstrInfo::evaluateCondBranch(CC, C0, C1) ? TBB : FBB;

  // At this point, its legal to optimize.
  RII->removeBranch(MBB);

  // Only need to insert a branch if we're not falling through.
  if (Folded) {
    DebugLoc DL = MBB.findBranchDebugLoc();
    RII->insertBranch(MBB, Folded, nullptr, {}, DL);
  }

  // Update the successors. Remove them all and add back the correct one.
  while (!MBB.succ_empty())
    MBB.removeSuccessor(MBB.succ_end() - 1);

  // If it's a fallthrough, we need to figure out where MBB is going.
  if (!Folded) {
    MachineFunction::iterator Fallthrough = ++MBB.getIterator();
    if (Fallthrough != MBB.getParent()->end())
      MBB.addSuccessor(&*Fallthrough);
  } else
    MBB.addSuccessor(Folded);

  return true;
}

bool YSXLateBranchOpt::runOnMachineFunction(MachineFunction &Fn) {
  if (skipFunction(Fn.getFunction()))
    return false;

  auto &ST = Fn.getSubtarget<YSXSubtarget>();
  RII = ST.getInstrInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : Fn)
    Changed |= runOnBasicBlock(MBB);
  return Changed;
}

FunctionPass *llvm::createYSXLateBranchOptPass() {
  return new YSXLateBranchOpt();
}
