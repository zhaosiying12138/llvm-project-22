//===-- YSXExpandPseudoInsts.cpp - Expand pseudo instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands pseudo instructions into target
// instructions. This pass should be run after register allocation but before
// the post-regalloc scheduling pass.
//
//===----------------------------------------------------------------------===//

#include "YSX.h"
#include "YSXInstrInfo.h"
#include "YSXTargetMachine.h"

#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define YSX_EXPAND_PSEUDO_NAME "RISC-V pseudo instruction expansion pass"
#define YSX_PRERA_EXPAND_PSEUDO_NAME "RISC-V Pre-RA pseudo instruction expansion pass"

namespace {

class YSXExpandPseudo : public MachineFunctionPass {
public:
  const YSXSubtarget *STI;
  const YSXInstrInfo *TII;
  static char ID;

  YSXExpandPseudo() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return YSX_EXPAND_PSEUDO_NAME; }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
#ifndef NDEBUG
  unsigned getInstSizeInBytes(const MachineFunction &MF) const {
    unsigned Size = 0;
    for (auto &MBB : MF)
      for (auto &MI : MBB)
        Size += TII->getInstSizeInBytes(MI);
    return Size;
  }
#endif
};

char YSXExpandPseudo::ID = 0;

bool YSXExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  STI = &MF.getSubtarget<YSXSubtarget>();
  TII = STI->getInstrInfo();

#ifndef NDEBUG
  const unsigned OldSize = getInstSizeInBytes(MF);
#endif

  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);

#ifndef NDEBUG
  const unsigned NewSize = getInstSizeInBytes(MF);
  assert(OldSize >= NewSize);
#endif
  return Modified;
}

bool YSXExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool YSXExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  (void)MBB;
  (void)MBBI;
  (void)NextMBBI;
  return false;
}

class YSXPreRAExpandPseudo : public MachineFunctionPass {
public:
  const YSXSubtarget *STI;
  const YSXInstrInfo *TII;
  static char ID;

  YSXPreRAExpandPseudo() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
  StringRef getPassName() const override {
    return YSX_PRERA_EXPAND_PSEUDO_NAME;
  }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandAuipcInstPair(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI,
                           MachineBasicBlock::iterator &NextMBBI,
                           unsigned FlagsHi, unsigned SecondOpcode);
  bool expandLoadLocalAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadGlobalAddress(MachineBasicBlock &MBB,
                               MachineBasicBlock::iterator MBBI,
                               MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSIEAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSGDAddress(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI,
                              MachineBasicBlock::iterator &NextMBBI);
  bool expandLoadTLSDescAddress(MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MBBI,
                                MachineBasicBlock::iterator &NextMBBI);

#ifndef NDEBUG
  unsigned getInstSizeInBytes(const MachineFunction &MF) const {
    unsigned Size = 0;
    for (auto &MBB : MF)
      for (auto &MI : MBB)
        Size += TII->getInstSizeInBytes(MI);
    return Size;
  }
#endif
};

char YSXPreRAExpandPseudo::ID = 0;

bool YSXPreRAExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  STI = &MF.getSubtarget<YSXSubtarget>();
  TII = STI->getInstrInfo();

#ifndef NDEBUG
  const unsigned OldSize = getInstSizeInBytes(MF);
#endif

  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);

#ifndef NDEBUG
  const unsigned NewSize = getInstSizeInBytes(MF);
  assert(OldSize >= NewSize);
#endif
  return Modified;
}

bool YSXPreRAExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool YSXPreRAExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI,
                                      MachineBasicBlock::iterator &NextMBBI) {

  switch (MBBI->getOpcode()) {
  case YSX::PseudoLLA:
    return expandLoadLocalAddress(MBB, MBBI, NextMBBI);
  case YSX::PseudoLGA:
    return expandLoadGlobalAddress(MBB, MBBI, NextMBBI);
  case YSX::PseudoLA_TLS_IE:
    return expandLoadTLSIEAddress(MBB, MBBI, NextMBBI);
  case YSX::PseudoLA_TLS_GD:
    return expandLoadTLSGDAddress(MBB, MBBI, NextMBBI);
  case YSX::PseudoLA_TLSDESC:
    return expandLoadTLSDescAddress(MBB, MBBI, NextMBBI);
  }
  return false;
}

