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

#include "llvm/CodeGen/LivePhysRegs.h"
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
  bool expandCCOp(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                  MachineBasicBlock::iterator &NextMBBI);
  bool expandCCOpToCMov(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI);
  bool expandVMSET_VMCLR(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI, unsigned Opcode);
  bool expandMV_FPR16INX(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandMV_FPR32INX(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandRV32ZdinxStore(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI);
  bool expandRV32ZdinxLoad(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI);
  bool expandPseudoReadVLENBViaVSETVLIX0(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MBBI);
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
#if 0
  // YSXInstrInfo::getInstSizeInBytes expects that the total size of the
  // expanded instructions for each pseudo is correct in the Size field of the
  // tablegen definition for the pseudo.
  switch (MBBI->getOpcode()) {
  case YSX::PseudoMV_FPR16INX:
    return expandMV_FPR16INX(MBB, MBBI);
  case YSX::PseudoMV_FPR32INX:
    return expandMV_FPR32INX(MBB, MBBI);
  case YSX::PseudoRV32ZdinxSD:
    return expandRV32ZdinxStore(MBB, MBBI);
  case YSX::PseudoRV32ZdinxLD:
    return expandRV32ZdinxLoad(MBB, MBBI);
  case YSX::PseudoCCMOVGPRNoX0:
  case YSX::PseudoCCMOVGPR:
  case YSX::PseudoCCADD:
  case YSX::PseudoCCSUB:
  case YSX::PseudoCCAND:
  case YSX::PseudoCCOR:
  case YSX::PseudoCCXOR:
  case YSX::PseudoCCMAX:
  case YSX::PseudoCCMAXU:
  case YSX::PseudoCCMIN:
  case YSX::PseudoCCMINU:
  case YSX::PseudoCCMUL:
  case YSX::PseudoCCLUI:
  case YSX::PseudoCCQC_E_LB:
  case YSX::PseudoCCQC_E_LH:
  case YSX::PseudoCCQC_E_LW:
  case YSX::PseudoCCQC_E_LHU:
  case YSX::PseudoCCQC_E_LBU:
  case YSX::PseudoCCLB:
  case YSX::PseudoCCLH:
  case YSX::PseudoCCLW:
  case YSX::PseudoCCLHU:
  case YSX::PseudoCCLBU:
  case YSX::PseudoCCLWU:
  case YSX::PseudoCCLD:
  case YSX::PseudoCCQC_LI:
  case YSX::PseudoCCQC_E_LI:
  case YSX::PseudoCCADDW:
  case YSX::PseudoCCSUBW:
  case YSX::PseudoCCSLL:
  case YSX::PseudoCCSRL:
  case YSX::PseudoCCSRA:
  case YSX::PseudoCCADDI:
  case YSX::PseudoCCSLLI:
  case YSX::PseudoCCSRLI:
  case YSX::PseudoCCSRAI:
  case YSX::PseudoCCANDI:
  case YSX::PseudoCCORI:
  case YSX::PseudoCCXORI:
  case YSX::PseudoCCSLLW:
  case YSX::PseudoCCSRLW:
  case YSX::PseudoCCSRAW:
  case YSX::PseudoCCADDIW:
  case YSX::PseudoCCSLLIW:
  case YSX::PseudoCCSRLIW:
  case YSX::PseudoCCSRAIW:
  case YSX::PseudoCCANDN:
  case YSX::PseudoCCORN:
  case YSX::PseudoCCXNOR:
  case YSX::PseudoCCNDS_BFOS:
  case YSX::PseudoCCNDS_BFOZ:
    return expandCCOp(MBB, MBBI, NextMBBI);
  case YSX::PseudoVMCLR_M_B1:
  case YSX::PseudoVMCLR_M_B2:
  case YSX::PseudoVMCLR_M_B4:
  case YSX::PseudoVMCLR_M_B8:
  case YSX::PseudoVMCLR_M_B16:
  case YSX::PseudoVMCLR_M_B32:
  case YSX::PseudoVMCLR_M_B64:
    // vmclr.m vd => vmxor.mm vd, vd, vd
    return expandVMSET_VMCLR(MBB, MBBI, YSX::VMXOR_MM);
  case YSX::PseudoVMSET_M_B1:
  case YSX::PseudoVMSET_M_B2:
  case YSX::PseudoVMSET_M_B4:
  case YSX::PseudoVMSET_M_B8:
  case YSX::PseudoVMSET_M_B16:
  case YSX::PseudoVMSET_M_B32:
  case YSX::PseudoVMSET_M_B64:
    // vmset.m vd => vmxnor.mm vd, vd, vd
    return expandVMSET_VMCLR(MBB, MBBI, YSX::VMXNOR_MM);
  case YSX::PseudoReadVLENBViaVSETVLIX0:
    return expandPseudoReadVLENBViaVSETVLIX0(MBB, MBBI);
  }
#endif

  return false;
}

bool YSXExpandPseudo::expandCCOp(MachineBasicBlock &MBB,
                                   MachineBasicBlock::iterator MBBI,
                                   MachineBasicBlock::iterator &NextMBBI) {
#if 0
  // First try expanding to a Conditional Move rather than a branch+mv
  if (expandCCOpToCMov(MBB, MBBI))
    return true;

  MachineFunction *MF = MBB.getParent();
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  MachineBasicBlock *TrueBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  MachineBasicBlock *MergeBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  MF->insert(++MBB.getIterator(), TrueBB);
  MF->insert(++TrueBB->getIterator(), MergeBB);

  // We want to copy the "true" value when the condition is true which means
  // we need to invert the branch condition to jump over TrueBB when the
  // condition is false.
  auto CC = static_cast<YSXCC::CondCode>(MI.getOperand(3).getImm());
  CC = YSXCC::getInverseBranchCondition(CC);

  // Insert branch instruction.
  BuildMI(MBB, MBBI, DL, TII->get(YSXCC::getBrCond(CC)))
      .addReg(MI.getOperand(1).getReg())
      .addReg(MI.getOperand(2).getReg())
      .addMBB(MergeBB);

  Register DestReg = MI.getOperand(0).getReg();
  assert(MI.getOperand(4).getReg() == DestReg);

  if (MI.getOpcode() == YSX::PseudoCCMOVGPR ||
      MI.getOpcode() == YSX::PseudoCCMOVGPRNoX0) {
    // Add MV.
    BuildMI(TrueBB, DL, TII->get(YSX::ADDI), DestReg)
        .add(MI.getOperand(5))
        .addImm(0);
  } else {
    unsigned NewOpc;
    // clang-format off
    switch (MI.getOpcode()) {
    default:
      llvm_unreachable("Unexpected opcode!");
    case YSX::PseudoCCADD:   NewOpc = YSX::ADD;   break;
    case YSX::PseudoCCSUB:   NewOpc = YSX::SUB;   break;
    case YSX::PseudoCCSLL:   NewOpc = YSX::SLL;   break;
    case YSX::PseudoCCSRL:   NewOpc = YSX::SRL;   break;
    case YSX::PseudoCCSRA:   NewOpc = YSX::SRA;   break;
    case YSX::PseudoCCAND:   NewOpc = YSX::AND;   break;
    case YSX::PseudoCCOR:    NewOpc = YSX::OR;    break;
    case YSX::PseudoCCXOR:   NewOpc = YSX::XOR;   break;
    case YSX::PseudoCCMAX:   NewOpc = YSX::MAX;   break;
    case YSX::PseudoCCMIN:   NewOpc = YSX::MIN;   break;
    case YSX::PseudoCCMAXU:  NewOpc = YSX::MAXU;  break;
    case YSX::PseudoCCMINU:  NewOpc = YSX::MINU;  break;
    case YSX::PseudoCCMUL:   NewOpc = YSX::MUL;   break;
    case YSX::PseudoCCLUI:   NewOpc = YSX::LUI;   break;
    case YSX::PseudoCCQC_E_LB:  NewOpc = YSX::QC_E_LB;    break;
    case YSX::PseudoCCQC_E_LH:  NewOpc = YSX::QC_E_LH;    break;
    case YSX::PseudoCCQC_E_LW:  NewOpc = YSX::QC_E_LW;    break;
    case YSX::PseudoCCQC_E_LHU: NewOpc = YSX::QC_E_LHU;   break;
    case YSX::PseudoCCQC_E_LBU: NewOpc = YSX::QC_E_LBU;   break;
    case YSX::PseudoCCLB:    NewOpc = YSX::LB;    break;
    case YSX::PseudoCCLH:    NewOpc = YSX::LH;    break;
    case YSX::PseudoCCLW:    NewOpc = YSX::LW;    break;
    case YSX::PseudoCCLHU:   NewOpc = YSX::LHU;   break;
    case YSX::PseudoCCLBU:   NewOpc = YSX::LBU;   break;
    case YSX::PseudoCCLWU:   NewOpc = YSX::LWU;   break;
    case YSX::PseudoCCLD:    NewOpc = YSX::LD;    break;
    case YSX::PseudoCCQC_LI:  NewOpc = YSX::QC_LI;   break;
    case YSX::PseudoCCQC_E_LI: NewOpc = YSX::QC_E_LI;   break;
    case YSX::PseudoCCADDI:  NewOpc = YSX::ADDI;  break;
    case YSX::PseudoCCSLLI:  NewOpc = YSX::SLLI;  break;
    case YSX::PseudoCCSRLI:  NewOpc = YSX::SRLI;  break;
    case YSX::PseudoCCSRAI:  NewOpc = YSX::SRAI;  break;
    case YSX::PseudoCCANDI:  NewOpc = YSX::ANDI;  break;
    case YSX::PseudoCCORI:   NewOpc = YSX::ORI;   break;
    case YSX::PseudoCCXORI:  NewOpc = YSX::XORI;  break;
    case YSX::PseudoCCADDW:  NewOpc = YSX::ADDW;  break;
    case YSX::PseudoCCSUBW:  NewOpc = YSX::SUBW;  break;
    case YSX::PseudoCCSLLW:  NewOpc = YSX::SLLW;  break;
    case YSX::PseudoCCSRLW:  NewOpc = YSX::SRLW;  break;
    case YSX::PseudoCCSRAW:  NewOpc = YSX::SRAW;  break;
    case YSX::PseudoCCADDIW: NewOpc = YSX::ADDIW; break;
    case YSX::PseudoCCSLLIW: NewOpc = YSX::SLLIW; break;
    case YSX::PseudoCCSRLIW: NewOpc = YSX::SRLIW; break;
    case YSX::PseudoCCSRAIW: NewOpc = YSX::SRAIW; break;
    case YSX::PseudoCCANDN:  NewOpc = YSX::ANDN;  break;
    case YSX::PseudoCCORN:   NewOpc = YSX::ORN;   break;
    case YSX::PseudoCCXNOR:  NewOpc = YSX::XNOR;  break;
    case YSX::PseudoCCNDS_BFOS: NewOpc = YSX::NDS_BFOS; break;
    case YSX::PseudoCCNDS_BFOZ: NewOpc = YSX::NDS_BFOZ; break;
    }
    // clang-format on

    if (NewOpc == YSX::NDS_BFOZ || NewOpc == YSX::NDS_BFOS) {
      BuildMI(TrueBB, DL, TII->get(NewOpc), DestReg)
          .add(MI.getOperand(5))
          .add(MI.getOperand(6))
          .add(MI.getOperand(7));
    } else if (NewOpc == YSX::LUI || NewOpc == YSX::QC_LI ||
               NewOpc == YSX::QC_E_LI) {
      BuildMI(TrueBB, DL, TII->get(NewOpc), DestReg).add(MI.getOperand(5));
    } else {
      BuildMI(TrueBB, DL, TII->get(NewOpc), DestReg)
          .add(MI.getOperand(5))
          .add(MI.getOperand(6));
    }
  }

  TrueBB->addSuccessor(MergeBB);

  MergeBB->splice(MergeBB->end(), &MBB, MI, MBB.end());
  MergeBB->transferSuccessors(&MBB);

  MBB.addSuccessor(TrueBB);
  MBB.addSuccessor(MergeBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  // Make sure live-ins are correctly attached to this new basic block.
  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *TrueBB);
  computeAndAddLiveIns(LiveRegs, *MergeBB);

  return true;
#endif
  return false;
}

bool YSXExpandPseudo::expandCCOpToCMov(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MBBI) {
#if 0
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  if (MI.getOpcode() != YSX::PseudoCCMOVGPR &&
      MI.getOpcode() != YSX::PseudoCCMOVGPRNoX0)
    return false;

  if (!STI->hasVendorXRemovedQcicm())
    return false;

  // FIXME: Would be wonderful to support LHS=X0, but not very easy.
  if (MI.getOperand(1).getReg() == YSX::X0 ||
      MI.getOperand(4).getReg() == YSX::X0 ||
      MI.getOperand(5).getReg() == YSX::X0)
    return false;

  auto CC = static_cast<YSXCC::CondCode>(MI.getOperand(3).getImm());

  unsigned CMovOpcode, CMovIOpcode;
  switch (CC) {
  default:
    llvm_unreachable("Unhandled CC");
  case YSXCC::COND_EQ:
    CMovOpcode = YSX::QC_MVEQ;
    CMovIOpcode = YSX::QC_MVEQI;
    break;
  case YSXCC::COND_NE:
    CMovOpcode = YSX::QC_MVNE;
    CMovIOpcode = YSX::QC_MVNEI;
    break;
  case YSXCC::COND_LT:
    CMovOpcode = YSX::QC_MVLT;
    CMovIOpcode = YSX::QC_MVLTI;
    break;
  case YSXCC::COND_GE:
    CMovOpcode = YSX::QC_MVGE;
    CMovIOpcode = YSX::QC_MVGEI;
    break;
  case YSXCC::COND_LTU:
    CMovOpcode = YSX::QC_MVLTU;
    CMovIOpcode = YSX::QC_MVLTUI;
    break;
  case YSXCC::COND_GEU:
    CMovOpcode = YSX::QC_MVGEU;
    CMovIOpcode = YSX::QC_MVGEUI;
    break;
  }

  if (MI.getOperand(2).getReg() == YSX::X0) {
    // $dst = PseudoCCMOVGPR $lhs, X0, $cc, $falsev (=$dst), $truev
    // $dst = PseudoCCMOVGPRNoX0 $lhs, X0, $cc, $falsev (=$dst), $truev
    // =>
    // $dst = QC_MVccI $falsev (=$dst), $lhs, 0, $truev
    BuildMI(MBB, MBBI, DL, TII->get(CMovIOpcode))
        .addDef(MI.getOperand(0).getReg())
        .addReg(MI.getOperand(4).getReg())
        .addReg(MI.getOperand(1).getReg())
        .addImm(0)
        .addReg(MI.getOperand(5).getReg());

    MI.eraseFromParent();
    return true;
  }

  // $dst = PseudoCCMOVGPR $lhs, $rhs, $cc, $falsev (=$dst), $truev
  // $dst = PseudoCCMOVGPRNoX0 $lhs, $rhs, $cc, $falsev (=$dst), $truev
  // =>
  // $dst = QC_MVcc $falsev (=$dst), $lhs, $rhs, $truev
  BuildMI(MBB, MBBI, DL, TII->get(CMovOpcode))
      .addDef(MI.getOperand(0).getReg())
      .addReg(MI.getOperand(4).getReg())
      .addReg(MI.getOperand(1).getReg())
      .addReg(MI.getOperand(2).getReg())
      .addReg(MI.getOperand(5).getReg());
  MI.eraseFromParent();
  return true;
#endif
  return false;
}

bool YSXExpandPseudo::expandVMSET_VMCLR(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI,
                                          unsigned Opcode) {
#if 0
  DebugLoc DL = MBBI->getDebugLoc();
  Register DstReg = MBBI->getOperand(0).getReg();
  const MCInstrDesc &Desc = TII->get(Opcode);
  BuildMI(MBB, MBBI, DL, Desc, DstReg)
      .addReg(DstReg, RegState::Undef)
      .addReg(DstReg, RegState::Undef);
  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
#endif
  return false;
}

bool YSXExpandPseudo::expandMV_FPR16INX(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  const TargetRegisterInfo *TRI = STI->getRegisterInfo();
  Register DstReg = TRI->getMatchingSuperReg(
      MBBI->getOperand(0).getReg(), YSX::sub_16, &YSX::GPRRegClass);
  Register SrcReg = TRI->getMatchingSuperReg(
      MBBI->getOperand(1).getReg(), YSX::sub_16, &YSX::GPRRegClass);

  BuildMI(MBB, MBBI, DL, TII->get(YSX::ADDI), DstReg)
      .addReg(SrcReg, getKillRegState(MBBI->getOperand(1).isKill()))
      .addImm(0);

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool YSXExpandPseudo::expandMV_FPR32INX(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  const TargetRegisterInfo *TRI = STI->getRegisterInfo();
  Register DstReg = TRI->getMatchingSuperReg(
      MBBI->getOperand(0).getReg(), YSX::sub_32, &YSX::GPRRegClass);
  Register SrcReg = TRI->getMatchingSuperReg(
      MBBI->getOperand(1).getReg(), YSX::sub_32, &YSX::GPRRegClass);

  BuildMI(MBB, MBBI, DL, TII->get(YSX::ADDI), DstReg)
      .addReg(SrcReg, getKillRegState(MBBI->getOperand(1).isKill()))
      .addImm(0);

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

// This function expands the PseudoRV32ZdinxSD for storing a double-precision
// floating-point value into memory by generating an equivalent instruction
// sequence for RV32.
bool YSXExpandPseudo::expandRV32ZdinxStore(MachineBasicBlock &MBB,
                                             MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  const TargetRegisterInfo *TRI = STI->getRegisterInfo();
  Register Lo =
      TRI->getSubReg(MBBI->getOperand(0).getReg(), YSX::sub_gpr_even);
  Register Hi =
      TRI->getSubReg(MBBI->getOperand(0).getReg(), YSX::sub_gpr_odd);
  if (Hi == YSX::DUMMY_REG_PAIR_WITH_X0)
    Hi = YSX::X0;

  auto MIBLo = BuildMI(MBB, MBBI, DL, TII->get(YSX::SW))
                   .addReg(Lo, getKillRegState(MBBI->getOperand(0).isKill()))
                   .addReg(MBBI->getOperand(1).getReg())
                   .add(MBBI->getOperand(2));

  MachineInstrBuilder MIBHi;
  if (MBBI->getOperand(2).isGlobal() || MBBI->getOperand(2).isCPI()) {
    assert(MBBI->getOperand(2).getOffset() % 8 == 0);
    MBBI->getOperand(2).setOffset(MBBI->getOperand(2).getOffset() + 4);
    MIBHi = BuildMI(MBB, MBBI, DL, TII->get(YSX::SW))
                .addReg(Hi, getKillRegState(MBBI->getOperand(0).isKill()))
                .add(MBBI->getOperand(1))
                .add(MBBI->getOperand(2));
  } else {
    assert(isInt<12>(MBBI->getOperand(2).getImm() + 4));
    MIBHi = BuildMI(MBB, MBBI, DL, TII->get(YSX::SW))
                .addReg(Hi, getKillRegState(MBBI->getOperand(0).isKill()))
                .add(MBBI->getOperand(1))
                .addImm(MBBI->getOperand(2).getImm() + 4);
  }

  MachineFunction *MF = MBB.getParent();
  SmallVector<MachineMemOperand *> NewLoMMOs;
  SmallVector<MachineMemOperand *> NewHiMMOs;
  for (const MachineMemOperand *MMO : MBBI->memoperands()) {
    NewLoMMOs.push_back(MF->getMachineMemOperand(MMO, 0, 4));
    NewHiMMOs.push_back(MF->getMachineMemOperand(MMO, 4, 4));
  }
  MIBLo.setMemRefs(NewLoMMOs);
  MIBHi.setMemRefs(NewHiMMOs);

  MBBI->eraseFromParent();
  return true;
}

// This function expands PseudoRV32ZdinxLoad for loading a double-precision
// floating-point value from memory into an equivalent instruction sequence for
// RV32.
bool YSXExpandPseudo::expandRV32ZdinxLoad(MachineBasicBlock &MBB,
                                            MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  const TargetRegisterInfo *TRI = STI->getRegisterInfo();
  Register Lo =
      TRI->getSubReg(MBBI->getOperand(0).getReg(), YSX::sub_gpr_even);
  Register Hi =
      TRI->getSubReg(MBBI->getOperand(0).getReg(), YSX::sub_gpr_odd);
  assert(Hi != YSX::DUMMY_REG_PAIR_WITH_X0 && "Cannot write to X0_Pair");

  MachineInstrBuilder MIBLo, MIBHi;

  // If the register of operand 1 is equal to the Lo register, then swap the
  // order of loading the Lo and Hi statements.
  bool IsOp1EqualToLo = Lo == MBBI->getOperand(1).getReg();
  // Order: Lo, Hi
  if (!IsOp1EqualToLo) {
    MIBLo = BuildMI(MBB, MBBI, DL, TII->get(YSX::LW), Lo)
                .addReg(MBBI->getOperand(1).getReg())
                .add(MBBI->getOperand(2));
  }

  if (MBBI->getOperand(2).isGlobal() || MBBI->getOperand(2).isCPI()) {
    auto Offset = MBBI->getOperand(2).getOffset();
    assert(Offset % 8 == 0);
    MBBI->getOperand(2).setOffset(Offset + 4);
    MIBHi = BuildMI(MBB, MBBI, DL, TII->get(YSX::LW), Hi)
                .addReg(MBBI->getOperand(1).getReg())
                .add(MBBI->getOperand(2));
    MBBI->getOperand(2).setOffset(Offset);
  } else {
    assert(isInt<12>(MBBI->getOperand(2).getImm() + 4));
    MIBHi = BuildMI(MBB, MBBI, DL, TII->get(YSX::LW), Hi)
                .addReg(MBBI->getOperand(1).getReg())
                .addImm(MBBI->getOperand(2).getImm() + 4);
  }

  // Order: Hi, Lo
  if (IsOp1EqualToLo) {
    MIBLo = BuildMI(MBB, MBBI, DL, TII->get(YSX::LW), Lo)
                .addReg(MBBI->getOperand(1).getReg())
                .add(MBBI->getOperand(2));
  }

  MachineFunction *MF = MBB.getParent();
  SmallVector<MachineMemOperand *> NewLoMMOs;
  SmallVector<MachineMemOperand *> NewHiMMOs;
  for (const MachineMemOperand *MMO : MBBI->memoperands()) {
    NewLoMMOs.push_back(MF->getMachineMemOperand(MMO, 0, 4));
    NewHiMMOs.push_back(MF->getMachineMemOperand(MMO, 4, 4));
  }
  MIBLo.setMemRefs(NewLoMMOs);
  MIBHi.setMemRefs(NewHiMMOs);

  MBBI->eraseFromParent();
  return true;
}

bool YSXExpandPseudo::expandPseudoReadVLENBViaVSETVLIX0(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI) {
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