bool YSXPreRAExpandPseudo::expandAuipcInstPair(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI, unsigned FlagsHi,
    unsigned SecondOpcode) {
  MachineFunction *MF = MBB.getParent();
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  Register DestReg = MI.getOperand(0).getReg();
  Register ScratchReg =
      MF->getRegInfo().createVirtualRegister(&YSX::GPRRegClass);

  MachineOperand &Symbol = MI.getOperand(1);
  Symbol.setTargetFlags(FlagsHi);
  MCSymbol *AUIPCSymbol = MF->getContext().createNamedTempSymbol("pcrel_hi");

  MachineInstr *MIAUIPC =
      BuildMI(MBB, MBBI, DL, TII->get(YSX::AUIPC), ScratchReg).add(Symbol);
  MIAUIPC->setPreInstrSymbol(*MF, AUIPCSymbol);

  MachineInstr *SecondMI =
      BuildMI(MBB, MBBI, DL, TII->get(SecondOpcode), DestReg)
          .addReg(ScratchReg)
          .addSym(AUIPCSymbol, YSXII::MO_PCREL_LO);

  if (MI.hasOneMemOperand())
    SecondMI->addMemOperand(*MF, *MI.memoperands_begin());

  MI.eraseFromParent();
  return true;
}

bool YSXPreRAExpandPseudo::expandLoadLocalAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, YSXII::MO_PCREL_HI,
                             YSX::ADDI);
}

bool YSXPreRAExpandPseudo::expandLoadGlobalAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  unsigned SecondOpcode = STI->is64Bit() ? YSX::LD : YSX::LW;
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, YSXII::MO_GOT_HI,
                             SecondOpcode);
}

bool YSXPreRAExpandPseudo::expandLoadTLSIEAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  unsigned SecondOpcode = STI->is64Bit() ? YSX::LD : YSX::LW;
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, YSXII::MO_TLS_GOT_HI,
                             SecondOpcode);
}

bool YSXPreRAExpandPseudo::expandLoadTLSGDAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  return expandAuipcInstPair(MBB, MBBI, NextMBBI, YSXII::MO_TLS_GD_HI,
                             YSX::ADDI);
}

bool YSXPreRAExpandPseudo::expandLoadTLSDescAddress(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    MachineBasicBlock::iterator &NextMBBI) {
  MachineFunction *MF = MBB.getParent();
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  const auto &STI = MF->getSubtarget<YSXSubtarget>();
  unsigned SecondOpcode = STI.is64Bit() ? YSX::LD : YSX::LW;

  Register FinalReg = MI.getOperand(0).getReg();
  Register DestReg =
      MF->getRegInfo().createVirtualRegister(&YSX::GPRRegClass);
  Register ScratchReg =
      MF->getRegInfo().createVirtualRegister(&YSX::GPRRegClass);

  MachineOperand &Symbol = MI.getOperand(1);
  Symbol.setTargetFlags(YSXII::MO_TLSDESC_HI);
  MCSymbol *AUIPCSymbol = MF->getContext().createNamedTempSymbol("tlsdesc_hi");

  MachineInstr *MIAUIPC =
      BuildMI(MBB, MBBI, DL, TII->get(YSX::AUIPC), ScratchReg).add(Symbol);
  MIAUIPC->setPreInstrSymbol(*MF, AUIPCSymbol);

  BuildMI(MBB, MBBI, DL, TII->get(SecondOpcode), DestReg)
      .addReg(ScratchReg)
      .addSym(AUIPCSymbol, YSXII::MO_TLSDESC_LOAD_LO);

  BuildMI(MBB, MBBI, DL, TII->get(YSX::ADDI), YSX::X10)
      .addReg(ScratchReg)
      .addSym(AUIPCSymbol, YSXII::MO_TLSDESC_ADD_LO);

  BuildMI(MBB, MBBI, DL, TII->get(YSX::PseudoTLSDESCCall), YSX::X5)
      .addReg(DestReg)
      .addImm(0)
      .addSym(AUIPCSymbol, YSXII::MO_TLSDESC_CALL);

  BuildMI(MBB, MBBI, DL, TII->get(YSX::ADD), FinalReg)
      .addReg(YSX::X10)
      .addReg(YSX::X4);

  MI.eraseFromParent();
  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(YSXExpandPseudo, "ysx-expand-pseudo",
                YSX_EXPAND_PSEUDO_NAME, false, false)

INITIALIZE_PASS(YSXPreRAExpandPseudo, "ysx-prera-expand-pseudo",
                YSX_PRERA_EXPAND_PSEUDO_NAME, false, false)

namespace llvm {

FunctionPass *createYSXExpandPseudoPass() { return new YSXExpandPseudo(); }
FunctionPass *createYSXPreRAExpandPseudoPass() { return new YSXPreRAExpandPseudo(); }

} // end of namespace llvm
