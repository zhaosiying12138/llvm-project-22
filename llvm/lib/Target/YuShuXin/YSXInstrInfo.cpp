//===-- YSXInstrInfo.cpp - RISC-V Instruction Information -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the RISC-V implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#include "YSXInstrInfo.h"
#include "MCTargetDesc/YSXBaseInfo.h"
#include "MCTargetDesc/YSXMatInt.h"
#include "YSX.h"
#include "YSXMachineFunctionInfo.h"
#include "YSXSubtarget.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/CodeGen/MachineCombinerPattern.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineTraceMetrics.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#define GET_INSTRINFO_NAMED_OPS
#include "YSXGenInstrInfo.inc"

#define DEBUG_TYPE "ysx-instr-info"
STATISTIC(NumVRegSpilled,
          "Number of registers within vector register groups spilled");
STATISTIC(NumVRegReloaded,
          "Number of registers within vector register groups reloaded");

static bool isCompressibleInst(const MachineInstr &MI,
                               const YSXSubtarget &STI) {
  return false;
}

static cl::opt<bool> PreferWholeRegisterMove(
    "ysx-prefer-whole-register-move", cl::init(false), cl::Hidden,
    cl::desc("Prefer whole register move for vector registers."));

static cl::opt<MachineTraceStrategy> ForceMachineCombinerStrategy(
    "ysx-force-machine-combiner-strategy", cl::Hidden,
    cl::desc("Force machine combiner to use a specific strategy for machine "
             "trace metrics evaluation."),
    cl::init(MachineTraceStrategy::TS_NumStrategies),
    cl::values(clEnumValN(MachineTraceStrategy::TS_Local, "local",
                          "Local strategy."),
               clEnumValN(MachineTraceStrategy::TS_MinInstrCount, "min-instr",
                          "MinInstrCount strategy.")));

namespace llvm::YSXVPseudosTable {

using namespace YSX;

#define GET_YSXVPseudosTable_IMPL
#include "YSXGenSearchableTables.inc"

} // namespace llvm::YSXVPseudosTable

namespace llvm::YSX {

#define GET_YSXMaskedPseudosTable_IMPL
#include "YSXGenSearchableTables.inc"

} // end namespace llvm::YSX

YSXInstrInfo::YSXInstrInfo(const YSXSubtarget &STI)
    : YSXGenInstrInfo(STI, RegInfo, YSX::ADJCALLSTACKDOWN,
                        YSX::ADJCALLSTACKUP),
      RegInfo(STI.getHwMode()), STI(STI) {}

#define GET_INSTRINFO_HELPERS
#include "YSXGenInstrInfo.inc"

MCInst YSXInstrInfo::getNop() const {
  return MCInstBuilder(YSX::ADDI)
      .addReg(YSX::X0)
      .addReg(YSX::X0)
      .addImm(0);
}

Register YSXInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                             int &FrameIndex) const {
  TypeSize Dummy = TypeSize::getZero();
  return isLoadFromStackSlot(MI, FrameIndex, Dummy);
}

Register YSXInstrInfo::isLoadFromStackSlot(const MachineInstr &MI,
                                             int &FrameIndex,
                                             TypeSize &MemBytes) const {
  switch (MI.getOpcode()) {
  default:
    return 0;
  case YSX::LB:
  case YSX::LBU:
    MemBytes = TypeSize::getFixed(1);
    break;
  case YSX::LH:
  case YSX::LHU:
    MemBytes = TypeSize::getFixed(2);
    break;
  case YSX::LW:
  case YSX::LWU:
    MemBytes = TypeSize::getFixed(4);
    break;
  case YSX::LD:
    MemBytes = TypeSize::getFixed(8);
    break;
  }

  if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
      MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }

  return 0;
}

Register YSXInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                            int &FrameIndex) const {
  TypeSize Dummy = TypeSize::getZero();
  return isStoreToStackSlot(MI, FrameIndex, Dummy);
}

Register YSXInstrInfo::isStoreToStackSlot(const MachineInstr &MI,
                                            int &FrameIndex,
                                            TypeSize &MemBytes) const {
  switch (MI.getOpcode()) {
  default:
    return 0;
  case YSX::SB:
    MemBytes = TypeSize::getFixed(1);
    break;
  case YSX::SH:
    MemBytes = TypeSize::getFixed(2);
    break;
  case YSX::SW:
    MemBytes = TypeSize::getFixed(4);
    break;
  case YSX::SD:
    MemBytes = TypeSize::getFixed(8);
    break;
  }

  if (MI.getOperand(1).isFI() && MI.getOperand(2).isImm() &&
      MI.getOperand(2).getImm() == 0) {
    FrameIndex = MI.getOperand(1).getIndex();
    return MI.getOperand(0).getReg();
  }

  return 0;
}

bool YSXInstrInfo::isReMaterializableImpl(
    const MachineInstr &MI) const {
  return TargetInstrInfo::isReMaterializableImpl(MI);
}

void YSXInstrInfo::copyPhysRegVector(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
    const DebugLoc &DL, MCRegister DstReg, MCRegister SrcReg, bool KillSrc,
    const TargetRegisterClass *RegClass) const {
  llvm_unreachable("YSX does not support vector register copies");
}

void YSXInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 const DebugLoc &DL, Register DstReg,
                                 Register SrcReg, bool KillSrc,
                                 bool RenamableDest, bool RenamableSrc) const {
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();
  unsigned KillFlag = getKillRegState(KillSrc);

  if (YSX::GPRRegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(YSX::ADDI), DstReg)
        .addReg(SrcReg, KillFlag | getRenamableRegState(RenamableSrc))
        .addImm(0);
    return;
  }

#if 0
  if (YSX::GPRF16RegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(YSX::PseudoMV_FPR16INX), DstReg)
        .addReg(SrcReg, KillFlag | getRenamableRegState(RenamableSrc));
    return;
  }

  if (YSX::GPRF32RegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(YSX::PseudoMV_FPR32INX), DstReg)
        .addReg(SrcReg, KillFlag | getRenamableRegState(RenamableSrc));
    return;
  }
#endif

  if (YSX::GPRPairRegClass.contains(DstReg, SrcReg)) {
#if 0
    if (STI.isRV32() && STI.hasStdExtZdinx()) {
      // On RV32_Zdinx, FMV.D will move a pair of registers to another pair of
      // registers, in one instruction.
      BuildMI(MBB, MBBI, DL, get(YSX::FSGNJ_D_IN32X), DstReg)
          .addReg(SrcReg, getRenamableRegState(RenamableSrc))
          .addReg(SrcReg, KillFlag | getRenamableRegState(RenamableSrc));
      return;
    }
#endif

    MCRegister EvenReg = TRI->getSubReg(SrcReg, YSX::sub_gpr_even);
    MCRegister OddReg = TRI->getSubReg(SrcReg, YSX::sub_gpr_odd);
    // We need to correct the odd register of X0_Pair.
    if (OddReg == YSX::DUMMY_REG_PAIR_WITH_X0)
      OddReg = YSX::X0;
    assert(DstReg != YSX::X0_Pair && "Cannot write to X0_Pair");

    // Emit an ADDI for both parts of GPRPair.
    BuildMI(MBB, MBBI, DL, get(YSX::ADDI),
            TRI->getSubReg(DstReg, YSX::sub_gpr_even))
        .addReg(EvenReg, KillFlag)
        .addImm(0);
    BuildMI(MBB, MBBI, DL, get(YSX::ADDI),
            TRI->getSubReg(DstReg, YSX::sub_gpr_odd))
        .addReg(OddReg, KillFlag)
        .addImm(0);
    return;
  }

#if 0
  if (YSX::FPR16RegClass.contains(DstReg, SrcReg)) {
    unsigned Opc;
    if (STI.hasStdExtZfh()) {
      Opc = YSX::FSGNJ_H;
    } else {
      assert(STI.hasStdExtF() &&
             (STI.hasStdExtZfhmin() || STI.hasStdExtZfbfmin()) &&
             "Unexpected extensions");
      // Zfhmin/Zfbfmin doesn't have FSGNJ_H, replace FSGNJ_H with FSGNJ_S.
      DstReg = TRI->getMatchingSuperReg(DstReg, YSX::sub_16,
                                        &YSX::FPR32RegClass);
      SrcReg = TRI->getMatchingSuperReg(SrcReg, YSX::sub_16,
                                        &YSX::FPR32RegClass);
      Opc = YSX::FSGNJ_S;
    }
    BuildMI(MBB, MBBI, DL, get(Opc), DstReg)
        .addReg(SrcReg, KillFlag)
        .addReg(SrcReg, KillFlag);
    return;
  }

  if (YSX::FPR32RegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(YSX::FSGNJ_S), DstReg)
        .addReg(SrcReg, KillFlag)
        .addReg(SrcReg, KillFlag);
    return;
  }

  if (YSX::FPR64RegClass.contains(DstReg, SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(YSX::FSGNJ_D), DstReg)
        .addReg(SrcReg, KillFlag)
        .addReg(SrcReg, KillFlag);
    return;
  }

  if (YSX::FPR32RegClass.contains(DstReg) &&
      YSX::GPRRegClass.contains(SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(YSX::FMV_W_X), DstReg)
        .addReg(SrcReg, KillFlag);
    return;
  }

  if (YSX::GPRRegClass.contains(DstReg) &&
      YSX::FPR32RegClass.contains(SrcReg)) {
    BuildMI(MBB, MBBI, DL, get(YSX::FMV_X_W), DstReg)
        .addReg(SrcReg, KillFlag);
    return;
  }

  if (YSX::FPR64RegClass.contains(DstReg) &&
      YSX::GPRRegClass.contains(SrcReg)) {
    assert(STI.getXLen() == 64 && "Unexpected GPR size");
    BuildMI(MBB, MBBI, DL, get(YSX::FMV_D_X), DstReg)
        .addReg(SrcReg, KillFlag);
    return;
  }

  if (YSX::GPRRegClass.contains(DstReg) &&
      YSX::FPR64RegClass.contains(SrcReg)) {
    assert(STI.getXLen() == 64 && "Unexpected GPR size");
    BuildMI(MBB, MBBI, DL, get(YSX::FMV_X_D), DstReg)
        .addReg(SrcReg, KillFlag);
    return;
  }

  // VR->VR copies.
  const TargetRegisterClass *RegClass =
      TRI->getCommonMinimalPhysRegClass(SrcReg, DstReg);
  if (YSXRegisterInfo::isYSXVecRegClass(RegClass)) {
    copyPhysRegVector(MBB, MBBI, DL, DstReg, SrcReg, KillSrc, RegClass);
    return;
  }
#endif

  llvm_unreachable("Impossible reg-to-reg copy");
}

void YSXInstrInfo::storeRegToStackSlot(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator I,
                                         Register SrcReg, bool IsKill, int FI,
                                         const TargetRegisterClass *RC,
                                         Register VReg,
                                         MachineInstr::MIFlag Flags) const {
  MachineFunction *MF = MBB.getParent();
  MachineFrameInfo &MFI = MF->getFrameInfo();
  Align Alignment = MFI.getObjectAlign(FI);

  if (!YSX::GPRRegClass.hasSubClassEq(RC))
    llvm_unreachable("Can't store this register to stack slot");

  unsigned Opcode = YSX::SD;
  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOStore,
      MFI.getObjectSize(FI), Alignment);

  BuildMI(MBB, I, DebugLoc(), get(Opcode))
      .addReg(SrcReg, getKillRegState(IsKill))
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(MMO)
      .setMIFlag(Flags);
}

void YSXInstrInfo::loadRegFromStackSlot(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator I,
                                          Register DstReg, int FI,
                                          const TargetRegisterClass *RC,
                                          Register VReg, unsigned SubReg,
                                          MachineInstr::MIFlag Flags) const {
  MachineFunction *MF = MBB.getParent();
  MachineFrameInfo &MFI = MF->getFrameInfo();
  Align Alignment = MFI.getObjectAlign(FI);
  DebugLoc DL =
      Flags & MachineInstr::FrameDestroy ? MBB.findDebugLoc(I) : DebugLoc();

  if (!YSX::GPRRegClass.hasSubClassEq(RC))
    llvm_unreachable("Can't load this register from stack slot");

  unsigned Opcode = YSX::LD;
  MachineMemOperand *MMO = MF->getMachineMemOperand(
      MachinePointerInfo::getFixedStack(*MF, FI), MachineMemOperand::MOLoad,
      MFI.getObjectSize(FI), Alignment);

  BuildMI(MBB, I, DL, get(Opcode), DstReg)
      .addFrameIndex(FI)
      .addImm(0)
      .addMemOperand(MMO)
      .setMIFlag(Flags);
}
std::optional<unsigned> getFoldedOpcode(MachineFunction &MF, MachineInstr &MI,
                                        ArrayRef<unsigned> Ops,
                                        const YSXSubtarget &ST) {

  // The below optimizations narrow the load so they are only valid for little
  // endian.
  // TODO: Support big endian by adding an offset into the frame object?
  if (MF.getDataLayout().isBigEndian())
    return std::nullopt;

  // Fold load from stack followed by retained integer extension pseudos.
  if (Ops.size() != 1 || Ops[0] != 1)
    return std::nullopt;

  switch (MI.getOpcode()) {
  default:
    if (YSXInstrInfo::isSEXT_W(MI))
      return YSX::LW;
    if (YSXInstrInfo::isZEXT_W(MI))
      return YSX::LWU;
    if (YSXInstrInfo::isZEXT_B(MI))
      return YSX::LBU;
    break;
  }

  return std::nullopt;
}

// This is the version used during InlineSpiller::spillAroundUses
MachineInstr *YSXInstrInfo::foldMemoryOperandImpl(
    MachineFunction &MF, MachineInstr &MI, ArrayRef<unsigned> Ops,
    MachineBasicBlock::iterator InsertPt, int FrameIndex, LiveIntervals *LIS,
    VirtRegMap *VRM) const {

  std::optional<unsigned> LoadOpc = getFoldedOpcode(MF, MI, Ops, STI);
  if (!LoadOpc)
    return nullptr;
  Register DstReg = MI.getOperand(0).getReg();
  return BuildMI(*MI.getParent(), InsertPt, MI.getDebugLoc(), get(*LoadOpc),
                 DstReg)
      .addFrameIndex(FrameIndex)
      .addImm(0);
}

MachineInstr *YSXInstrInfo::foldMemoryOperandImpl(
    MachineFunction &MF, MachineInstr &MI, ArrayRef<unsigned> Ops,
    MachineBasicBlock::iterator InsertPt, MachineInstr &LoadMI,
    LiveIntervals *LIS) const {
  return nullptr;
#if 0
  // For now, only handle YSX::PseudoCCMOVGPR.
  if (MI.getOpcode() != YSX::PseudoCCMOVGPR)
    return nullptr;

  unsigned PredOpc = getLoadPredicatedOpcode(LoadMI.getOpcode());

  if (!STI.hasShortForwardBranchILoad() || !PredOpc)
    return nullptr;

  MachineRegisterInfo &MRI = MF.getRegInfo();
  if (Ops.size() != 1 || (Ops[0] != 4 && Ops[0] != 5))
    return nullptr;

  bool Invert = Ops[0] == 5;
  const MachineOperand &FalseReg = MI.getOperand(!Invert ? 5 : 4);
  Register DestReg = MI.getOperand(0).getReg();
  const TargetRegisterClass *PreviousClass = MRI.getRegClass(FalseReg.getReg());
  if (!MRI.constrainRegClass(DestReg, PreviousClass))
    return nullptr;

  // Create a new predicated version of DefMI.
  MachineInstrBuilder NewMI = BuildMI(*MI.getParent(), InsertPt,
                                      MI.getDebugLoc(), get(PredOpc), DestReg)
                                  .add({MI.getOperand(1), MI.getOperand(2)});

  // Add condition code, inverting if necessary.
  auto CC = static_cast<YSXCC::CondCode>(MI.getOperand(3).getImm());
  if (!Invert)
    CC = YSXCC::getInverseBranchCondition(CC);
  NewMI.addImm(CC);

  // Copy the false register.
  NewMI.add(FalseReg);

  // Copy all the DefMI operands.
  const MCInstrDesc &DefDesc = LoadMI.getDesc();
  for (unsigned i = 1, e = DefDesc.getNumOperands(); i != e; ++i)
    NewMI.add(LoadMI.getOperand(i));

  NewMI.cloneMemRefs(LoadMI);
  return NewMI;
#endif
}

void YSXInstrInfo::movImm(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI,
                            const DebugLoc &DL, Register DstReg, uint64_t Val,
                            MachineInstr::MIFlag Flag, bool DstRenamable,
                            bool DstIsDead) const {
  Register SrcReg = YSX::X0;

  // For RV32, allow a sign or unsigned 32 bit value.
  if (!STI.is64Bit() && !isInt<32>(Val)) {
    // If have a uimm32 it will still fit in a register so we can allow it.
    if (!isUInt<32>(Val))
      report_fatal_error("Should only materialize 32-bit constants for RV32");

    // Sign extend for generateInstSeq.
    Val = SignExtend64<32>(Val);
  }

  YSXMatInt::InstSeq Seq = YSXMatInt::generateInstSeq(Val, STI);
  assert(!Seq.empty());

  bool SrcRenamable = false;
  unsigned Num = 0;

  for (const YSXMatInt::Inst &Inst : Seq) {
    bool LastItem = ++Num == Seq.size();
    unsigned DstRegState = getDeadRegState(DstIsDead && LastItem) |
                           getRenamableRegState(DstRenamable);
    unsigned SrcRegState = getKillRegState(SrcReg != YSX::X0) |
                           getRenamableRegState(SrcRenamable);
    switch (Inst.getOpndKind()) {
    case YSXMatInt::Imm:
      BuildMI(MBB, MBBI, DL, get(Inst.getOpcode()))
          .addReg(DstReg, RegState::Define | DstRegState)
          .addImm(Inst.getImm())
          .setMIFlag(Flag);
      break;
    case YSXMatInt::RegX0:
      BuildMI(MBB, MBBI, DL, get(Inst.getOpcode()))
          .addReg(DstReg, RegState::Define | DstRegState)
          .addReg(SrcReg, SrcRegState)
          .addReg(YSX::X0)
          .setMIFlag(Flag);
      break;
    case YSXMatInt::RegReg:
      BuildMI(MBB, MBBI, DL, get(Inst.getOpcode()))
          .addReg(DstReg, RegState::Define | DstRegState)
          .addReg(SrcReg, SrcRegState)
          .addReg(SrcReg, SrcRegState)
          .setMIFlag(Flag);
      break;
    case YSXMatInt::RegImm:
      BuildMI(MBB, MBBI, DL, get(Inst.getOpcode()))
          .addReg(DstReg, RegState::Define | DstRegState)
          .addReg(SrcReg, SrcRegState)
          .addImm(Inst.getImm())
          .setMIFlag(Flag);
      break;
    }

    // Only the first instruction has X0 as its source.
    SrcReg = DstReg;
    SrcRenamable = DstRenamable;
  }
}

YSXCC::CondCode YSXInstrInfo::getCondFromBranchOpc(unsigned Opc) {
  switch (Opc) {
  default:
    return YSXCC::COND_INVALID;
  case YSX::BEQ:
    return YSXCC::COND_EQ;
  case YSX::BNE:
    return YSXCC::COND_NE;
  case YSX::BLT:
    return YSXCC::COND_LT;
  case YSX::BGE:
    return YSXCC::COND_GE;
  case YSX::BLTU:
    return YSXCC::COND_LTU;
  case YSX::BGEU:
    return YSXCC::COND_GEU;
  }
}

bool YSXInstrInfo::evaluateCondBranch(YSXCC::CondCode CC, int64_t C0,
                                        int64_t C1) {
  switch (CC) {
  default:
    llvm_unreachable("Unexpected CC");
  case YSXCC::COND_EQ:
    return C0 == C1;
  case YSXCC::COND_NE:
    return C0 != C1;
  case YSXCC::COND_LT:
    return C0 < C1;
  case YSXCC::COND_GE:
    return C0 >= C1;
  case YSXCC::COND_LTU:
    return (uint64_t)C0 < (uint64_t)C1;
  case YSXCC::COND_GEU:
    return (uint64_t)C0 >= (uint64_t)C1;
  }
}

// The contents of values added to Cond are not examined outside of
// YSXInstrInfo, giving us flexibility in what to push to it. For YSX, we
// push BranchOpcode, Reg1, Reg2.
static void parseCondBranch(MachineInstr &LastInst, MachineBasicBlock *&Target,
                            SmallVectorImpl<MachineOperand> &Cond) {
  // Block ends with fall-through condbranch.
  assert(LastInst.getDesc().isConditionalBranch() &&
         "Unknown conditional branch");
  Target = LastInst.getOperand(2).getMBB();
  Cond.push_back(MachineOperand::CreateImm(LastInst.getOpcode()));
  Cond.push_back(LastInst.getOperand(0));
  Cond.push_back(LastInst.getOperand(1));
}

#if 0
static unsigned getInverseXRemovedQcicmOpcode(unsigned Opcode) {
  llvm_unreachable("YSX does not support XRemovedQci conditional moves");
#if 0
  switch (Opcode) {
  default:
    llvm_unreachable("Unexpected Opcode");
  case YSX::QC_MVEQ:
    return YSX::QC_MVNE;
  case YSX::QC_MVNE:
    return YSX::QC_MVEQ;
  case YSX::QC_MVLT:
    return YSX::QC_MVGE;
  case YSX::QC_MVGE:
    return YSX::QC_MVLT;
  case YSX::QC_MVLTU:
    return YSX::QC_MVGEU;
  case YSX::QC_MVGEU:
    return YSX::QC_MVLTU;
  case YSX::QC_MVEQI:
    return YSX::QC_MVNEI;
  case YSX::QC_MVNEI:
    return YSX::QC_MVEQI;
  case YSX::QC_MVLTI:
    return YSX::QC_MVGEI;
  case YSX::QC_MVGEI:
    return YSX::QC_MVLTI;
  case YSX::QC_MVLTUI:
    return YSX::QC_MVGEUI;
  case YSX::QC_MVGEUI:
    return YSX::QC_MVLTUI;
  }
#endif
}
#endif

unsigned YSXCC::getBrCond(YSXCC::CondCode CC, unsigned SelectOpc) {
  switch (SelectOpc) {
  default:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_EQ:
      return YSX::BEQ;
    case YSXCC::COND_NE:
      return YSX::BNE;
    case YSXCC::COND_LT:
      return YSX::BLT;
    case YSXCC::COND_GE:
      return YSX::BGE;
    case YSXCC::COND_LTU:
      return YSX::BLTU;
    case YSXCC::COND_GEU:
      return YSX::BGEU;
    }
    break;
#if 0
  case YSX::Select_GPR_Using_CC_Imm5_Zibi:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_EQ:
      return YSX::BEQI;
    case YSXCC::COND_NE:
      return YSX::BNEI;
    }
    break;
  case YSX::Select_GPR_Using_CC_SImm5_CV:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_EQ:
      return YSX::CV_BEQIMM;
    case YSXCC::COND_NE:
      return YSX::CV_BNEIMM;
    }
    break;
  case YSX::Select_GPRNoX0_Using_CC_SImm5NonZero_QC:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_EQ:
      return YSX::QC_BEQI;
    case YSXCC::COND_NE:
      return YSX::QC_BNEI;
    case YSXCC::COND_LT:
      return YSX::QC_BLTI;
    case YSXCC::COND_GE:
      return YSX::QC_BGEI;
    }
    break;
  case YSX::Select_GPRNoX0_Using_CC_UImm5NonZero_QC:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_LTU:
      return YSX::QC_BLTUI;
    case YSXCC::COND_GEU:
      return YSX::QC_BGEUI;
    }
    break;
  case YSX::Select_GPRNoX0_Using_CC_SImm16NonZero_QC:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_EQ:
      return YSX::QC_E_BEQI;
    case YSXCC::COND_NE:
      return YSX::QC_E_BNEI;
    case YSXCC::COND_LT:
      return YSX::QC_E_BLTI;
    case YSXCC::COND_GE:
      return YSX::QC_E_BGEI;
    }
    break;
  case YSX::Select_GPRNoX0_Using_CC_UImm16NonZero_QC:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_LTU:
      return YSX::QC_E_BLTUI;
    case YSXCC::COND_GEU:
      return YSX::QC_E_BGEUI;
    }
    break;
  case YSX::Select_GPR_Using_CC_UImmLog2XLen_NDS:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_EQ:
      return YSX::NDS_BBC;
    case YSXCC::COND_NE:
      return YSX::NDS_BBS;
    }
    break;
  case YSX::Select_GPR_Using_CC_UImm7_NDS:
    switch (CC) {
    default:
      llvm_unreachable("Unexpected condition code!");
    case YSXCC::COND_EQ:
      return YSX::NDS_BEQC;
    case YSXCC::COND_NE:
      return YSX::NDS_BNEC;
    }
    break;
#endif
  }
}

YSXCC::CondCode YSXCC::getInverseBranchCondition(YSXCC::CondCode CC) {
  switch (CC) {
  default:
    llvm_unreachable("Unrecognized conditional branch");
  case YSXCC::COND_EQ:
    return YSXCC::COND_NE;
  case YSXCC::COND_NE:
    return YSXCC::COND_EQ;
  case YSXCC::COND_LT:
    return YSXCC::COND_GE;
  case YSXCC::COND_GE:
    return YSXCC::COND_LT;
  case YSXCC::COND_LTU:
    return YSXCC::COND_GEU;
  case YSXCC::COND_GEU:
    return YSXCC::COND_LTU;
  }
}

bool YSXInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                   MachineBasicBlock *&TBB,
                                   MachineBasicBlock *&FBB,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   bool AllowModify) const {
  TBB = FBB = nullptr;
  Cond.clear();

  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end() || !isUnpredicatedTerminator(*I))
    return false;

  // Count the number of terminators and find the first unconditional or
  // indirect branch.
  MachineBasicBlock::iterator FirstUncondOrIndirectBr = MBB.end();
  int NumTerminators = 0;
  for (auto J = I.getReverse(); J != MBB.rend() && isUnpredicatedTerminator(*J);
       J++) {
    NumTerminators++;
    if (J->getDesc().isUnconditionalBranch() ||
        J->getDesc().isIndirectBranch()) {
      FirstUncondOrIndirectBr = J.getReverse();
    }
  }

  // If AllowModify is true, we can erase any terminators after
  // FirstUncondOrIndirectBR.
  if (AllowModify && FirstUncondOrIndirectBr != MBB.end()) {
    while (std::next(FirstUncondOrIndirectBr) != MBB.end()) {
      std::next(FirstUncondOrIndirectBr)->eraseFromParent();
      NumTerminators--;
    }
    I = FirstUncondOrIndirectBr;
  }

  // We can't handle blocks that end in an indirect branch.
  if (I->getDesc().isIndirectBranch())
    return true;

  // We can't handle Generic branch opcodes from Global ISel.
  if (I->isPreISelOpcode())
    return true;

  // We can't handle blocks with more than 2 terminators.
  if (NumTerminators > 2)
    return true;

  // Handle a single unconditional branch.
  if (NumTerminators == 1 && I->getDesc().isUnconditionalBranch()) {
    TBB = getBranchDestBlock(*I);
    return false;
  }

  // Handle a single conditional branch.
  if (NumTerminators == 1 && I->getDesc().isConditionalBranch()) {
    parseCondBranch(*I, TBB, Cond);
    return false;
  }

  // Handle a conditional branch followed by an unconditional branch.
  if (NumTerminators == 2 && std::prev(I)->getDesc().isConditionalBranch() &&
      I->getDesc().isUnconditionalBranch()) {
    parseCondBranch(*std::prev(I), TBB, Cond);
    FBB = getBranchDestBlock(*I);
    return false;
  }

  // Otherwise, we can't handle this.
  return true;
}

unsigned YSXInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                      int *BytesRemoved) const {
  if (BytesRemoved)
    *BytesRemoved = 0;
  MachineBasicBlock::iterator I = MBB.getLastNonDebugInstr();
  if (I == MBB.end())
    return 0;

  if (!I->getDesc().isUnconditionalBranch() &&
      !I->getDesc().isConditionalBranch())
    return 0;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();

  I = MBB.end();

  if (I == MBB.begin())
    return 1;
  --I;
  if (!I->getDesc().isConditionalBranch())
    return 1;

  // Remove the branch.
  if (BytesRemoved)
    *BytesRemoved += getInstSizeInBytes(*I);
  I->eraseFromParent();
  return 2;
}

// Inserts a branch into the end of the specific MachineBasicBlock, returning
// the number of instructions inserted.
unsigned YSXInstrInfo::insertBranch(
    MachineBasicBlock &MBB, MachineBasicBlock *TBB, MachineBasicBlock *FBB,
    ArrayRef<MachineOperand> Cond, const DebugLoc &DL, int *BytesAdded) const {
  if (BytesAdded)
    *BytesAdded = 0;

  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 3 || Cond.size() == 0) &&
         "RISC-V branch conditions have two components!");

  // Unconditional branch.
  if (Cond.empty()) {
    MachineInstr &MI = *BuildMI(&MBB, DL, get(YSX::PseudoBR)).addMBB(TBB);
    if (BytesAdded)
      *BytesAdded += getInstSizeInBytes(MI);
    return 1;
  }

  // Either a one or two-way conditional branch.
  MachineInstr &CondMI = *BuildMI(&MBB, DL, get(Cond[0].getImm()))
                              .add(Cond[1])
                              .add(Cond[2])
                              .addMBB(TBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(CondMI);

  // One-way conditional branch.
  if (!FBB)
    return 1;

  // Two-way conditional branch.
  MachineInstr &MI = *BuildMI(&MBB, DL, get(YSX::PseudoBR)).addMBB(FBB);
  if (BytesAdded)
    *BytesAdded += getInstSizeInBytes(MI);
  return 2;
}

void YSXInstrInfo::insertIndirectBranch(MachineBasicBlock &MBB,
                                          MachineBasicBlock &DestBB,
                                          MachineBasicBlock &RestoreBB,
                                          const DebugLoc &DL, int64_t BrOffset,
                                          RegScavenger *RS) const {
  assert(RS && "RegScavenger required for long branching");
  assert(MBB.empty() &&
         "new block should be inserted for expanding unconditional branch");
  assert(MBB.pred_size() == 1);
  assert(RestoreBB.empty() &&
         "restore block should be inserted for restoring clobbered registers");

  MachineFunction *MF = MBB.getParent();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  YSXMachineFunctionInfo *RVFI = MF->getInfo<YSXMachineFunctionInfo>();
  const TargetRegisterInfo *TRI = MF->getSubtarget().getRegisterInfo();

  if (!isInt<32>(BrOffset))
    report_fatal_error(
        "Branch offsets outside of the signed 32-bit range not supported");

  // FIXME: A virtual register must be used initially, as the register
  // scavenger won't work with empty blocks (SIInstrInfo::insertIndirectBranch
  // uses the same workaround).
  Register ScratchReg = MRI.createVirtualRegister(&YSX::GPRJALRRegClass);
  auto II = MBB.end();
  // We may also update the jump target to RestoreBB later.
  MachineInstr &MI = *BuildMI(MBB, II, DL, get(YSX::PseudoJump))
                          .addReg(ScratchReg, RegState::Define | RegState::Dead)
                          .addMBB(&DestBB, YSXII::MO_CALL);

  RS->enterBasicBlockEnd(MBB);
  const TargetRegisterClass *RC = &YSX::GPRRegClass;
  if (STI.hasStdExtZicfilp())
    RC = &YSX::GPRX7RegClass;
  Register TmpGPR =
      RS->scavengeRegisterBackwards(*RC, MI.getIterator(),
                                    /*RestoreAfter=*/false, /*SpAdj=*/0,
                                    /*AllowSpill=*/false);
  if (TmpGPR.isValid())
    RS->setRegUsed(TmpGPR);
  else {
    // The case when there is no scavenged register needs special handling.

    // Pick s11(or s1 for rve) because it doesn't make a difference.
    TmpGPR = STI.hasStdExtE() ? YSX::X9 : YSX::X27;
    // Force t2 if Zicfilp is on
    if (STI.hasStdExtZicfilp())
      TmpGPR = YSX::X7;

    int FrameIndex = RVFI->getBranchRelaxationScratchFrameIndex();
    if (FrameIndex == -1)
      report_fatal_error("underestimated function size");

    storeRegToStackSlot(MBB, MI, TmpGPR, /*IsKill=*/true, FrameIndex,
                        &YSX::GPRRegClass, Register());
    TRI->eliminateFrameIndex(std::prev(MI.getIterator()),
                             /*SpAdj=*/0, /*FIOperandNum=*/1);

    MI.getOperand(1).setMBB(&RestoreBB);

    loadRegFromStackSlot(RestoreBB, RestoreBB.end(), TmpGPR, FrameIndex,
                         &YSX::GPRRegClass, Register());
    TRI->eliminateFrameIndex(RestoreBB.back(),
                             /*SpAdj=*/0, /*FIOperandNum=*/1);
  }

  MRI.replaceRegWith(ScratchReg, TmpGPR);
  MRI.clearVirtRegs();
}

bool YSXInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert((Cond.size() == 3) && "Invalid branch condition!");
  switch (Cond[0].getImm()) {
  default:
    llvm_unreachable("Unknown conditional branch!");
  case YSX::BEQ:
    Cond[0].setImm(YSX::BNE);
    break;
#if 0
  case YSX::BEQI:
    Cond[0].setImm(YSX::BNEI);
    break;
#endif
  case YSX::BNE:
    Cond[0].setImm(YSX::BEQ);
    break;
#if 0
  case YSX::BNEI:
    Cond[0].setImm(YSX::BEQI);
    break;
#endif
  case YSX::BLT:
    Cond[0].setImm(YSX::BGE);
    break;
  case YSX::BGE:
    Cond[0].setImm(YSX::BLT);
    break;
  case YSX::BLTU:
    Cond[0].setImm(YSX::BGEU);
    break;
  case YSX::BGEU:
    Cond[0].setImm(YSX::BLTU);
    break;
#if 0
  case YSX::CV_BEQIMM:
    Cond[0].setImm(YSX::CV_BNEIMM);
    break;
  case YSX::CV_BNEIMM:
    Cond[0].setImm(YSX::CV_BEQIMM);
    break;
  case YSX::QC_BEQI:
    Cond[0].setImm(YSX::QC_BNEI);
    break;
  case YSX::QC_BNEI:
    Cond[0].setImm(YSX::QC_BEQI);
    break;
  case YSX::QC_BGEI:
    Cond[0].setImm(YSX::QC_BLTI);
    break;
  case YSX::QC_BLTI:
    Cond[0].setImm(YSX::QC_BGEI);
    break;
  case YSX::QC_BGEUI:
    Cond[0].setImm(YSX::QC_BLTUI);
    break;
  case YSX::QC_BLTUI:
    Cond[0].setImm(YSX::QC_BGEUI);
    break;
  case YSX::QC_E_BEQI:
    Cond[0].setImm(YSX::QC_E_BNEI);
    break;
  case YSX::QC_E_BNEI:
    Cond[0].setImm(YSX::QC_E_BEQI);
    break;
  case YSX::QC_E_BGEI:
    Cond[0].setImm(YSX::QC_E_BLTI);
    break;
  case YSX::QC_E_BLTI:
    Cond[0].setImm(YSX::QC_E_BGEI);
    break;
  case YSX::QC_E_BGEUI:
    Cond[0].setImm(YSX::QC_E_BLTUI);
    break;
  case YSX::QC_E_BLTUI:
    Cond[0].setImm(YSX::QC_E_BGEUI);
    break;
  case YSX::NDS_BBC:
    Cond[0].setImm(YSX::NDS_BBS);
    break;
  case YSX::NDS_BBS:
    Cond[0].setImm(YSX::NDS_BBC);
    break;
  case YSX::NDS_BEQC:
    Cond[0].setImm(YSX::NDS_BNEC);
    break;
  case YSX::NDS_BNEC:
    Cond[0].setImm(YSX::NDS_BEQC);
    break;
#endif
  }

  return false;
}

// Return true if the instruction is a load immediate instruction (i.e.
// ADDI x0, imm).
static bool isLoadImm(const MachineInstr *MI, int64_t &Imm) {
  if (MI->getOpcode() == YSX::ADDI && MI->getOperand(1).isReg() &&
      MI->getOperand(1).getReg() == YSX::X0) {
    Imm = MI->getOperand(2).getImm();
    return true;
  }
  return false;
}

bool YSXInstrInfo::isFromLoadImm(const MachineRegisterInfo &MRI,
                                   const MachineOperand &Op, int64_t &Imm) {
  // Either a load from immediate instruction or X0.
  if (!Op.isReg())
    return false;

  Register Reg = Op.getReg();
  if (Reg == YSX::X0) {
    Imm = 0;
    return true;
  }
  return Reg.isVirtual() && isLoadImm(MRI.getVRegDef(Reg), Imm);
}

bool YSXInstrInfo::optimizeCondBranch(MachineInstr &MI) const {
  bool IsSigned = false;
  bool IsEquality = false;
  switch (MI.getOpcode()) {
  default:
    return false;
  case YSX::BEQ:
  case YSX::BNE:
    IsEquality = true;
    break;
  case YSX::BGE:
  case YSX::BLT:
    IsSigned = true;
    break;
  case YSX::BGEU:
  case YSX::BLTU:
    break;
  }

  MachineBasicBlock *MBB = MI.getParent();
  MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();

  const MachineOperand &LHS = MI.getOperand(0);
  const MachineOperand &RHS = MI.getOperand(1);
  MachineBasicBlock *TBB = MI.getOperand(2).getMBB();

  YSXCC::CondCode CC = getCondFromBranchOpc(MI.getOpcode());
  assert(CC != YSXCC::COND_INVALID);

  // Canonicalize conditional branches which can be constant folded into
  // beqz or bnez.  We can't modify the CFG here.
  int64_t C0, C1;
  if (isFromLoadImm(MRI, LHS, C0) && isFromLoadImm(MRI, RHS, C1)) {
    unsigned NewOpc = evaluateCondBranch(CC, C0, C1) ? YSX::BEQ : YSX::BNE;
    // Build the new branch and remove the old one.
    BuildMI(*MBB, MI, MI.getDebugLoc(), get(NewOpc))
        .addReg(YSX::X0)
        .addReg(YSX::X0)
        .addMBB(TBB);
    MI.eraseFromParent();
    return true;
  }

  if (IsEquality)
    return false;

  // For two constants C0 and C1 from
  // ```
  // li Y, C0
  // li Z, C1
  // ```
  // 1. if C1 = C0 + 1
  // we can turn:
  //  (a) blt Y, X -> bge X, Z
  //  (b) bge Y, X -> blt X, Z
  //
  // 2. if C1 = C0 - 1
  // we can turn:
  //  (a) blt X, Y -> bge Z, X
  //  (b) bge X, Y -> blt Z, X
  //
  // To make sure this optimization is really beneficial, we only
  // optimize for cases where Y had only one use (i.e. only used by the branch).
  // Try to find the register for constant Z; return
  // invalid register otherwise.
  auto searchConst = [&](int64_t C1) -> Register {
    MachineBasicBlock::reverse_iterator II(&MI), E = MBB->rend();
    auto DefC1 = std::find_if(++II, E, [&](const MachineInstr &I) -> bool {
      int64_t Imm;
      return isLoadImm(&I, Imm) && Imm == C1 &&
             I.getOperand(0).getReg().isVirtual();
    });
    if (DefC1 != E)
      return DefC1->getOperand(0).getReg();

    return Register();
  };

  unsigned NewOpc = YSXCC::getBrCond(getInverseBranchCondition(CC));

  // Might be case 1.
  // Don't change 0 to 1 since we can use x0.
  // For unsigned cases changing -1U to 0 would be incorrect.
  // The incorrect case for signed would be INT_MAX, but isFromLoadImm can't
  // return that.
  if (isFromLoadImm(MRI, LHS, C0) && C0 != 0 && LHS.getReg().isVirtual() &&
      MRI.hasOneUse(LHS.getReg()) && (IsSigned || C0 != -1)) {
    assert(isInt<12>(C0) && "Unexpected immediate");
    if (Register RegZ = searchConst(C0 + 1)) {
      BuildMI(*MBB, MI, MI.getDebugLoc(), get(NewOpc))
          .add(RHS)
          .addReg(RegZ)
          .addMBB(TBB);
      // We might extend the live range of Z, clear its kill flag to
      // account for this.
      MRI.clearKillFlags(RegZ);
      MI.eraseFromParent();
      return true;
    }
  }

  // Might be case 2.
  // For signed cases we don't want to change 0 since we can use x0.
  // For unsigned cases changing 0 to -1U would be incorrect.
  // The incorrect case for signed would be INT_MIN, but isFromLoadImm can't
  // return that.
  if (isFromLoadImm(MRI, RHS, C0) && C0 != 0 && RHS.getReg().isVirtual() &&
      MRI.hasOneUse(RHS.getReg())) {
    assert(isInt<12>(C0) && "Unexpected immediate");
    if (Register RegZ = searchConst(C0 - 1)) {
      BuildMI(*MBB, MI, MI.getDebugLoc(), get(NewOpc))
          .addReg(RegZ)
          .add(LHS)
          .addMBB(TBB);
      // We might extend the live range of Z, clear its kill flag to
      // account for this.
      MRI.clearKillFlags(RegZ);
      MI.eraseFromParent();
      return true;
    }
  }

  return false;
}

MachineBasicBlock *
YSXInstrInfo::getBranchDestBlock(const MachineInstr &MI) const {
  assert(MI.getDesc().isBranch() && "Unexpected opcode!");
  // The branch target is always the last operand.
  int NumOp = MI.getNumExplicitOperands();
  return MI.getOperand(NumOp - 1).getMBB();
}

bool YSXInstrInfo::isBranchOffsetInRange(unsigned BranchOp,
                                           int64_t BrOffset) const {
  unsigned XLen = STI.getXLen();
  // Ideally we could determine the supported branch offset from the
  // YSXII::FormMask, but this can't be used for Pseudo instructions like
  // PseudoBR.
  switch (BranchOp) {
  default:
    llvm_unreachable("Unexpected opcode!");
  case YSX::BEQ:
  case YSX::BNE:
  case YSX::BLT:
  case YSX::BGE:
  case YSX::BLTU:
  case YSX::BGEU:
    return isInt<13>(BrOffset);
  case YSX::JAL:
  case YSX::PseudoBR:
    return isInt<21>(BrOffset);
  case YSX::PseudoJump:
    return isInt<32>(SignExtend64(BrOffset + 0x800, XLen));
  }
}

// If the operation has a predicated pseudo instruction, return the pseudo
// instruction opcode. Otherwise, return YSX::INSTRUCTION_LIST_END.
// TODO: Support more operations.
static unsigned getPredicatedOpcode(unsigned Opcode) {
  return YSX::INSTRUCTION_LIST_END;
#if 0
  // clang-format off
  switch (Opcode) {
  case YSX::ADD:   return YSX::PseudoCCADD;
  case YSX::SUB:   return YSX::PseudoCCSUB;
  case YSX::SLL:   return YSX::PseudoCCSLL;
  case YSX::SRL:   return YSX::PseudoCCSRL;
  case YSX::SRA:   return YSX::PseudoCCSRA;
  case YSX::AND:   return YSX::PseudoCCAND;
  case YSX::OR:    return YSX::PseudoCCOR;
  case YSX::XOR:   return YSX::PseudoCCXOR;
  case YSX::MAX:   return YSX::PseudoCCMAX;
  case YSX::MAXU:  return YSX::PseudoCCMAXU;
  case YSX::MIN:   return YSX::PseudoCCMIN;
  case YSX::MINU:  return YSX::PseudoCCMINU;
  case YSX::MUL:   return YSX::PseudoCCMUL;
  case YSX::LUI:   return YSX::PseudoCCLUI;
  case YSX::QC_LI:   return YSX::PseudoCCQC_LI;
  case YSX::QC_E_LI:   return YSX::PseudoCCQC_E_LI;

  case YSX::ADDI:  return YSX::PseudoCCADDI;
  case YSX::SLLI:  return YSX::PseudoCCSLLI;
  case YSX::SRLI:  return YSX::PseudoCCSRLI;
  case YSX::SRAI:  return YSX::PseudoCCSRAI;
  case YSX::ANDI:  return YSX::PseudoCCANDI;
  case YSX::ORI:   return YSX::PseudoCCORI;
  case YSX::XORI:  return YSX::PseudoCCXORI;

  case YSX::ADDW:  return YSX::PseudoCCADDW;
  case YSX::SUBW:  return YSX::PseudoCCSUBW;
  case YSX::SLLW:  return YSX::PseudoCCSLLW;
  case YSX::SRLW:  return YSX::PseudoCCSRLW;
  case YSX::SRAW:  return YSX::PseudoCCSRAW;

  case YSX::ADDIW: return YSX::PseudoCCADDIW;
  case YSX::SLLIW: return YSX::PseudoCCSLLIW;
  case YSX::SRLIW: return YSX::PseudoCCSRLIW;
  case YSX::SRAIW: return YSX::PseudoCCSRAIW;

  case YSX::ANDN:  return YSX::PseudoCCANDN;
  case YSX::ORN:   return YSX::PseudoCCORN;
  case YSX::XNOR:  return YSX::PseudoCCXNOR;

  case YSX::NDS_BFOS:  return YSX::PseudoCCNDS_BFOS;
  case YSX::NDS_BFOZ:  return YSX::PseudoCCNDS_BFOZ;
  }
  // clang-format on

  return YSX::INSTRUCTION_LIST_END;
#endif
}

/// Identify instructions that can be folded into a CCMOV instruction, and
/// return the defining instruction.
static MachineInstr *canFoldAsPredicatedOp(Register Reg,
                                           const MachineRegisterInfo &MRI,
                                           const TargetInstrInfo *TII,
                                           const YSXSubtarget &STI) {
  if (!Reg.isVirtual())
    return nullptr;
  if (!MRI.hasOneNonDBGUse(Reg))
    return nullptr;
  MachineInstr *MI = MRI.getVRegDef(Reg);
  if (!MI)
    return nullptr;

  if (!STI.hasShortForwardBranchIMul() && MI->getOpcode() == YSX::MUL)
    return nullptr;

  // Check if MI can be predicated and folded into the CCMOV.
  if (getPredicatedOpcode(MI->getOpcode()) == YSX::INSTRUCTION_LIST_END)
    return nullptr;
  // Don't predicate li idiom.
  if (MI->getOpcode() == YSX::ADDI && MI->getOperand(1).isReg() &&
      MI->getOperand(1).getReg() == YSX::X0)
    return nullptr;
  // Check if MI has any other defs or physreg uses.
  for (const MachineOperand &MO : llvm::drop_begin(MI->operands())) {
    // Reject frame index operands, PEI can't handle the predicated pseudos.
    if (MO.isFI() || MO.isCPI() || MO.isJTI())
      return nullptr;
    if (!MO.isReg())
      continue;
    // MI can't have any tied operands, that would conflict with predication.
    if (MO.isTied())
      return nullptr;
    if (MO.isDef())
      return nullptr;
    // Allow constant physregs.
    if (MO.getReg().isPhysical() && !MRI.isConstantPhysReg(MO.getReg()))
      return nullptr;
  }
  bool DontMoveAcrossStores = true;
  if (!MI->isSafeToMove(DontMoveAcrossStores))
    return nullptr;
  return MI;
}

bool YSXInstrInfo::analyzeSelect(const MachineInstr &MI,
                                   SmallVectorImpl<MachineOperand> &Cond,
                                   unsigned &TrueOp, unsigned &FalseOp,
                                   bool &Optimizable) const {
  assert(MI.getOpcode() == YSX::PseudoCCMOVGPR &&
         "Unknown select instruction");
  // CCMOV operands:
  // 0: Def.
  // 1: LHS of compare.
  // 2: RHS of compare.
  // 3: Condition code.
  // 4: False use.
  // 5: True use.
  TrueOp = 5;
  FalseOp = 4;
  Cond.push_back(MI.getOperand(1));
  Cond.push_back(MI.getOperand(2));
  Cond.push_back(MI.getOperand(3));
  // We can only fold when we support short forward branch opt.
  Optimizable = STI.hasShortForwardBranchIALU();
  return false;
}

MachineInstr *
YSXInstrInfo::optimizeSelect(MachineInstr &MI,
                               SmallPtrSetImpl<MachineInstr *> &SeenMIs,
                               bool PreferFalse) const {
  assert(MI.getOpcode() == YSX::PseudoCCMOVGPR &&
         "Unknown select instruction");
  if (!STI.hasShortForwardBranchIALU())
    return nullptr;

  MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
  MachineInstr *DefMI =
      canFoldAsPredicatedOp(MI.getOperand(5).getReg(), MRI, this, STI);
  bool Invert = !DefMI;
  if (!DefMI)
    DefMI = canFoldAsPredicatedOp(MI.getOperand(4).getReg(), MRI, this, STI);
  if (!DefMI)
    return nullptr;

  // Find new register class to use.
  MachineOperand FalseReg = MI.getOperand(Invert ? 5 : 4);
  Register DestReg = MI.getOperand(0).getReg();
  const TargetRegisterClass *PreviousClass = MRI.getRegClass(FalseReg.getReg());
  if (!MRI.constrainRegClass(DestReg, PreviousClass))
    return nullptr;

  unsigned PredOpc = getPredicatedOpcode(DefMI->getOpcode());
  assert(PredOpc != YSX::INSTRUCTION_LIST_END && "Unexpected opcode!");

  // Create a new predicated version of DefMI.
  MachineInstrBuilder NewMI =
      BuildMI(*MI.getParent(), MI, MI.getDebugLoc(), get(PredOpc), DestReg);

  // Copy the condition portion.
  NewMI.add(MI.getOperand(1));
  NewMI.add(MI.getOperand(2));

  // Add condition code, inverting if necessary.
  auto CC = static_cast<YSXCC::CondCode>(MI.getOperand(3).getImm());
  if (Invert)
    CC = YSXCC::getInverseBranchCondition(CC);
  NewMI.addImm(CC);

  // Copy the false register.
  NewMI.add(FalseReg);

  // Copy all the DefMI operands.
  const MCInstrDesc &DefDesc = DefMI->getDesc();
  for (unsigned i = 1, e = DefDesc.getNumOperands(); i != e; ++i)
    NewMI.add(DefMI->getOperand(i));

  // Update SeenMIs set: register newly created MI and erase removed DefMI.
  SeenMIs.insert(NewMI);
  SeenMIs.erase(DefMI);

  // If MI is inside a loop, and DefMI is outside the loop, then kill flags on
  // DefMI would be invalid when transferred inside the loop.  Checking for a
  // loop is expensive, but at least remove kill flags if they are in different
  // BBs.
  if (DefMI->getParent() != MI.getParent())
    NewMI->clearKillInfo();

  // The caller will erase MI, but not DefMI.
  DefMI->eraseFromParent();
  return NewMI;
}

unsigned YSXInstrInfo::getInstSizeInBytes(const MachineInstr &MI) const {
  if (MI.isMetaInstruction())
    return 0;

  unsigned Opcode = MI.getOpcode();

  if (Opcode == TargetOpcode::INLINEASM ||
      Opcode == TargetOpcode::INLINEASM_BR) {
    const MachineFunction &MF = *MI.getParent()->getParent();
    return getInlineAsmLength(MI.getOperand(0).getSymbolName(),
                              *MF.getTarget().getMCAsmInfo());
  }

  if (!MI.memoperands_empty()) {
    MachineMemOperand *MMO = *(MI.memoperands_begin());
    if (STI.hasStdExtZihintntl() && MMO->isNonTemporal()) {
      if (STI.hasStdExtZca()) {
        if (isCompressibleInst(MI, STI))
          return 4; // c.ntl.all + c.load/c.store
        return 6;   // c.ntl.all + load/store
      }
      return 8; // ntl.all + load/store
    }
  }

  if (Opcode == TargetOpcode::BUNDLE)
    return getInstBundleLength(MI);

  if (MI.getParent() && MI.getParent()->getParent()) {
    if (isCompressibleInst(MI, STI))
      return 2;
  }

  switch (Opcode) {
  case TargetOpcode::STACKMAP:
    // The upper bound for a stackmap intrinsic is the full length of its shadow
    return StackMapOpers(&MI).getNumPatchBytes();
  case TargetOpcode::PATCHPOINT:
    // The size of the patchpoint intrinsic is the number of bytes requested
    return PatchPointOpers(&MI).getNumPatchBytes();
  case TargetOpcode::STATEPOINT: {
    // The size of the statepoint intrinsic is the number of bytes requested
    unsigned NumBytes = StatepointOpers(&MI).getNumPatchBytes();
    // No patch bytes means at most a PseudoCall is emitted
    return std::max(NumBytes, 8U);
  }
  case TargetOpcode::PATCHABLE_FUNCTION_ENTER:
  case TargetOpcode::PATCHABLE_FUNCTION_EXIT:
  case TargetOpcode::PATCHABLE_TAIL_CALL: {
    const MachineFunction &MF = *MI.getParent()->getParent();
    const Function &F = MF.getFunction();
    if (Opcode == TargetOpcode::PATCHABLE_FUNCTION_ENTER &&
        F.hasFnAttribute("patchable-function-entry")) {
      unsigned Num;
      if (F.getFnAttribute("patchable-function-entry")
              .getValueAsString()
              .getAsInteger(10, Num))
        return get(Opcode).getSize();

      // Number of C.NOP or NOP
      return (STI.hasStdExtZca() ? 2 : 4) * Num;
    }
    // XRay uses C.JAL + 21 or 33 C.NOP for each sled in RV32 and RV64,
    // respectively.
    return STI.is64Bit() ? 68 : 44;
  }
  default:
    return get(Opcode).getSize();
  }
}

unsigned YSXInstrInfo::getInstBundleLength(const MachineInstr &MI) const {
  unsigned Size = 0;
  MachineBasicBlock::const_instr_iterator I = MI.getIterator();
  MachineBasicBlock::const_instr_iterator E = MI.getParent()->instr_end();
  while (++I != E && I->isInsideBundle()) {
    assert(!I->isBundle() && "No nested bundle!");
    Size += getInstSizeInBytes(*I);
  }
  return Size;
}

bool YSXInstrInfo::isAsCheapAsAMove(const MachineInstr &MI) const {
  const unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default:
    break;
  case YSX::ADDI:
  case YSX::ORI:
  case YSX::XORI:
    return (MI.getOperand(1).isReg() &&
            MI.getOperand(1).getReg() == YSX::X0) ||
           (MI.getOperand(2).isImm() && MI.getOperand(2).getImm() == 0);
  }
  return MI.isAsCheapAsAMove();
}

std::optional<DestSourcePair>
YSXInstrInfo::isCopyInstrImpl(const MachineInstr &MI) const {
  if (MI.isMoveReg())
    return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
  switch (MI.getOpcode()) {
  default:
    break;
  case YSX::ADD:
  case YSX::OR:
  case YSX::XOR:
    if (MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == YSX::X0 &&
        MI.getOperand(2).isReg())
      return DestSourcePair{MI.getOperand(0), MI.getOperand(2)};
    if (MI.getOperand(2).isReg() && MI.getOperand(2).getReg() == YSX::X0 &&
        MI.getOperand(1).isReg())
      return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
    break;
  case YSX::ADDI:
    // Operand 1 can be a frameindex but callers expect registers
    if (MI.getOperand(1).isReg() && MI.getOperand(2).isImm() &&
        MI.getOperand(2).getImm() == 0)
      return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
    break;
  case YSX::SUB:
    if (MI.getOperand(2).isReg() && MI.getOperand(2).getReg() == YSX::X0 &&
        MI.getOperand(1).isReg())
      return DestSourcePair{MI.getOperand(0), MI.getOperand(1)};
    break;
  }
  return std::nullopt;
}

MachineTraceStrategy YSXInstrInfo::getMachineCombinerTraceStrategy() const {
  if (ForceMachineCombinerStrategy.getNumOccurrences() == 0) {
    // The option is unused. Choose Local strategy only for in-order cores. When
    // scheduling model is unspecified, use MinInstrCount strategy as more
    // generic one.
    const auto &SchedModel = STI.getSchedModel();
    return (!SchedModel.hasInstrSchedModel() || SchedModel.isOutOfOrder())
               ? MachineTraceStrategy::TS_MinInstrCount
               : MachineTraceStrategy::TS_Local;
  }
  // The strategy was forced by the option.
  return ForceMachineCombinerStrategy;
}

void YSXInstrInfo::finalizeInsInstrs(
    MachineInstr &Root, unsigned &Pattern,
    SmallVectorImpl<MachineInstr *> &InsInstrs) const {
#if 0
  int16_t FrmOpIdx =
      YSX::getNamedOperandIdx(Root.getOpcode(), YSX::OpName::frm);
  if (FrmOpIdx < 0) {
    assert(all_of(InsInstrs,
                  [](MachineInstr *MI) {
                    return YSX::getNamedOperandIdx(MI->getOpcode(),
                                                     YSX::OpName::frm) < 0;
                  }) &&
           "New instructions require FRM whereas the old one does not have it");
    return;
  }

  const MachineOperand &FRM = Root.getOperand(FrmOpIdx);
  MachineFunction &MF = *Root.getMF();

  for (auto *NewMI : InsInstrs) {
    // We'd already added the FRM operand.
    if (static_cast<unsigned>(YSX::getNamedOperandIdx(
            NewMI->getOpcode(), YSX::OpName::frm)) != NewMI->getNumOperands())
      continue;
    MachineInstrBuilder MIB(MF, NewMI);
    MIB.add(FRM);
    if (FRM.getImm() == YSXFPRndMode::DYN)
      MIB.addUse(YSX::FRM, RegState::Implicit);
  }
#endif
}

static bool isFADD(unsigned Opc) {
#if 0
  switch (Opc) {
  default:
    return false;
  case YSX::FADD_H:
  case YSX::FADD_S:
  case YSX::FADD_D:
    return true;
  }
#endif
  return false;
}

#if 0
static bool isFSUB(unsigned Opc) {
#if 0
  switch (Opc) {
  default:
    return false;
  case YSX::FSUB_H:
  case YSX::FSUB_S:
  case YSX::FSUB_D:
    return true;
  }
#endif
  return false;
}
#endif

static bool isFMUL(unsigned Opc) {
#if 0
  switch (Opc) {
  default:
    return false;
  case YSX::FMUL_H:
  case YSX::FMUL_S:
  case YSX::FMUL_D:
    return true;
  }
#endif
  return false;
}

bool YSXInstrInfo::isVectorAssociativeAndCommutative(const MachineInstr &Inst,
                                                       bool Invert) const {
#if 0
#define OPCODE_LMUL_CASE(OPC)                                                  \
  case YSX::OPC##_M1:                                                        \
  case YSX::OPC##_M2:                                                        \
  case YSX::OPC##_M4:                                                        \
  case YSX::OPC##_M8:                                                        \
  case YSX::OPC##_MF2:                                                       \
  case YSX::OPC##_MF4:                                                       \
  case YSX::OPC##_MF8

#define OPCODE_LMUL_MASK_CASE(OPC)                                             \
  case YSX::OPC##_M1_MASK:                                                   \
  case YSX::OPC##_M2_MASK:                                                   \
  case YSX::OPC##_M4_MASK:                                                   \
  case YSX::OPC##_M8_MASK:                                                   \
  case YSX::OPC##_MF2_MASK:                                                  \
  case YSX::OPC##_MF4_MASK:                                                  \
  case YSX::OPC##_MF8_MASK

  unsigned Opcode = Inst.getOpcode();
  if (Invert) {
    if (auto InvOpcode = getInverseOpcode(Opcode))
      Opcode = *InvOpcode;
    else
      return false;
  }

  // clang-format off
  switch (Opcode) {
  default:
    return false;
  OPCODE_LMUL_CASE(PseudoVADD_VV):
  OPCODE_LMUL_MASK_CASE(PseudoVADD_VV):
  OPCODE_LMUL_CASE(PseudoVMUL_VV):
  OPCODE_LMUL_MASK_CASE(PseudoVMUL_VV):
    return true;
  }
  // clang-format on

#undef OPCODE_LMUL_MASK_CASE
#undef OPCODE_LMUL_CASE
#endif
  return false;
}

bool YSXInstrInfo::areYSXVecInstsReassociable(const MachineInstr &Root,
                                             const MachineInstr &Prev) const {
  return false;
#if 0
  if (!areOpcodesEqualOrInverse(Root.getOpcode(), Prev.getOpcode()))
    return false;

  assert(Root.getMF() == Prev.getMF());
  const MachineRegisterInfo *MRI = &Root.getMF()->getRegInfo();
  const TargetRegisterInfo *TRI = MRI->getTargetRegisterInfo();

  // Make sure vtype operands are also the same.
  const MCInstrDesc &Desc = get(Root.getOpcode());
  const uint64_t TSFlags = Desc.TSFlags;

  auto checkImmOperand = [&](unsigned OpIdx) {
    return Root.getOperand(OpIdx).getImm() == Prev.getOperand(OpIdx).getImm();
  };

  auto checkRegOperand = [&](unsigned OpIdx) {
    return Root.getOperand(OpIdx).getReg() == Prev.getOperand(OpIdx).getReg();
  };

  // PassThru
  // TODO: Potentially we can loosen the condition to consider Root to be
  // associable with Prev if Root has NoReg as passthru. In which case we
  // also need to loosen the condition on vector policies between these.
  if (!checkRegOperand(1))
    return false;

  // SEW
  if (YSXII::hasSEWOp(TSFlags) &&
      !checkImmOperand(YSXII::getSEWOpNum(Desc)))
    return false;

  // Mask
  if (YSXII::usesMaskPolicy(TSFlags)) {
    const MachineBasicBlock *MBB = Root.getParent();
    const MachineBasicBlock::const_reverse_iterator It1(&Root);
    const MachineBasicBlock::const_reverse_iterator It2(&Prev);
    Register MI1VReg;

    bool SeenMI2 = false;
    for (auto End = MBB->rend(), It = It1; It != End; ++It) {
      if (It == It2) {
        SeenMI2 = true;
        if (!MI1VReg.isValid())
          // There is no V0 def between Root and Prev; they're sharing the
          // same V0.
          break;
      }

      if (It->modifiesRegister(YSX::V0, TRI)) {
        Register SrcReg = It->getOperand(1).getReg();
        // If it's not VReg it'll be more difficult to track its defs, so
        // bailing out here just to be safe.
        if (!SrcReg.isVirtual())
          return false;

        if (!MI1VReg.isValid()) {
          // This is the V0 def for Root.
          MI1VReg = SrcReg;
          continue;
        }

        // Some random mask updates.
        if (!SeenMI2)
          continue;

        // This is the V0 def for Prev; check if it's the same as that of
        // Root.
        if (MI1VReg != SrcReg)
          return false;
        else
          break;
      }
    }

    // If we haven't encountered Prev, it's likely that this function was
    // called in a wrong way (e.g. Root is before Prev).
    assert(SeenMI2 && "Prev is expected to appear before Root");
  }

  // Tail / Mask policies
  if (YSXII::hasVecPolicyOp(TSFlags) &&
      !checkImmOperand(YSXII::getVecPolicyOpNum(Desc)))
    return false;

  // VL
  if (YSXII::hasVLOp(TSFlags)) {
    unsigned OpIdx = YSXII::getVLOpNum(Desc);
    const MachineOperand &Op1 = Root.getOperand(OpIdx);
    const MachineOperand &Op2 = Prev.getOperand(OpIdx);
    if (Op1.getType() != Op2.getType())
      return false;
    switch (Op1.getType()) {
    case MachineOperand::MO_Register:
      if (Op1.getReg() != Op2.getReg())
        return false;
      break;
    case MachineOperand::MO_Immediate:
      if (Op1.getImm() != Op2.getImm())
        return false;
      break;
    default:
      llvm_unreachable("Unrecognized VL operand type");
    }
  }

  // Rounding modes
  if (YSXII::hasRoundModeOp(TSFlags) &&
      !checkImmOperand(YSXII::getVLOpNum(Desc) - 1))
    return false;

  return true;
#endif
}

// Most of our YSXVec pseudos have passthru operand, so the real operands
// start from index = 2.
bool YSXInstrInfo::hasReassociableVectorSibling(const MachineInstr &Inst,
                                                  bool &Commuted) const {
  const MachineBasicBlock *MBB = Inst.getParent();
  const MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
  assert(YSXII::isFirstDefTiedToFirstUse(get(Inst.getOpcode())) &&
         "Expect the present of passthrough operand.");
  MachineInstr *MI1 = MRI.getUniqueVRegDef(Inst.getOperand(2).getReg());
  MachineInstr *MI2 = MRI.getUniqueVRegDef(Inst.getOperand(3).getReg());

  // If only one operand has the same or inverse opcode and it's the second
  // source operand, the operands must be commuted.
  Commuted = !areYSXVecInstsReassociable(Inst, *MI1) &&
             areYSXVecInstsReassociable(Inst, *MI2);
  if (Commuted)
    std::swap(MI1, MI2);

  return areYSXVecInstsReassociable(Inst, *MI1) &&
         (isVectorAssociativeAndCommutative(*MI1) ||
          isVectorAssociativeAndCommutative(*MI1, /* Invert */ true)) &&
         hasReassociableOperands(*MI1, MBB) &&
         MRI.hasOneNonDBGUse(MI1->getOperand(0).getReg());
}

bool YSXInstrInfo::hasReassociableOperands(
    const MachineInstr &Inst, const MachineBasicBlock *MBB) const {
  if (!isVectorAssociativeAndCommutative(Inst) &&
      !isVectorAssociativeAndCommutative(Inst, /*Invert=*/true))
    return TargetInstrInfo::hasReassociableOperands(Inst, MBB);

  const MachineOperand &Op1 = Inst.getOperand(2);
  const MachineOperand &Op2 = Inst.getOperand(3);
  const MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();

  // We need virtual register definitions for the operands that we will
  // reassociate.
  MachineInstr *MI1 = nullptr;
  MachineInstr *MI2 = nullptr;
  if (Op1.isReg() && Op1.getReg().isVirtual())
    MI1 = MRI.getUniqueVRegDef(Op1.getReg());
  if (Op2.isReg() && Op2.getReg().isVirtual())
    MI2 = MRI.getUniqueVRegDef(Op2.getReg());

  // And at least one operand must be defined in MBB.
  return MI1 && MI2 && (MI1->getParent() == MBB || MI2->getParent() == MBB);
}

void YSXInstrInfo::getReassociateOperandIndices(
    const MachineInstr &Root, unsigned Pattern,
    std::array<unsigned, 5> &OperandIndices) const {
  TargetInstrInfo::getReassociateOperandIndices(Root, Pattern, OperandIndices);
  if (YSX::getYSXVecMCOpcode(Root.getOpcode())) {
    // Skip the passthrough operand, so increment all indices by one.
    for (unsigned I = 0; I < 5; ++I)
      ++OperandIndices[I];
  }
}

bool YSXInstrInfo::hasReassociableSibling(const MachineInstr &Inst,
                                            bool &Commuted) const {
  if (isVectorAssociativeAndCommutative(Inst) ||
      isVectorAssociativeAndCommutative(Inst, /*Invert=*/true))
    return hasReassociableVectorSibling(Inst, Commuted);

  return TargetInstrInfo::hasReassociableSibling(Inst, Commuted);
}

bool YSXInstrInfo::isAssociativeAndCommutative(const MachineInstr &Inst,
                                                 bool Invert) const {
  if (isVectorAssociativeAndCommutative(Inst, Invert))
    return true;

  unsigned Opc = Inst.getOpcode();
  if (Invert) {
    auto InverseOpcode = getInverseOpcode(Opc);
    if (!InverseOpcode)
      return false;
    Opc = *InverseOpcode;
  }

  if (isFADD(Opc) || isFMUL(Opc))
    return Inst.getFlag(MachineInstr::MIFlag::FmReassoc) &&
           Inst.getFlag(MachineInstr::MIFlag::FmNsz);

  switch (Opc) {
  default:
    return false;
  case YSX::ADD:
  case YSX::ADDW:
  case YSX::AND:
  case YSX::OR:
  case YSX::XOR:
  // From RISC-V ISA spec, if both the high and low bits of the same product
  // are required, then the recommended code sequence is:
  //
  // MULH[[S]U] rdh, rs1, rs2
  // MUL        rdl, rs1, rs2
  // (source register specifiers must be in same order and rdh cannot be the
  //  same as rs1 or rs2)
  //
  // Microarchitectures can then fuse these into a single multiply operation
  // instead of performing two separate multiplies.
  // MachineCombiner may reassociate MUL operands and lose the fusion
  // opportunity.
  case YSX::MUL:
  case YSX::MULW:
    return true;
  }

  return false;
}

std::optional<unsigned>
YSXInstrInfo::getInverseOpcode(unsigned Opcode) const {
#if 0
#define YSXVec_OPC_LMUL_CASE(OPC, INV)                                            \
  case YSX::OPC##_M1:                                                        \
    return YSX::INV##_M1;                                                    \
  case YSX::OPC##_M2:                                                        \
    return YSX::INV##_M2;                                                    \
  case YSX::OPC##_M4:                                                        \
    return YSX::INV##_M4;                                                    \
  case YSX::OPC##_M8:                                                        \
    return YSX::INV##_M8;                                                    \
  case YSX::OPC##_MF2:                                                       \
    return YSX::INV##_MF2;                                                   \
  case YSX::OPC##_MF4:                                                       \
    return YSX::INV##_MF4;                                                   \
  case YSX::OPC##_MF8:                                                       \
    return YSX::INV##_MF8

#define YSXVec_OPC_LMUL_MASK_CASE(OPC, INV)                                       \
  case YSX::OPC##_M1_MASK:                                                   \
    return YSX::INV##_M1_MASK;                                               \
  case YSX::OPC##_M2_MASK:                                                   \
    return YSX::INV##_M2_MASK;                                               \
  case YSX::OPC##_M4_MASK:                                                   \
    return YSX::INV##_M4_MASK;                                               \
  case YSX::OPC##_M8_MASK:                                                   \
    return YSX::INV##_M8_MASK;                                               \
  case YSX::OPC##_MF2_MASK:                                                  \
    return YSX::INV##_MF2_MASK;                                              \
  case YSX::OPC##_MF4_MASK:                                                  \
    return YSX::INV##_MF4_MASK;                                              \
  case YSX::OPC##_MF8_MASK:                                                  \
    return YSX::INV##_MF8_MASK

  switch (Opcode) {
  default:
    return std::nullopt;
  case YSX::FADD_H:
    return YSX::FSUB_H;
  case YSX::FADD_S:
    return YSX::FSUB_S;
  case YSX::FADD_D:
    return YSX::FSUB_D;
  case YSX::FSUB_H:
    return YSX::FADD_H;
  case YSX::FSUB_S:
    return YSX::FADD_S;
  case YSX::FSUB_D:
    return YSX::FADD_D;
  case YSX::ADD:
    return YSX::SUB;
  case YSX::SUB:
    return YSX::ADD;
  case YSX::ADDW:
    return YSX::SUBW;
  case YSX::SUBW:
    return YSX::ADDW;
    // clang-format off
  YSXVec_OPC_LMUL_CASE(PseudoVADD_VV, PseudoVSUB_VV);
  YSXVec_OPC_LMUL_MASK_CASE(PseudoVADD_VV, PseudoVSUB_VV);
  YSXVec_OPC_LMUL_CASE(PseudoVSUB_VV, PseudoVADD_VV);
  YSXVec_OPC_LMUL_MASK_CASE(PseudoVSUB_VV, PseudoVADD_VV);
    // clang-format on
  }

#undef YSXVec_OPC_LMUL_MASK_CASE
#undef YSXVec_OPC_LMUL_CASE
#endif

  switch (Opcode) {
  default:
    return std::nullopt;
  case YSX::ADD:
    return YSX::SUB;
  case YSX::SUB:
    return YSX::ADD;
  case YSX::ADDW:
    return YSX::SUBW;
  case YSX::SUBW:
    return YSX::ADDW;
  }
}

#if 0
static bool canCombineFPFusedMultiply(const MachineInstr &Root,
                                      const MachineOperand &MO,
                                      bool DoRegPressureReduce) {
  if (!MO.isReg() || !MO.getReg().isVirtual())
    return false;
  const MachineRegisterInfo &MRI = Root.getMF()->getRegInfo();
  MachineInstr *MI = MRI.getVRegDef(MO.getReg());
  if (!MI || !isFMUL(MI->getOpcode()))
    return false;

  if (!Root.getFlag(MachineInstr::MIFlag::FmContract) ||
      !MI->getFlag(MachineInstr::MIFlag::FmContract))
    return false;

  // Try combining even if fmul has more than one use as it eliminates
  // dependency between fadd(fsub) and fmul. However, it can extend liveranges
  // for fmul operands, so reject the transformation in register pressure
  // reduction mode.
  if (DoRegPressureReduce && !MRI.hasOneNonDBGUse(MI->getOperand(0).getReg()))
    return false;

  // Do not combine instructions from different basic blocks.
  if (Root.getParent() != MI->getParent())
    return false;
  return YSX::hasEqualFRM(Root, *MI);
}
#endif

static bool getFPFusedMultiplyPatterns(MachineInstr &Root,
                                       SmallVectorImpl<unsigned> &Patterns,
                                       bool DoRegPressureReduce) {
#if 0
  unsigned Opc = Root.getOpcode();
  bool IsFAdd = isFADD(Opc);
  if (!IsFAdd && !isFSUB(Opc))
    return false;
  bool Added = false;
  if (canCombineFPFusedMultiply(Root, Root.getOperand(1),
                                DoRegPressureReduce)) {
    Patterns.push_back(IsFAdd ? YSXMachineCombinerPattern::FMADD_AX
                              : YSXMachineCombinerPattern::FMSUB);
    Added = true;
  }
  if (canCombineFPFusedMultiply(Root, Root.getOperand(2),
                                DoRegPressureReduce)) {
    Patterns.push_back(IsFAdd ? YSXMachineCombinerPattern::FMADD_XA
                              : YSXMachineCombinerPattern::FNMSUB);
    Added = true;
  }
  return Added;
#endif
  return false;
}

static bool getFPPatterns(MachineInstr &Root,
                          SmallVectorImpl<unsigned> &Patterns,
                          bool DoRegPressureReduce) {
  return getFPFusedMultiplyPatterns(Root, Patterns, DoRegPressureReduce);
}

// Look for opportunities to combine (sh3add Z, (add X, (slli Y, 5))) into
// (sh3add (sh2add Y, Z), X).
static bool getSHXADDPatterns(const MachineInstr &Root,
                              SmallVectorImpl<unsigned> &Patterns) {
  return false;
#if 0
  unsigned ShiftAmt = getSHXADDShiftAmount(Root.getOpcode());
  if (!ShiftAmt)
    return false;

  const MachineBasicBlock &MBB = *Root.getParent();

  const MachineInstr *AddMI = canCombine(MBB, Root.getOperand(2), YSX::ADD);
  if (!AddMI)
    return false;

  bool Found = false;
  if (canCombineShiftIntoShXAdd(MBB, AddMI->getOperand(1), ShiftAmt)) {
    Patterns.push_back(YSXMachineCombinerPattern::SHXADD_ADD_SLLI_OP1);
    Found = true;
  }
  if (canCombineShiftIntoShXAdd(MBB, AddMI->getOperand(2), ShiftAmt)) {
    Patterns.push_back(YSXMachineCombinerPattern::SHXADD_ADD_SLLI_OP2);
    Found = true;
  }

  return Found;
#endif
}

CombinerObjective YSXInstrInfo::getCombinerObjective(unsigned Pattern) const {
  switch (Pattern) {
  case YSXMachineCombinerPattern::FMADD_AX:
  case YSXMachineCombinerPattern::FMADD_XA:
  case YSXMachineCombinerPattern::FMSUB:
  case YSXMachineCombinerPattern::FNMSUB:
    return CombinerObjective::MustReduceDepth;
  default:
    return TargetInstrInfo::getCombinerObjective(Pattern);
  }
}

bool YSXInstrInfo::getMachineCombinerPatterns(
    MachineInstr &Root, SmallVectorImpl<unsigned> &Patterns,
    bool DoRegPressureReduce) const {

  if (getFPPatterns(Root, Patterns, DoRegPressureReduce))
    return true;

  if (getSHXADDPatterns(Root, Patterns))
    return true;

  return TargetInstrInfo::getMachineCombinerPatterns(Root, Patterns,
                                                     DoRegPressureReduce);
}

#if 0
static unsigned getFPFusedMultiplyOpcode(unsigned RootOpc, unsigned Pattern) {
#if 0
  switch (RootOpc) {
  default:
    llvm_unreachable("Unexpected opcode");
  case YSX::FADD_H:
    return YSX::FMADD_H;
  case YSX::FADD_S:
    return YSX::FMADD_S;
  case YSX::FADD_D:
    return YSX::FMADD_D;
  case YSX::FSUB_H:
    return Pattern == YSXMachineCombinerPattern::FMSUB ? YSX::FMSUB_H
                                                         : YSX::FNMSUB_H;
  case YSX::FSUB_S:
    return Pattern == YSXMachineCombinerPattern::FMSUB ? YSX::FMSUB_S
                                                         : YSX::FNMSUB_S;
  case YSX::FSUB_D:
    return Pattern == YSXMachineCombinerPattern::FMSUB ? YSX::FMSUB_D
                                                         : YSX::FNMSUB_D;
  }
#endif
  llvm_unreachable("FP fused multiply is not supported by YSX rv64ima");
}

static unsigned getAddendOperandIdx(unsigned Pattern) {
  switch (Pattern) {
  default:
    llvm_unreachable("Unexpected pattern");
  case YSXMachineCombinerPattern::FMADD_AX:
  case YSXMachineCombinerPattern::FMSUB:
    return 2;
  case YSXMachineCombinerPattern::FMADD_XA:
  case YSXMachineCombinerPattern::FNMSUB:
    return 1;
  }
}

static void combineFPFusedMultiply(MachineInstr &Root, MachineInstr &Prev,
                                   unsigned Pattern,
                                   SmallVectorImpl<MachineInstr *> &InsInstrs,
                                   SmallVectorImpl<MachineInstr *> &DelInstrs) {
  MachineFunction *MF = Root.getMF();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();

  MachineOperand &Mul1 = Prev.getOperand(1);
  MachineOperand &Mul2 = Prev.getOperand(2);
  MachineOperand &Dst = Root.getOperand(0);
  MachineOperand &Addend = Root.getOperand(getAddendOperandIdx(Pattern));

  Register DstReg = Dst.getReg();
  unsigned FusedOpc = getFPFusedMultiplyOpcode(Root.getOpcode(), Pattern);
  uint32_t IntersectedFlags = Root.getFlags() & Prev.getFlags();
  DebugLoc MergedLoc =
      DILocation::getMergedLocation(Root.getDebugLoc(), Prev.getDebugLoc());

  bool Mul1IsKill = Mul1.isKill();
  bool Mul2IsKill = Mul2.isKill();
  bool AddendIsKill = Addend.isKill();

  // We need to clear kill flags since we may be extending the live range past
  // a kill. If the mul had kill flags, we can preserve those since we know
  // where the previous range stopped.
  MRI.clearKillFlags(Mul1.getReg());
  MRI.clearKillFlags(Mul2.getReg());

  MachineInstrBuilder MIB =
      BuildMI(*MF, MergedLoc, TII->get(FusedOpc), DstReg)
          .addReg(Mul1.getReg(), getKillRegState(Mul1IsKill))
          .addReg(Mul2.getReg(), getKillRegState(Mul2IsKill))
          .addReg(Addend.getReg(), getKillRegState(AddendIsKill))
          .setMIFlags(IntersectedFlags);

  InsInstrs.push_back(MIB);
  if (MRI.hasOneNonDBGUse(Prev.getOperand(0).getReg()))
    DelInstrs.push_back(&Prev);
  DelInstrs.push_back(&Root);
}
#endif

// Combine patterns like (sh3add Z, (add X, (slli Y, 5))) to
// (sh3add (sh2add Y, Z), X) if the shift amount can be split across two
// shXadd instructions. The outer shXadd keeps its original opcode.
static void
genShXAddAddShift(MachineInstr &Root, unsigned AddOpIdx,
                  SmallVectorImpl<MachineInstr *> &InsInstrs,
                  SmallVectorImpl<MachineInstr *> &DelInstrs,
                  DenseMap<Register, unsigned> &InstrIdxForVirtReg) {
  llvm_unreachable("YSX does not support Zba SHxADD combines");
#if 0
  MachineFunction *MF = Root.getMF();
  MachineRegisterInfo &MRI = MF->getRegInfo();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();

  unsigned OuterShiftAmt = getSHXADDShiftAmount(Root.getOpcode());
  assert(OuterShiftAmt != 0 && "Unexpected opcode");

  MachineInstr *AddMI = MRI.getUniqueVRegDef(Root.getOperand(2).getReg());
  MachineInstr *ShiftMI =
      MRI.getUniqueVRegDef(AddMI->getOperand(AddOpIdx).getReg());

  unsigned InnerShiftAmt = ShiftMI->getOperand(2).getImm();
  assert(InnerShiftAmt >= OuterShiftAmt && "Unexpected shift amount");

  unsigned InnerOpc;
  switch (InnerShiftAmt - OuterShiftAmt) {
  default:
    llvm_unreachable("Unexpected shift amount");
  case 0:
    InnerOpc = YSX::ADD;
    break;
  case 1:
    InnerOpc = YSX::SH1ADD;
    break;
  case 2:
    InnerOpc = YSX::SH2ADD;
    break;
  case 3:
    InnerOpc = YSX::SH3ADD;
    break;
  }

  const MachineOperand &X = AddMI->getOperand(3 - AddOpIdx);
  const MachineOperand &Y = ShiftMI->getOperand(1);
  const MachineOperand &Z = Root.getOperand(1);

  Register NewVR = MRI.createVirtualRegister(&YSX::GPRRegClass);

  auto MIB1 = BuildMI(*MF, MIMetadata(Root), TII->get(InnerOpc), NewVR)
                  .addReg(Y.getReg(), getKillRegState(Y.isKill()))
                  .addReg(Z.getReg(), getKillRegState(Z.isKill()));
  auto MIB2 = BuildMI(*MF, MIMetadata(Root), TII->get(Root.getOpcode()),
                      Root.getOperand(0).getReg())
                  .addReg(NewVR, RegState::Kill)
                  .addReg(X.getReg(), getKillRegState(X.isKill()));

  InstrIdxForVirtReg.insert(std::make_pair(NewVR, 0));
  InsInstrs.push_back(MIB1);
  InsInstrs.push_back(MIB2);
  DelInstrs.push_back(ShiftMI);
  DelInstrs.push_back(AddMI);
  DelInstrs.push_back(&Root);
#endif
}

void YSXInstrInfo::genAlternativeCodeSequence(
    MachineInstr &Root, unsigned Pattern,
    SmallVectorImpl<MachineInstr *> &InsInstrs,
    SmallVectorImpl<MachineInstr *> &DelInstrs,
    DenseMap<Register, unsigned> &InstrIdxForVirtReg) const {
#if 0
  MachineRegisterInfo &MRI = Root.getMF()->getRegInfo();
#endif
  switch (Pattern) {
  default:
    TargetInstrInfo::genAlternativeCodeSequence(Root, Pattern, InsInstrs,
                                                DelInstrs, InstrIdxForVirtReg);
    return;
  case YSXMachineCombinerPattern::FMADD_AX:
  case YSXMachineCombinerPattern::FMSUB:
#if 0
  {
    MachineInstr &Prev = *MRI.getVRegDef(Root.getOperand(1).getReg());
    combineFPFusedMultiply(Root, Prev, Pattern, InsInstrs, DelInstrs);
    return;
  }
#endif
    llvm_unreachable("FP fused multiply is not supported by YSX rv64ima");
  case YSXMachineCombinerPattern::FMADD_XA:
  case YSXMachineCombinerPattern::FNMSUB:
#if 0
  {
    MachineInstr &Prev = *MRI.getVRegDef(Root.getOperand(2).getReg());
    combineFPFusedMultiply(Root, Prev, Pattern, InsInstrs, DelInstrs);
    return;
  }
#endif
    llvm_unreachable("FP fused multiply is not supported by YSX rv64ima");
  case YSXMachineCombinerPattern::SHXADD_ADD_SLLI_OP1:
    genShXAddAddShift(Root, 1, InsInstrs, DelInstrs, InstrIdxForVirtReg);
    return;
  case YSXMachineCombinerPattern::SHXADD_ADD_SLLI_OP2:
    genShXAddAddShift(Root, 2, InsInstrs, DelInstrs, InstrIdxForVirtReg);
    return;
  }
}

bool YSXInstrInfo::verifyInstruction(const MachineInstr &MI,
                                       StringRef &ErrInfo) const {
  MCInstrDesc const &Desc = MI.getDesc();

  for (const auto &[Index, Operand] : enumerate(Desc.operands())) {
    const MachineOperand &MO = MI.getOperand(Index);
    unsigned OpType = Operand.OperandType;
    switch (OpType) {
    default:
      if (OpType >= YSXOp::OPERAND_FIRST_YSX_IMM &&
          OpType <= YSXOp::OPERAND_LAST_YSX_IMM) {
        if (!MO.isImm()) {
          ErrInfo = "Expected an immediate operand.";
          return false;
        }
        int64_t Imm = MO.getImm();
        bool Ok;
        switch (OpType) {
        default:
          llvm_unreachable("Unexpected operand type");

          // clang-format off
#define CASE_OPERAND_UIMM(NUM)                                                 \
  case YSXOp::OPERAND_UIMM##NUM:                                             \
    Ok = isUInt<NUM>(Imm);                                                     \
    break;
#define CASE_OPERAND_SIMM(NUM)                                                 \
  case YSXOp::OPERAND_SIMM##NUM:                                             \
    Ok = isInt<NUM>(Imm);                                                      \
    break;
        CASE_OPERAND_UIMM(1)
        CASE_OPERAND_UIMM(2)
        CASE_OPERAND_UIMM(3)
        CASE_OPERAND_UIMM(4)
        CASE_OPERAND_UIMM(5)
        CASE_OPERAND_UIMM(6)
        CASE_OPERAND_UIMM(7)
        CASE_OPERAND_UIMM(8)
        CASE_OPERAND_UIMM(9)
        CASE_OPERAND_UIMM(10)
        CASE_OPERAND_UIMM(12)
        CASE_OPERAND_UIMM(16)
        CASE_OPERAND_UIMM(32)
        CASE_OPERAND_UIMM(48)
        CASE_OPERAND_UIMM(64)
          // clang-format on
        case YSXOp::OPERAND_UIMM2_LSB0:
          Ok = isShiftedUInt<1, 1>(Imm);
          break;
        case YSXOp::OPERAND_UIMM5_LSB0:
          Ok = isShiftedUInt<4, 1>(Imm);
          break;
        case YSXOp::OPERAND_UIMM5_NONZERO:
          Ok = isUInt<5>(Imm) && (Imm != 0);
          break;
        case YSXOp::OPERAND_UIMM5_GT3:
          Ok = isUInt<5>(Imm) && (Imm > 3);
          break;
        case YSXOp::OPERAND_UIMM5_PLUS1:
          Ok = Imm >= 1 && Imm <= 32;
          break;
        case YSXOp::OPERAND_UIMM6_LSB0:
          Ok = isShiftedUInt<5, 1>(Imm);
          break;
        case YSXOp::OPERAND_UIMM7_LSB00:
          Ok = isShiftedUInt<5, 2>(Imm);
          break;
        case YSXOp::OPERAND_UIMM7_LSB000:
          Ok = isShiftedUInt<4, 3>(Imm);
          break;
        case YSXOp::OPERAND_UIMM8_LSB00:
          Ok = isShiftedUInt<6, 2>(Imm);
          break;
        case YSXOp::OPERAND_UIMM8_LSB000:
          Ok = isShiftedUInt<5, 3>(Imm);
          break;
        case YSXOp::OPERAND_UIMM8_GE32:
          Ok = isUInt<8>(Imm) && Imm >= 32;
          break;
        case YSXOp::OPERAND_UIMM9_LSB000:
          Ok = isShiftedUInt<6, 3>(Imm);
          break;
        case YSXOp::OPERAND_SIMM8_UNSIGNED:
          Ok = isInt<8>(Imm);
          break;
        case YSXOp::OPERAND_SIMM10_LSB0000_NONZERO:
          Ok = isShiftedInt<6, 4>(Imm) && (Imm != 0);
          break;
        case YSXOp::OPERAND_UIMM10_LSB00_NONZERO:
          Ok = isShiftedUInt<8, 2>(Imm) && (Imm != 0);
          break;
        case YSXOp::OPERAND_UIMM16_NONZERO:
          Ok = isUInt<16>(Imm) && (Imm != 0);
          break;
        case YSXOp::OPERAND_THREE:
          Ok = Imm == 3;
          break;
        case YSXOp::OPERAND_FOUR:
          Ok = Imm == 4;
          break;
        case YSXOp::OPERAND_IMM5_ZIBI:
          Ok = (isUInt<5>(Imm) && Imm != 0) || Imm == -1;
          break;
          // clang-format off
        CASE_OPERAND_SIMM(5)
        CASE_OPERAND_SIMM(6)
        CASE_OPERAND_SIMM(10)
        CASE_OPERAND_SIMM(11)
        CASE_OPERAND_SIMM(26)
        // clang-format on
        case YSXOp::OPERAND_SIMM5_PLUS1:
          Ok = Imm >= -15 && Imm <= 16;
          break;
        case YSXOp::OPERAND_SIMM5_NONZERO:
          Ok = isInt<5>(Imm) && (Imm != 0);
          break;
        case YSXOp::OPERAND_SIMM6_NONZERO:
          Ok = Imm != 0 && isInt<6>(Imm);
          break;
        case YSXOp::OPERAND_VTYPEI10:
          Ok = isUInt<10>(Imm);
          break;
        case YSXOp::OPERAND_VTYPEI11:
          Ok = isUInt<11>(Imm);
          break;
        case YSXOp::OPERAND_SIMM12_LSB00000:
          Ok = isShiftedInt<7, 5>(Imm);
          break;
        case YSXOp::OPERAND_SIMM16_NONZERO:
          Ok = isInt<16>(Imm) && (Imm != 0);
          break;
        case YSXOp::OPERAND_SIMM20_LI:
          Ok = isInt<20>(Imm);
          break;
        case YSXOp::OPERAND_UIMMLOG2XLEN:
          Ok = STI.is64Bit() ? isUInt<6>(Imm) : isUInt<5>(Imm);
          break;
        case YSXOp::OPERAND_UIMMLOG2XLEN_NONZERO:
          Ok = STI.is64Bit() ? isUInt<6>(Imm) : isUInt<5>(Imm);
          Ok = Ok && Imm != 0;
          break;
        case YSXOp::OPERAND_CLUI_IMM:
          Ok = (isUInt<5>(Imm) && Imm != 0) || (Imm >= 0xfffe0 && Imm <= 0xfffff);
          break;
        case YSXOp::OPERAND_RVKRNUM:
          Ok = Imm >= 0 && Imm <= 10;
          break;
        case YSXOp::OPERAND_RVKRNUM_0_7:
          Ok = Imm >= 0 && Imm <= 7;
          break;
        case YSXOp::OPERAND_RVKRNUM_1_10:
          Ok = Imm >= 1 && Imm <= 10;
          break;
        case YSXOp::OPERAND_RVKRNUM_2_14:
          Ok = Imm >= 2 && Imm <= 14;
          break;
        case YSXOp::OPERAND_RLIST:
          Ok = Imm >= YSXZC::RA && Imm <= YSXZC::RA_S0_S11;
          break;
        case YSXOp::OPERAND_RLIST_S0:
          Ok = Imm >= YSXZC::RA_S0 && Imm <= YSXZC::RA_S0_S11;
          break;
        case YSXOp::OPERAND_STACKADJ:
          Ok = Imm >= 0 && Imm <= 48 && Imm % 16 == 0;
          break;
        case YSXOp::OPERAND_FRMARG:
          Ok = YSXFPRndMode::isValidRoundingMode(Imm);
          break;
        case YSXOp::OPERAND_RTZARG:
          Ok = Imm == YSXFPRndMode::RTZ;
          break;
        case YSXOp::OPERAND_COND_CODE:
          Ok = Imm >= 0 && Imm < YSXCC::COND_INVALID;
          break;
        case YSXOp::OPERAND_ATOMIC_ORDERING:
          Ok = isValidAtomicOrdering(Imm);
          break;
        case YSXOp::OPERAND_VEC_POLICY:
          Ok = false;
          break;
        case YSXOp::OPERAND_SEW:
          Ok = false;
          break;
        case YSXOp::OPERAND_SEW_MASK:
          Ok = Imm == 0;
          break;
        case YSXOp::OPERAND_VEC_RM:
          assert(YSXII::hasRoundModeOp(Desc.TSFlags));
          if (YSXII::usesVXRM(Desc.TSFlags))
            Ok = isUInt<2>(Imm);
          else
            Ok = YSXFPRndMode::isValidRoundingMode(Imm);
          break;
        case YSXOp::OPERAND_XSFMM_VTYPE:
          Ok = false;
          break;
        case YSXOp::OPERAND_XSFMM_TWIDEN:
          Ok = Imm == 1 || Imm == 2 || Imm == 4;
          break;
        }
        if (!Ok) {
          ErrInfo = "Invalid immediate";
          return false;
        }
      }
      break;
    case YSXOp::OPERAND_SIMM12_LO:
      // TODO: We could be stricter about what non-register operands are
      // allowed.
      if (MO.isReg()) {
        ErrInfo = "Expected a non-register operand.";
        return false;
      }
      if (MO.isImm() && !isInt<12>(MO.getImm())) {
        ErrInfo = "Invalid immediate";
        return false;
      }
      break;
    case YSXOp::OPERAND_UIMM20_LUI:
    case YSXOp::OPERAND_UIMM20_AUIPC:
      // TODO: We could be stricter about what non-register operands are
      // allowed.
      if (MO.isReg()) {
        ErrInfo = "Expected a non-register operand.";
        return false;
      }
      if (MO.isImm() && !isUInt<20>(MO.getImm())) {
        ErrInfo = "Invalid immediate";
        return false;
      }
      break;
    case YSXOp::OPERAND_BARE_SIMM32:
      // TODO: We could be stricter about what non-register operands are
      // allowed.
      if (MO.isReg()) {
        ErrInfo = "Expected a non-register operand.";
        return false;
      }
      if (MO.isImm() && !isInt<32>(MO.getImm())) {
        ErrInfo = "Invalid immediate";
        return false;
      }
      break;
    case YSXOp::OPERAND_AVL:
      if (MO.isImm()) {
        int64_t Imm = MO.getImm();
        // VLMAX is represented as -1.
        if (!isUInt<5>(Imm) && Imm != -1) {
          ErrInfo = "Invalid immediate";
          return false;
        }
      } else if (!MO.isReg()) {
        ErrInfo = "Expected a register or immediate operand.";
        return false;
      }
      break;
    }
  }

  const uint64_t TSFlags = Desc.TSFlags;
  if (YSXII::hasVLOp(TSFlags)) {
    const MachineOperand &Op = MI.getOperand(YSXII::getVLOpNum(Desc));
    if (!Op.isImm() && !Op.isReg())  {
      ErrInfo = "Invalid operand type for VL operand";
      return false;
    }
    if (Op.isReg() && Op.getReg().isValid()) {
      const MachineRegisterInfo &MRI = MI.getParent()->getParent()->getRegInfo();
      auto *RC = MRI.getRegClass(Op.getReg());
      if (!YSX::GPRNoX0RegClass.hasSubClassEq(RC)) {
        ErrInfo = "Invalid register class for VL operand";
        return false;
      }
    }
    if (!YSXII::hasSEWOp(TSFlags)) {
      ErrInfo = "VL operand w/o SEW operand?";
      return false;
    }
  }
  if (YSXII::hasSEWOp(TSFlags)) {
    ErrInfo = "YSX does not support vector SEW operands";
    return false;
  }
  if (YSXII::hasVecPolicyOp(TSFlags)) {
    ErrInfo = "YSX does not support vector policy operands";
    return false;
  }

  return true;
}

bool YSXInstrInfo::canFoldIntoAddrMode(const MachineInstr &MemI, Register Reg,
                                         const MachineInstr &AddrI,
                                         ExtAddrMode &AM) const {
  switch (MemI.getOpcode()) {
  default:
    return false;
  case YSX::LB:
  case YSX::LBU:
  case YSX::LH:
  case YSX::LHU:
  case YSX::LW:
  case YSX::LWU:
  case YSX::LD:
  case YSX::SB:
  case YSX::SH:
  case YSX::SW:
  case YSX::SD:
    break;
  }

  if (MemI.getOperand(0).getReg() == Reg)
    return false;

  if (AddrI.getOpcode() != YSX::ADDI || !AddrI.getOperand(1).isReg() ||
      !AddrI.getOperand(2).isImm())
    return false;

  int64_t OldOffset = MemI.getOperand(2).getImm();
  int64_t Disp = AddrI.getOperand(2).getImm();
  int64_t NewOffset = OldOffset + Disp;
  if (!STI.is64Bit())
    NewOffset = SignExtend64<32>(NewOffset);

  if (!isInt<12>(NewOffset))
    return false;

  AM.BaseReg = AddrI.getOperand(1).getReg();
  AM.ScaledReg = 0;
  AM.Scale = 0;
  AM.Displacement = NewOffset;
  AM.Form = ExtAddrMode::Formula::Basic;
  return true;
}

MachineInstr *YSXInstrInfo::emitLdStWithAddr(MachineInstr &MemI,
                                               const ExtAddrMode &AM) const {

  const DebugLoc &DL = MemI.getDebugLoc();
  MachineBasicBlock &MBB = *MemI.getParent();

  assert(AM.ScaledReg == 0 && AM.Scale == 0 &&
         "Addressing mode not supported for folding");

  return BuildMI(MBB, MemI, DL, get(MemI.getOpcode()))
      .addReg(MemI.getOperand(0).getReg(),
              MemI.mayLoad() ? RegState::Define : 0)
      .addReg(AM.BaseReg)
      .addImm(AM.Displacement)
      .setMemRefs(MemI.memoperands())
      .setMIFlags(MemI.getFlags());
}

// TODO: At the moment, MIPS introduced paring of instructions operating with
// word or double word. This should be extended with more instructions when more
// vendors support load/store pairing.
bool YSXInstrInfo::isPairableLdStInstOpc(unsigned Opc) {
  switch (Opc) {
  default:
    return false;
  case YSX::SW:
  case YSX::SD:
  case YSX::LD:
  case YSX::LW:
    return true;
  }
}

bool YSXInstrInfo::isLdStSafeToPair(const MachineInstr &LdSt,
                                      const TargetRegisterInfo *TRI) {
  // If this is a volatile load/store, don't mess with it.
  if (LdSt.hasOrderedMemoryRef() || LdSt.getNumExplicitOperands() != 3)
    return false;

  if (LdSt.getOperand(1).isFI())
    return true;

  assert(LdSt.getOperand(1).isReg() && "Expected a reg operand.");
  // Can't cluster if the instruction modifies the base register
  // or it is update form. e.g. ld x5,8(x5)
  if (LdSt.modifiesRegister(LdSt.getOperand(1).getReg(), TRI))
    return false;

  if (!LdSt.getOperand(2).isImm())
    return false;

  return true;
}

bool YSXInstrInfo::getMemOperandsWithOffsetWidth(
    const MachineInstr &LdSt, SmallVectorImpl<const MachineOperand *> &BaseOps,
    int64_t &Offset, bool &OffsetIsScalable, LocationSize &Width,
    const TargetRegisterInfo *TRI) const {
  if (!LdSt.mayLoadOrStore())
    return false;

  // Conservatively, only handle scalar loads/stores for now.
  switch (LdSt.getOpcode()) {
  case YSX::LB:
  case YSX::LBU:
  case YSX::SB:
  case YSX::LH:
  case YSX::LHU:
  case YSX::SH:
  case YSX::LW:
  case YSX::LWU:
  case YSX::SW:
  case YSX::LD:
  case YSX::SD:
    break;
  default:
    return false;
  }
  const MachineOperand *BaseOp;
  OffsetIsScalable = false;
  if (!getMemOperandWithOffsetWidth(LdSt, BaseOp, Offset, Width, TRI))
    return false;
  BaseOps.push_back(BaseOp);
  return true;
}

// TODO: This was copied from SIInstrInfo. Could it be lifted to a common
// helper?
static bool memOpsHaveSameBasePtr(const MachineInstr &MI1,
                                  ArrayRef<const MachineOperand *> BaseOps1,
                                  const MachineInstr &MI2,
                                  ArrayRef<const MachineOperand *> BaseOps2) {
  // Only examine the first "base" operand of each instruction, on the
  // assumption that it represents the real base address of the memory access.
  // Other operands are typically offsets or indices from this base address.
  if (BaseOps1.front()->isIdenticalTo(*BaseOps2.front()))
    return true;

  if (!MI1.hasOneMemOperand() || !MI2.hasOneMemOperand())
    return false;

  auto MO1 = *MI1.memoperands_begin();
  auto MO2 = *MI2.memoperands_begin();
  if (MO1->getAddrSpace() != MO2->getAddrSpace())
    return false;

  auto Base1 = MO1->getValue();
  auto Base2 = MO2->getValue();
  if (!Base1 || !Base2)
    return false;
  Base1 = getUnderlyingObject(Base1);
  Base2 = getUnderlyingObject(Base2);

  if (isa<UndefValue>(Base1) || isa<UndefValue>(Base2))
    return false;

  return Base1 == Base2;
}

bool YSXInstrInfo::shouldClusterMemOps(
    ArrayRef<const MachineOperand *> BaseOps1, int64_t Offset1,
    bool OffsetIsScalable1, ArrayRef<const MachineOperand *> BaseOps2,
    int64_t Offset2, bool OffsetIsScalable2, unsigned ClusterSize,
    unsigned NumBytes) const {
  // If the mem ops (to be clustered) do not have the same base ptr, then they
  // should not be clustered
  if (!BaseOps1.empty() && !BaseOps2.empty()) {
    const MachineInstr &FirstLdSt = *BaseOps1.front()->getParent();
    const MachineInstr &SecondLdSt = *BaseOps2.front()->getParent();
    if (!memOpsHaveSameBasePtr(FirstLdSt, BaseOps1, SecondLdSt, BaseOps2))
      return false;
  } else if (!BaseOps1.empty() || !BaseOps2.empty()) {
    // If only one base op is empty, they do not have the same base ptr
    return false;
  }

  unsigned CacheLineSize =
      BaseOps1.front()->getParent()->getMF()->getSubtarget().getCacheLineSize();
  // Assume a cache line size of 64 bytes if no size is set in YSXSubtarget.
  CacheLineSize = CacheLineSize ? CacheLineSize : 64;
  // Cluster if the memory operations are on the same or a neighbouring cache
  // line, but limit the maximum ClusterSize to avoid creating too much
  // additional register pressure.
  return ClusterSize <= 4 && std::abs(Offset1 - Offset2) < CacheLineSize;
}

// Set BaseReg (the base register operand), Offset (the byte offset being
// accessed) and the access Width of the passed instruction that reads/writes
// memory. Returns false if the instruction does not read/write memory or the
// BaseReg/Offset/Width can't be determined. Is not guaranteed to always
// recognise base operands and offsets in all cases.
// TODO: Add an IsScalable bool ref argument (like the equivalent AArch64
// function) and set it as appropriate.
bool YSXInstrInfo::getMemOperandWithOffsetWidth(
    const MachineInstr &LdSt, const MachineOperand *&BaseReg, int64_t &Offset,
    LocationSize &Width, const TargetRegisterInfo *TRI) const {
  if (!LdSt.mayLoadOrStore())
    return false;

  // Here we assume the standard RISC-V ISA, which uses a base+offset
  // addressing mode. You'll need to relax these conditions to support custom
  // load/store instructions.
  if (LdSt.getNumExplicitOperands() != 3)
    return false;
  if ((!LdSt.getOperand(1).isReg() && !LdSt.getOperand(1).isFI()) ||
      !LdSt.getOperand(2).isImm())
    return false;

  if (!LdSt.hasOneMemOperand())
    return false;

  Width = (*LdSt.memoperands_begin())->getSize();
  BaseReg = &LdSt.getOperand(1);
  Offset = LdSt.getOperand(2).getImm();
  return true;
}

bool YSXInstrInfo::areMemAccessesTriviallyDisjoint(
    const MachineInstr &MIa, const MachineInstr &MIb) const {
  assert(MIa.mayLoadOrStore() && "MIa must be a load or store.");
  assert(MIb.mayLoadOrStore() && "MIb must be a load or store.");

  if (MIa.hasUnmodeledSideEffects() || MIb.hasUnmodeledSideEffects() ||
      MIa.hasOrderedMemoryRef() || MIb.hasOrderedMemoryRef())
    return false;

  // Retrieve the base register, offset from the base register and width. Width
  // is the size of memory that is being loaded/stored (e.g. 1, 2, 4).  If
  // base registers are identical, and the offset of a lower memory access +
  // the width doesn't overlap the offset of a higher memory access,
  // then the memory accesses are different.
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();
  const MachineOperand *BaseOpA = nullptr, *BaseOpB = nullptr;
  int64_t OffsetA = 0, OffsetB = 0;
  LocationSize WidthA = LocationSize::precise(0),
               WidthB = LocationSize::precise(0);
  if (getMemOperandWithOffsetWidth(MIa, BaseOpA, OffsetA, WidthA, TRI) &&
      getMemOperandWithOffsetWidth(MIb, BaseOpB, OffsetB, WidthB, TRI)) {
    if (BaseOpA->isIdenticalTo(*BaseOpB)) {
      int LowOffset = std::min(OffsetA, OffsetB);
      int HighOffset = std::max(OffsetA, OffsetB);
      LocationSize LowWidth = (LowOffset == OffsetA) ? WidthA : WidthB;
      if (LowWidth.hasValue() &&
          LowOffset + (int)LowWidth.getValue() <= HighOffset)
        return true;
    }
  }
  return false;
}

std::pair<unsigned, unsigned>
YSXInstrInfo::decomposeMachineOperandsTargetFlags(unsigned TF) const {
  const unsigned Mask = YSXII::MO_DIRECT_FLAG_MASK;
  return std::make_pair(TF & Mask, TF & ~Mask);
}

ArrayRef<std::pair<unsigned, const char *>>
YSXInstrInfo::getSerializableDirectMachineOperandTargetFlags() const {
  using namespace YSXII;
  static const std::pair<unsigned, const char *> TargetFlags[] = {
      {MO_CALL, "ysx-call"},
      {MO_LO, "ysx-lo"},
      {MO_HI, "ysx-hi"},
      {MO_PCREL_LO, "ysx-pcrel-lo"},
      {MO_PCREL_HI, "ysx-pcrel-hi"},
      {MO_GOT_HI, "ysx-got-hi"},
      {MO_TPREL_LO, "ysx-tprel-lo"},
      {MO_TPREL_HI, "ysx-tprel-hi"},
      {MO_TPREL_ADD, "ysx-tprel-add"},
      {MO_TLS_GOT_HI, "ysx-tls-got-hi"},
      {MO_TLS_GD_HI, "ysx-tls-gd-hi"},
      {MO_TLSDESC_HI, "ysx-tlsdesc-hi"},
      {MO_TLSDESC_LOAD_LO, "ysx-tlsdesc-load-lo"},
      {MO_TLSDESC_ADD_LO, "ysx-tlsdesc-add-lo"},
      {MO_TLSDESC_CALL, "ysx-tlsdesc-call"}};
  return ArrayRef(TargetFlags);
}
bool YSXInstrInfo::isFunctionSafeToOutlineFrom(
    MachineFunction &MF, bool OutlineFromLinkOnceODRs) const {
  const Function &F = MF.getFunction();

  // Can F be deduplicated by the linker? If it can, don't outline from it.
  if (!OutlineFromLinkOnceODRs && F.hasLinkOnceODRLinkage())
    return false;

  // Don't outline from functions with section markings; the program could
  // expect that all the code is in the named section.
  if (F.hasSection())
    return false;

  // It's safe to outline from MF.
  return true;
}

bool YSXInstrInfo::isMBBSafeToOutlineFrom(MachineBasicBlock &MBB,
                                            unsigned &Flags) const {
  // More accurate safety checking is done in getOutliningCandidateInfo.
  return TargetInstrInfo::isMBBSafeToOutlineFrom(MBB, Flags);
}

// Enum values indicating how an outlined call should be constructed.
enum MachineOutlinerConstructionID {
  MachineOutlinerTailCall,
  MachineOutlinerDefault
};

bool YSXInstrInfo::shouldOutlineFromFunctionByDefault(
    MachineFunction &MF) const {
  return MF.getFunction().hasMinSize();
}

static bool isCandidatePatchable(const MachineBasicBlock &MBB) {
  const MachineFunction *MF = MBB.getParent();
  const Function &F = MF->getFunction();
  return F.getFnAttribute("fentry-call").getValueAsBool() ||
         F.hasFnAttribute("patchable-function-entry");
}

static bool isMIReadsReg(const MachineInstr &MI, const TargetRegisterInfo *TRI,
                         MCRegister RegNo) {
  return MI.readsRegister(RegNo, TRI) ||
         MI.getDesc().hasImplicitUseOfPhysReg(RegNo);
}

static bool isMIModifiesReg(const MachineInstr &MI,
                            const TargetRegisterInfo *TRI, MCRegister RegNo) {
  return MI.modifiesRegister(RegNo, TRI) ||
         MI.getDesc().hasImplicitDefOfPhysReg(RegNo);
}

static bool cannotInsertTailCall(const MachineBasicBlock &MBB) {
  if (!MBB.back().isReturn())
    return true;
  if (isCandidatePatchable(MBB))
    return true;

  // If the candidate reads the pre-set register
  // that can be used for expanding PseudoTAIL instruction,
  // then we cannot insert tail call.
  const TargetSubtargetInfo &STI = MBB.getParent()->getSubtarget();
  MCRegister TailExpandUseRegNo =
      YSXII::getTailExpandUseRegNo(STI.getFeatureBits());
  for (const MachineInstr &MI : MBB) {
    if (isMIReadsReg(MI, STI.getRegisterInfo(), TailExpandUseRegNo))
      return true;
    if (isMIModifiesReg(MI, STI.getRegisterInfo(), TailExpandUseRegNo))
      break;
  }
  return false;
}

static bool analyzeCandidate(outliner::Candidate &C) {
  // If last instruction is return then we can rely on
  // the verification already performed in the getOutliningTypeImpl.
  if (C.back().isReturn()) {
    assert(!cannotInsertTailCall(*C.getMBB()) &&
           "The candidate who uses return instruction must be outlined "
           "using tail call");
    return false;
  }

  // Filter out candidates where the X5 register (t0) can't be used to setup
  // the function call.
  const TargetRegisterInfo *TRI = C.getMF()->getSubtarget().getRegisterInfo();
  if (llvm::any_of(C, [TRI](const MachineInstr &MI) {
        return isMIModifiesReg(MI, TRI, YSX::X5);
      }))
    return true;

  return !C.isAvailableAcrossAndOutOfSeq(YSX::X5, *TRI);
}

std::optional<std::unique_ptr<outliner::OutlinedFunction>>
YSXInstrInfo::getOutliningCandidateInfo(
    const MachineModuleInfo &MMI,
    std::vector<outliner::Candidate> &RepeatedSequenceLocs,
    unsigned MinRepeats) const {

  // Analyze each candidate and erase the ones that are not viable.
  llvm::erase_if(RepeatedSequenceLocs, analyzeCandidate);

  // If the sequence doesn't have enough candidates left, then we're done.
  if (RepeatedSequenceLocs.size() < MinRepeats)
    return std::nullopt;

  // Each RepeatedSequenceLoc is identical.
  outliner::Candidate &Candidate = RepeatedSequenceLocs[0];
  unsigned InstrSizeCExt =
      Candidate.getMF()->getSubtarget<YSXSubtarget>().hasStdExtZca() ? 2 : 4;
  unsigned CallOverhead = 0, FrameOverhead = 0;

  // Count the number of CFI instructions in the candidate, if present.
  unsigned CFICount = 0;
  for (auto &I : Candidate) {
    if (I.isCFIInstruction())
      CFICount++;
  }

  // Ensure CFI coverage matches: comparing the number of CFIs in the candidate
  // with the total number of CFIs in the parent function for each candidate.
  // Outlining only a subset of a function’s CFIs would split the unwind state
  // across two code regions and lead to incorrect address offsets between the
  // outlined body and the remaining code. To preserve correct unwind info, we
  // only outline when all CFIs in the function can be outlined together.
  for (outliner::Candidate &C : RepeatedSequenceLocs) {
    std::vector<MCCFIInstruction> CFIInstructions =
        C.getMF()->getFrameInstructions();

    if (CFICount > 0 && CFICount != CFIInstructions.size())
      return std::nullopt;
  }

  MachineOutlinerConstructionID MOCI = MachineOutlinerDefault;
  if (Candidate.back().isReturn()) {
    MOCI = MachineOutlinerTailCall;
    // tail call = auipc + jalr in the worst case without linker relaxation.
    // FIXME: This code suggests the JALR can be compressed - how?
    CallOverhead = 4 + InstrSizeCExt;
    // Using tail call we move ret instruction from caller to callee.
    FrameOverhead = 0;
  } else {
    // call t0, function = 8 bytes.
    CallOverhead = 8;
    // jr t0 = 4 bytes, 2 bytes if compressed instructions are enabled.
    FrameOverhead = InstrSizeCExt;
  }

  // If we have CFI instructions, we can only outline if the outlined section
  // can be a tail call.
  if (MOCI != MachineOutlinerTailCall && CFICount > 0)
    return std::nullopt;

  for (auto &C : RepeatedSequenceLocs)
    C.setCallInfo(MOCI, CallOverhead);

  unsigned SequenceSize = 0;
  for (auto &MI : Candidate)
    SequenceSize += getInstSizeInBytes(MI);

  return std::make_unique<outliner::OutlinedFunction>(
      RepeatedSequenceLocs, SequenceSize, FrameOverhead, MOCI);
}

outliner::InstrType
YSXInstrInfo::getOutliningTypeImpl(const MachineModuleInfo &MMI,
                                     MachineBasicBlock::iterator &MBBI,
                                     unsigned Flags) const {
  MachineInstr &MI = *MBBI;
  MachineBasicBlock *MBB = MI.getParent();
  const TargetRegisterInfo *TRI =
      MBB->getParent()->getSubtarget().getRegisterInfo();
  const auto &F = MI.getMF()->getFunction();

  // We can only outline CFI instructions if we will tail call the outlined
  // function, or fix up the CFI offsets. Currently, CFI instructions are
  // outlined only if in a tail call.
  if (MI.isCFIInstruction())
    return outliner::InstrType::Legal;

  if (cannotInsertTailCall(*MBB) &&
      (MI.isReturn() || isMIModifiesReg(MI, TRI, YSX::X5)))
    return outliner::InstrType::Illegal;

  // Make sure the operands don't reference something unsafe.
  for (const auto &MO : MI.operands()) {

    // pcrel-hi and pcrel-lo can't put in separate sections, filter that out
    // if any possible.
    if (MO.getTargetFlags() == YSXII::MO_PCREL_LO &&
        (MI.getMF()->getTarget().getFunctionSections() || F.hasComdat() ||
         F.hasSection() || F.getSectionPrefix()))
      return outliner::InstrType::Illegal;
  }

  if (isLPAD(MI))
    return outliner::InstrType::Illegal;

  return outliner::InstrType::Legal;
}

void YSXInstrInfo::buildOutlinedFrame(
    MachineBasicBlock &MBB, MachineFunction &MF,
    const outliner::OutlinedFunction &OF) const {

  if (OF.FrameConstructionID == MachineOutlinerTailCall)
    return;

  MBB.addLiveIn(YSX::X5);

  // Add in a return instruction to the end of the outlined frame.
  MBB.insert(MBB.end(), BuildMI(MF, DebugLoc(), get(YSX::JALR))
      .addReg(YSX::X0, RegState::Define)
      .addReg(YSX::X5)
      .addImm(0));
}

MachineBasicBlock::iterator YSXInstrInfo::insertOutlinedCall(
    Module &M, MachineBasicBlock &MBB, MachineBasicBlock::iterator &It,
    MachineFunction &MF, outliner::Candidate &C) const {

  if (C.CallConstructionID == MachineOutlinerTailCall) {
    It = MBB.insert(It, BuildMI(MF, DebugLoc(), get(YSX::PseudoTAIL))
                            .addGlobalAddress(M.getNamedValue(MF.getName()),
                                              /*Offset=*/0, YSXII::MO_CALL));
    return It;
  }

  // Add in a call instruction to the outlined function at the given location.
  It = MBB.insert(It,
                  BuildMI(MF, DebugLoc(), get(YSX::PseudoCALLReg), YSX::X5)
                      .addGlobalAddress(M.getNamedValue(MF.getName()), 0,
                                        YSXII::MO_CALL));
  return It;
}

std::optional<RegImmPair> YSXInstrInfo::isAddImmediate(const MachineInstr &MI,
                                                         Register Reg) const {
  // TODO: Handle cases where Reg is a super- or sub-register of the
  // destination register.
  const MachineOperand &Op0 = MI.getOperand(0);
  if (!Op0.isReg() || Reg != Op0.getReg())
    return std::nullopt;

  // Don't consider ADDIW as a candidate because the caller may not be aware
  // of its sign extension behaviour.
  if (MI.getOpcode() == YSX::ADDI && MI.getOperand(1).isReg() &&
      MI.getOperand(2).isImm())
    return RegImmPair{MI.getOperand(1).getReg(), MI.getOperand(2).getImm()};

  return std::nullopt;
}

// MIR printer helper function to annotate Operands with a comment.
std::string YSXInstrInfo::createMIROperandComment(
    const MachineInstr &MI, const MachineOperand &Op, unsigned OpIdx,
    const TargetRegisterInfo *TRI) const {
  // Print a generic comment for this operand if there is one.
  std::string GenericComment =
      TargetInstrInfo::createMIROperandComment(MI, Op, OpIdx, TRI);
  if (!GenericComment.empty())
    return GenericComment;

  // If not, we must have an immediate operand.
  if (!Op.isImm())
    return std::string();

  const MCInstrDesc &Desc = MI.getDesc();
  if (OpIdx >= Desc.getNumOperands())
    return std::string();

  std::string Comment;
  raw_string_ostream OS(Comment);

  const MCOperandInfo &OpInfo = Desc.operands()[OpIdx];

  // Print the full VType operand of vsetvli/vsetivli instructions, and the SEW
  // operand of vector codegen pseudos.
  switch (OpInfo.OperandType) {
  case YSXOp::OPERAND_VTYPEI10:
  case YSXOp::OPERAND_VTYPEI11: {
    OS << Op.getImm();
    break;
  }
  case YSXOp::OPERAND_XSFMM_VTYPE: {
    OS << Op.getImm();
    break;
  }
  case YSXOp::OPERAND_XSFMM_TWIDEN: {
    unsigned Imm = Op.getImm();
    OS << "w" << Imm;
    break;
  }
  case YSXOp::OPERAND_SEW:
  case YSXOp::OPERAND_SEW_MASK: {
    OS << Op.getImm();
    break;
  }
  case YSXOp::OPERAND_VEC_POLICY:
    OS << Op.getImm();
    break;
  }

  return Comment;
}

// clang-format off
#define CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, LMUL)                                 \
  YSX::Pseudo##OP##_##LMUL

#define CASE_YSXVec_OPCODE_MASK_LMUL(OP, LMUL)                                   \
  YSX::Pseudo##OP##_##LMUL##_MASK

#define CASE_YSXVec_OPCODE_LMUL(OP, LMUL)                                        \
  CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, LMUL):                                      \
  case CASE_YSXVec_OPCODE_MASK_LMUL(OP, LMUL)

#define CASE_YSXVec_OPCODE_UNMASK_WIDEN(OP)                                      \
  CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, MF8):                                       \
  case CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, MF4):                                  \
  case CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, MF2):                                  \
  case CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, M1):                                   \
  case CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, M2):                                   \
  case CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, M4)

#define CASE_YSXVec_OPCODE_UNMASK(OP)                                            \
  CASE_YSXVec_OPCODE_UNMASK_WIDEN(OP):                                           \
  case CASE_YSXVec_OPCODE_UNMASK_LMUL(OP, M8)

#define CASE_YSXVec_OPCODE_MASK_WIDEN(OP)                                        \
  CASE_YSXVec_OPCODE_MASK_LMUL(OP, MF8):                                         \
  case CASE_YSXVec_OPCODE_MASK_LMUL(OP, MF4):                                    \
  case CASE_YSXVec_OPCODE_MASK_LMUL(OP, MF2):                                    \
  case CASE_YSXVec_OPCODE_MASK_LMUL(OP, M1):                                     \
  case CASE_YSXVec_OPCODE_MASK_LMUL(OP, M2):                                     \
  case CASE_YSXVec_OPCODE_MASK_LMUL(OP, M4)

#define CASE_YSXVec_OPCODE_MASK(OP)                                              \
  CASE_YSXVec_OPCODE_MASK_WIDEN(OP):                                             \
  case CASE_YSXVec_OPCODE_MASK_LMUL(OP, M8)

#define CASE_YSXVec_OPCODE_WIDEN(OP)                                             \
  CASE_YSXVec_OPCODE_UNMASK_WIDEN(OP):                                           \
  case CASE_YSXVec_OPCODE_MASK_WIDEN(OP)

#define CASE_YSXVec_OPCODE(OP)                                                   \
  CASE_YSXVec_OPCODE_UNMASK(OP):                                                 \
  case CASE_YSXVec_OPCODE_MASK(OP)
// clang-format on

// clang-format off
#define CASE_VMA_OPCODE_COMMON(OP, TYPE, LMUL)                                 \
  YSX::PseudoV##OP##_##TYPE##_##LMUL

#define CASE_VMA_OPCODE_LMULS(OP, TYPE)                                        \
  CASE_VMA_OPCODE_COMMON(OP, TYPE, MF8):                                       \
  case CASE_VMA_OPCODE_COMMON(OP, TYPE, MF4):                                  \
  case CASE_VMA_OPCODE_COMMON(OP, TYPE, MF2):                                  \
  case CASE_VMA_OPCODE_COMMON(OP, TYPE, M1):                                   \
  case CASE_VMA_OPCODE_COMMON(OP, TYPE, M2):                                   \
  case CASE_VMA_OPCODE_COMMON(OP, TYPE, M4):                                   \
  case CASE_VMA_OPCODE_COMMON(OP, TYPE, M8)

// VFMA instructions are SEW specific.
#define CASE_VFMA_OPCODE_COMMON(OP, TYPE, LMUL, SEW)                           \
  YSX::PseudoV##OP##_##TYPE##_##LMUL##_##SEW

#define CASE_VFMA_OPCODE_LMULS_M1(OP, TYPE, SEW)                               \
  CASE_VFMA_OPCODE_COMMON(OP, TYPE, M1, SEW):                                  \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, M2, SEW):                             \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, M4, SEW):                             \
  case CASE_VFMA_OPCODE_COMMON(OP, TYPE, M8, SEW)

#define CASE_VFMA_OPCODE_LMULS_MF2(OP, TYPE, SEW)                              \
  CASE_VFMA_OPCODE_COMMON(OP, TYPE, MF2, SEW):                                 \
  case CASE_VFMA_OPCODE_LMULS_M1(OP, TYPE, SEW)

#define CASE_VFMA_OPCODE_LMULS_MF4(OP, TYPE, SEW)                              \
  CASE_VFMA_OPCODE_COMMON(OP, TYPE, MF4, SEW):                                 \
  case CASE_VFMA_OPCODE_LMULS_MF2(OP, TYPE, SEW)

#define CASE_VFMA_OPCODE_VV(OP)                                                \
  CASE_VFMA_OPCODE_LMULS_MF4(OP, VV, E16):                                     \
  case CASE_VFMA_OPCODE_LMULS_MF4(OP##_ALT, VV, E16):                          \
  case CASE_VFMA_OPCODE_LMULS_MF2(OP, VV, E32):                                \
  case CASE_VFMA_OPCODE_LMULS_M1(OP, VV, E64)

#define CASE_VFMA_SPLATS(OP)                                                   \
  CASE_VFMA_OPCODE_LMULS_MF4(OP, VFPR16, E16):                                 \
  case CASE_VFMA_OPCODE_LMULS_MF4(OP##_ALT, VFPR16, E16):                      \
  case CASE_VFMA_OPCODE_LMULS_MF2(OP, VFPR32, E32):                            \
  case CASE_VFMA_OPCODE_LMULS_M1(OP, VFPR64, E64)
// clang-format on

bool YSXInstrInfo::findCommutedOpIndices(const MachineInstr &MI,
                                           unsigned &SrcOpIdx1,
                                           unsigned &SrcOpIdx2) const {
  const MCInstrDesc &Desc = MI.getDesc();
  if (!Desc.isCommutable())
    return false;

#if 0
  switch (MI.getOpcode()) {
  case YSX::TH_MVEQZ:
  case YSX::TH_MVNEZ:
    // We can't commute operands if operand 2 (i.e., rs1 in
    // mveqz/mvnez rd,rs1,rs2) is the zero-register (as it is
    // not valid as the in/out-operand 1).
    if (MI.getOperand(2).getReg() == YSX::X0)
      return false;
    // Operands 1 and 2 are commutable, if we switch the opcode.
    return fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, 1, 2);
  case YSX::QC_SELECTIEQ:
  case YSX::QC_SELECTINE:
  case YSX::QC_SELECTIIEQ:
  case YSX::QC_SELECTIINE:
    return fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, 1, 2);
  case YSX::QC_MVEQ:
  case YSX::QC_MVNE:
  case YSX::QC_MVLT:
  case YSX::QC_MVGE:
  case YSX::QC_MVLTU:
  case YSX::QC_MVGEU:
  case YSX::QC_MVEQI:
  case YSX::QC_MVNEI:
  case YSX::QC_MVLTI:
  case YSX::QC_MVGEI:
  case YSX::QC_MVLTUI:
  case YSX::QC_MVGEUI:
    return fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, 1, 4);
  case YSX::TH_MULA:
  case YSX::TH_MULAW:
  case YSX::TH_MULAH:
  case YSX::TH_MULS:
  case YSX::TH_MULSW:
  case YSX::TH_MULSH:
    // Operands 2 and 3 are commutable.
    return fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, 2, 3);
  case YSX::PseudoCCMOVGPRNoX0:
  case YSX::PseudoCCMOVGPR:
    // Operands 4 and 5 are commutable.
    return fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, 4, 5);
  case CASE_YSXVec_OPCODE(VADD_VV):
  case CASE_YSXVec_OPCODE(VAND_VV):
  case CASE_YSXVec_OPCODE(VOR_VV):
  case CASE_YSXVec_OPCODE(VXOR_VV):
  case CASE_YSXVec_OPCODE_MASK(VMSEQ_VV):
  case CASE_YSXVec_OPCODE_MASK(VMSNE_VV):
  case CASE_YSXVec_OPCODE(VMIN_VV):
  case CASE_YSXVec_OPCODE(VMINU_VV):
  case CASE_YSXVec_OPCODE(VMAX_VV):
  case CASE_YSXVec_OPCODE(VMAXU_VV):
  case CASE_YSXVec_OPCODE(VMUL_VV):
  case CASE_YSXVec_OPCODE(VMULH_VV):
  case CASE_YSXVec_OPCODE(VMULHU_VV):
  case CASE_YSXVec_OPCODE_WIDEN(VWADD_VV):
  case CASE_YSXVec_OPCODE_WIDEN(VWADDU_VV):
  case CASE_YSXVec_OPCODE_WIDEN(VWMUL_VV):
  case CASE_YSXVec_OPCODE_WIDEN(VWMULU_VV):
  case CASE_YSXVec_OPCODE_WIDEN(VWMACC_VV):
  case CASE_YSXVec_OPCODE_WIDEN(VWMACCU_VV):
  case CASE_YSXVec_OPCODE_UNMASK(VADC_VVM):
  case CASE_YSXVec_OPCODE(VSADD_VV):
  case CASE_YSXVec_OPCODE(VSADDU_VV):
  case CASE_YSXVec_OPCODE(VAADD_VV):
  case CASE_YSXVec_OPCODE(VAADDU_VV):
  case CASE_YSXVec_OPCODE(VSMUL_VV):
    // Operands 2 and 3 are commutable.
    return fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, 2, 3);
  case CASE_VFMA_SPLATS(FMADD):
  case CASE_VFMA_SPLATS(FMSUB):
  case CASE_VFMA_SPLATS(FMACC):
  case CASE_VFMA_SPLATS(FMSAC):
  case CASE_VFMA_SPLATS(FNMADD):
  case CASE_VFMA_SPLATS(FNMSUB):
  case CASE_VFMA_SPLATS(FNMACC):
  case CASE_VFMA_SPLATS(FNMSAC):
  case CASE_VFMA_OPCODE_VV(FMACC):
  case CASE_VFMA_OPCODE_VV(FMSAC):
  case CASE_VFMA_OPCODE_VV(FNMACC):
  case CASE_VFMA_OPCODE_VV(FNMSAC):
  case CASE_VMA_OPCODE_LMULS(MADD, VX):
  case CASE_VMA_OPCODE_LMULS(NMSUB, VX):
  case CASE_VMA_OPCODE_LMULS(MACC, VX):
  case CASE_VMA_OPCODE_LMULS(NMSAC, VX):
  case CASE_VMA_OPCODE_LMULS(MACC, VV):
  case CASE_VMA_OPCODE_LMULS(NMSAC, VV): {
    // If the tail policy is undisturbed we can't commute.
    assert(YSXII::hasVecPolicyOp(MI.getDesc().TSFlags));
    if ((MI.getOperand(YSXII::getVecPolicyOpNum(MI.getDesc())).getImm() &
         1) == 0)
      return false;

    // For these instructions we can only swap operand 1 and operand 3 by
    // changing the opcode.
    unsigned CommutableOpIdx1 = 1;
    unsigned CommutableOpIdx2 = 3;
    if (!fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, CommutableOpIdx1,
                              CommutableOpIdx2))
      return false;
    return true;
  }
  case CASE_VFMA_OPCODE_VV(FMADD):
  case CASE_VFMA_OPCODE_VV(FMSUB):
  case CASE_VFMA_OPCODE_VV(FNMADD):
  case CASE_VFMA_OPCODE_VV(FNMSUB):
  case CASE_VMA_OPCODE_LMULS(MADD, VV):
  case CASE_VMA_OPCODE_LMULS(NMSUB, VV): {
    // If the tail policy is undisturbed we can't commute.
    assert(YSXII::hasVecPolicyOp(MI.getDesc().TSFlags));
    if ((MI.getOperand(YSXII::getVecPolicyOpNum(MI.getDesc())).getImm() &
         1) == 0)
      return false;

    // For these instructions we have more freedom. We can commute with the
    // other multiplicand or with the addend/subtrahend/minuend.

    // Any fixed operand must be from source 1, 2 or 3.
    if (SrcOpIdx1 != CommuteAnyOperandIndex && SrcOpIdx1 > 3)
      return false;
    if (SrcOpIdx2 != CommuteAnyOperandIndex && SrcOpIdx2 > 3)
      return false;

    // It both ops are fixed one must be the tied source.
    if (SrcOpIdx1 != CommuteAnyOperandIndex &&
        SrcOpIdx2 != CommuteAnyOperandIndex && SrcOpIdx1 != 1 && SrcOpIdx2 != 1)
      return false;

    // Look for two different register operands assumed to be commutable
    // regardless of the FMA opcode. The FMA opcode is adjusted later if
    // needed.
    if (SrcOpIdx1 == CommuteAnyOperandIndex ||
        SrcOpIdx2 == CommuteAnyOperandIndex) {
      // At least one of operands to be commuted is not specified and
      // this method is free to choose appropriate commutable operands.
      unsigned CommutableOpIdx1 = SrcOpIdx1;
      if (SrcOpIdx1 == SrcOpIdx2) {
        // Both of operands are not fixed. Set one of commutable
        // operands to the tied source.
        CommutableOpIdx1 = 1;
      } else if (SrcOpIdx1 == CommuteAnyOperandIndex) {
        // Only one of the operands is not fixed.
        CommutableOpIdx1 = SrcOpIdx2;
      }

      // CommutableOpIdx1 is well defined now. Let's choose another commutable
      // operand and assign its index to CommutableOpIdx2.
      unsigned CommutableOpIdx2;
      if (CommutableOpIdx1 != 1) {
        // If we haven't already used the tied source, we must use it now.
        CommutableOpIdx2 = 1;
      } else {
        Register Op1Reg = MI.getOperand(CommutableOpIdx1).getReg();

        // The commuted operands should have different registers.
        // Otherwise, the commute transformation does not change anything and
        // is useless. We use this as a hint to make our decision.
        if (Op1Reg != MI.getOperand(2).getReg())
          CommutableOpIdx2 = 2;
        else
          CommutableOpIdx2 = 3;
      }

      // Assign the found pair of commutable indices to SrcOpIdx1 and
      // SrcOpIdx2 to return those values.
      if (!fixCommutedOpIndices(SrcOpIdx1, SrcOpIdx2, CommutableOpIdx1,
                                CommutableOpIdx2))
        return false;
    }

    return true;
  }
  }
#endif

  return TargetInstrInfo::findCommutedOpIndices(MI, SrcOpIdx1, SrcOpIdx2);
}

// clang-format off
#define CASE_VMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, LMUL)                \
  case YSX::PseudoV##OLDOP##_##TYPE##_##LMUL:                                \
    Opc = YSX::PseudoV##NEWOP##_##TYPE##_##LMUL;                             \
    break;

#define CASE_VMA_CHANGE_OPCODE_LMULS(OLDOP, NEWOP, TYPE)                       \
  CASE_VMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, MF8)                       \
  CASE_VMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, MF4)                       \
  CASE_VMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, MF2)                       \
  CASE_VMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M1)                        \
  CASE_VMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M2)                        \
  CASE_VMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M4)                        \
  CASE_VMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M8)

// VFMA depends on SEW.
#define CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, LMUL, SEW)          \
  case YSX::PseudoV##OLDOP##_##TYPE##_##LMUL##_##SEW:                        \
    Opc = YSX::PseudoV##NEWOP##_##TYPE##_##LMUL##_##SEW;                     \
    break;

#define CASE_VFMA_CHANGE_OPCODE_LMULS_M1(OLDOP, NEWOP, TYPE, SEW)              \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M1, SEW)                  \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M2, SEW)                  \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M4, SEW)                  \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, M8, SEW)

#define CASE_VFMA_CHANGE_OPCODE_LMULS_MF2(OLDOP, NEWOP, TYPE, SEW)             \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, MF2, SEW)                 \
  CASE_VFMA_CHANGE_OPCODE_LMULS_M1(OLDOP, NEWOP, TYPE, SEW)

#define CASE_VFMA_CHANGE_OPCODE_LMULS_MF4(OLDOP, NEWOP, TYPE, SEW)             \
  CASE_VFMA_CHANGE_OPCODE_COMMON(OLDOP, NEWOP, TYPE, MF4, SEW)                 \
  CASE_VFMA_CHANGE_OPCODE_LMULS_MF2(OLDOP, NEWOP, TYPE, SEW)

#define CASE_VFMA_CHANGE_OPCODE_VV(OLDOP, NEWOP)                               \
  CASE_VFMA_CHANGE_OPCODE_LMULS_MF4(OLDOP, NEWOP, VV, E16)                     \
  CASE_VFMA_CHANGE_OPCODE_LMULS_MF4(OLDOP##_ALT, NEWOP##_ALT, VV, E16)         \
  CASE_VFMA_CHANGE_OPCODE_LMULS_MF2(OLDOP, NEWOP, VV, E32)                     \
  CASE_VFMA_CHANGE_OPCODE_LMULS_M1(OLDOP, NEWOP, VV, E64)

#define CASE_VFMA_CHANGE_OPCODE_SPLATS(OLDOP, NEWOP)                           \
  CASE_VFMA_CHANGE_OPCODE_LMULS_MF4(OLDOP, NEWOP, VFPR16, E16)                 \
  CASE_VFMA_CHANGE_OPCODE_LMULS_MF4(OLDOP##_ALT, NEWOP##_ALT, VFPR16, E16)     \
  CASE_VFMA_CHANGE_OPCODE_LMULS_MF2(OLDOP, NEWOP, VFPR32, E32)                 \
  CASE_VFMA_CHANGE_OPCODE_LMULS_M1(OLDOP, NEWOP, VFPR64, E64)
// clang-format on

MachineInstr *YSXInstrInfo::commuteInstructionImpl(MachineInstr &MI,
                                                     bool NewMI,
                                                     unsigned OpIdx1,
                                                     unsigned OpIdx2) const {
  return TargetInstrInfo::commuteInstructionImpl(MI, NewMI, OpIdx1, OpIdx2);

#if 0
  auto cloneIfNew = [NewMI](MachineInstr &MI) -> MachineInstr & {
    if (NewMI)
      return *MI.getParent()->getParent()->CloneMachineInstr(&MI);
    return MI;
  };

  switch (MI.getOpcode()) {
  case YSX::TH_MVEQZ:
  case YSX::TH_MVNEZ: {
    auto &WorkingMI = cloneIfNew(MI);
    WorkingMI.setDesc(get(MI.getOpcode() == YSX::TH_MVEQZ ? YSX::TH_MVNEZ
                                                            : YSX::TH_MVEQZ));
    return TargetInstrInfo::commuteInstructionImpl(WorkingMI, false, OpIdx1,
                                                   OpIdx2);
  }
  case YSX::QC_SELECTIEQ:
  case YSX::QC_SELECTINE:
  case YSX::QC_SELECTIIEQ:
  case YSX::QC_SELECTIINE:
    return TargetInstrInfo::commuteInstructionImpl(MI, NewMI, OpIdx1, OpIdx2);
  case YSX::QC_MVEQ:
  case YSX::QC_MVNE:
  case YSX::QC_MVLT:
  case YSX::QC_MVGE:
  case YSX::QC_MVLTU:
  case YSX::QC_MVGEU:
  case YSX::QC_MVEQI:
  case YSX::QC_MVNEI:
  case YSX::QC_MVLTI:
  case YSX::QC_MVGEI:
  case YSX::QC_MVLTUI:
  case YSX::QC_MVGEUI: {
    auto &WorkingMI = cloneIfNew(MI);
    WorkingMI.setDesc(get(getInverseXRemovedQcicmOpcode(MI.getOpcode())));
    return TargetInstrInfo::commuteInstructionImpl(WorkingMI, false, OpIdx1,
                                                   OpIdx2);
  }
  case YSX::PseudoCCMOVGPRNoX0:
  case YSX::PseudoCCMOVGPR: {
    // CCMOV can be commuted by inverting the condition.
    auto CC = static_cast<YSXCC::CondCode>(MI.getOperand(3).getImm());
    CC = YSXCC::getInverseBranchCondition(CC);
    auto &WorkingMI = cloneIfNew(MI);
    WorkingMI.getOperand(3).setImm(CC);
    return TargetInstrInfo::commuteInstructionImpl(WorkingMI, /*NewMI*/ false,
                                                   OpIdx1, OpIdx2);
  }
  case CASE_VFMA_SPLATS(FMACC):
  case CASE_VFMA_SPLATS(FMADD):
  case CASE_VFMA_SPLATS(FMSAC):
  case CASE_VFMA_SPLATS(FMSUB):
  case CASE_VFMA_SPLATS(FNMACC):
  case CASE_VFMA_SPLATS(FNMADD):
  case CASE_VFMA_SPLATS(FNMSAC):
  case CASE_VFMA_SPLATS(FNMSUB):
  case CASE_VFMA_OPCODE_VV(FMACC):
  case CASE_VFMA_OPCODE_VV(FMSAC):
  case CASE_VFMA_OPCODE_VV(FNMACC):
  case CASE_VFMA_OPCODE_VV(FNMSAC):
  case CASE_VMA_OPCODE_LMULS(MADD, VX):
  case CASE_VMA_OPCODE_LMULS(NMSUB, VX):
  case CASE_VMA_OPCODE_LMULS(MACC, VX):
  case CASE_VMA_OPCODE_LMULS(NMSAC, VX):
  case CASE_VMA_OPCODE_LMULS(MACC, VV):
  case CASE_VMA_OPCODE_LMULS(NMSAC, VV): {
    // It only make sense to toggle these between clobbering the
    // addend/subtrahend/minuend one of the multiplicands.
    assert((OpIdx1 == 1 || OpIdx2 == 1) && "Unexpected opcode index");
    assert((OpIdx1 == 3 || OpIdx2 == 3) && "Unexpected opcode index");
    unsigned Opc;
    switch (MI.getOpcode()) {
      default:
        llvm_unreachable("Unexpected opcode");
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FMACC, FMADD)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FMADD, FMACC)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FMSAC, FMSUB)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FMSUB, FMSAC)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FNMACC, FNMADD)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FNMADD, FNMACC)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FNMSAC, FNMSUB)
      CASE_VFMA_CHANGE_OPCODE_SPLATS(FNMSUB, FNMSAC)
      CASE_VFMA_CHANGE_OPCODE_VV(FMACC, FMADD)
      CASE_VFMA_CHANGE_OPCODE_VV(FMSAC, FMSUB)
      CASE_VFMA_CHANGE_OPCODE_VV(FNMACC, FNMADD)
      CASE_VFMA_CHANGE_OPCODE_VV(FNMSAC, FNMSUB)
      CASE_VMA_CHANGE_OPCODE_LMULS(MACC, MADD, VX)
      CASE_VMA_CHANGE_OPCODE_LMULS(MADD, MACC, VX)
      CASE_VMA_CHANGE_OPCODE_LMULS(NMSAC, NMSUB, VX)
      CASE_VMA_CHANGE_OPCODE_LMULS(NMSUB, NMSAC, VX)
      CASE_VMA_CHANGE_OPCODE_LMULS(MACC, MADD, VV)
      CASE_VMA_CHANGE_OPCODE_LMULS(NMSAC, NMSUB, VV)
    }

    auto &WorkingMI = cloneIfNew(MI);
    WorkingMI.setDesc(get(Opc));
    return TargetInstrInfo::commuteInstructionImpl(WorkingMI, /*NewMI=*/false,
                                                   OpIdx1, OpIdx2);
  }
  case CASE_VFMA_OPCODE_VV(FMADD):
  case CASE_VFMA_OPCODE_VV(FMSUB):
  case CASE_VFMA_OPCODE_VV(FNMADD):
  case CASE_VFMA_OPCODE_VV(FNMSUB):
  case CASE_VMA_OPCODE_LMULS(MADD, VV):
  case CASE_VMA_OPCODE_LMULS(NMSUB, VV): {
    assert((OpIdx1 == 1 || OpIdx2 == 1) && "Unexpected opcode index");
    // If one of the operands, is the addend we need to change opcode.
    // Otherwise we're just swapping 2 of the multiplicands.
    if (OpIdx1 == 3 || OpIdx2 == 3) {
      unsigned Opc;
      switch (MI.getOpcode()) {
        default:
          llvm_unreachable("Unexpected opcode");
        CASE_VFMA_CHANGE_OPCODE_VV(FMADD, FMACC)
        CASE_VFMA_CHANGE_OPCODE_VV(FMSUB, FMSAC)
        CASE_VFMA_CHANGE_OPCODE_VV(FNMADD, FNMACC)
        CASE_VFMA_CHANGE_OPCODE_VV(FNMSUB, FNMSAC)
        CASE_VMA_CHANGE_OPCODE_LMULS(MADD, MACC, VV)
        CASE_VMA_CHANGE_OPCODE_LMULS(NMSUB, NMSAC, VV)
      }

      auto &WorkingMI = cloneIfNew(MI);
      WorkingMI.setDesc(get(Opc));
      return TargetInstrInfo::commuteInstructionImpl(WorkingMI, /*NewMI=*/false,
                                                     OpIdx1, OpIdx2);
    }
    // Let the default code handle it.
    break;
  }
  }

  return TargetInstrInfo::commuteInstructionImpl(MI, NewMI, OpIdx1, OpIdx2);
#endif
}

#undef CASE_VMA_CHANGE_OPCODE_COMMON
#undef CASE_VMA_CHANGE_OPCODE_LMULS
#undef CASE_VFMA_CHANGE_OPCODE_COMMON
#undef CASE_VFMA_CHANGE_OPCODE_LMULS_M1
#undef CASE_VFMA_CHANGE_OPCODE_LMULS_MF2
#undef CASE_VFMA_CHANGE_OPCODE_LMULS_MF4
#undef CASE_VFMA_CHANGE_OPCODE_VV
#undef CASE_VFMA_CHANGE_OPCODE_SPLATS

#undef CASE_YSXVec_OPCODE_UNMASK_LMUL
#undef CASE_YSXVec_OPCODE_MASK_LMUL
#undef CASE_YSXVec_OPCODE_LMUL
#undef CASE_YSXVec_OPCODE_UNMASK_WIDEN
#undef CASE_YSXVec_OPCODE_UNMASK
#undef CASE_YSXVec_OPCODE_MASK_WIDEN
#undef CASE_YSXVec_OPCODE_MASK
#undef CASE_YSXVec_OPCODE_WIDEN
#undef CASE_YSXVec_OPCODE

#undef CASE_VMA_OPCODE_COMMON
#undef CASE_VMA_OPCODE_LMULS
#undef CASE_VFMA_OPCODE_COMMON
#undef CASE_VFMA_OPCODE_LMULS_M1
#undef CASE_VFMA_OPCODE_LMULS_MF2
#undef CASE_VFMA_OPCODE_LMULS_MF4
#undef CASE_VFMA_OPCODE_VV
#undef CASE_VFMA_SPLATS

bool YSXInstrInfo::simplifyInstruction(MachineInstr &MI) const {
  switch (MI.getOpcode()) {
  default:
    break;
  case YSX::ADD:
  case YSX::OR:
  case YSX::XOR:
    // Normalize (so we hit the next if clause).
    // add/[x]or rd, zero, rs => add/[x]or rd, rs, zero
    if (MI.getOperand(1).getReg() == YSX::X0)
      commuteInstruction(MI);
    // add/[x]or rd, rs, zero => addi rd, rs, 0
    if (MI.getOperand(2).getReg() == YSX::X0) {
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    // xor rd, rs, rs => addi rd, zero, 0
    if (MI.getOpcode() == YSX::XOR &&
        MI.getOperand(1).getReg() == MI.getOperand(2).getReg()) {
      MI.getOperand(1).setReg(YSX::X0);
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::ORI:
  case YSX::XORI:
    // [x]ori rd, zero, N => addi rd, zero, N
    if (MI.getOperand(1).getReg() == YSX::X0) {
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::SUB:
    // sub rd, rs, zero => addi rd, rs, 0
    if (MI.getOperand(2).getReg() == YSX::X0) {
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::SUBW:
    // subw rd, rs, zero => addiw rd, rs, 0
    if (MI.getOperand(2).getReg() == YSX::X0) {
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDIW));
      return true;
    }
    break;
  case YSX::ADDW:
    // Normalize (so we hit the next if clause).
    // addw rd, zero, rs => addw rd, rs, zero
    if (MI.getOperand(1).getReg() == YSX::X0)
      commuteInstruction(MI);
    // addw rd, rs, zero => addiw rd, rs, 0
    if (MI.getOperand(2).getReg() == YSX::X0) {
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDIW));
      return true;
    }
    break;
  case YSX::AND:
  case YSX::MUL:
  case YSX::MULH:
  case YSX::MULHSU:
  case YSX::MULHU:
  case YSX::MULW:
    // and rd, zero, rs => addi rd, zero, 0
    // mul* rd, zero, rs => addi rd, zero, 0
    // and rd, rs, zero => addi rd, zero, 0
    // mul* rd, rs, zero => addi rd, zero, 0
    if (MI.getOperand(1).getReg() == YSX::X0 ||
        MI.getOperand(2).getReg() == YSX::X0) {
      MI.getOperand(1).setReg(YSX::X0);
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::ANDI:
    // andi rd, zero, C => addi rd, zero, 0
    if (MI.getOperand(1).getReg() == YSX::X0) {
      MI.getOperand(2).setImm(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::SLL:
  case YSX::SRL:
  case YSX::SRA:
    // shift rd, zero, rs => addi rd, zero, 0
    if (MI.getOperand(1).getReg() == YSX::X0) {
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    // shift rd, rs, zero => addi rd, rs, 0
    if (MI.getOperand(2).getReg() == YSX::X0) {
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::SLLW:
  case YSX::SRLW:
  case YSX::SRAW:
    // shiftw rd, zero, rs => addi rd, zero, 0
    if (MI.getOperand(1).getReg() == YSX::X0) {
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::SLLI:
  case YSX::SRLI:
  case YSX::SRAI:
  case YSX::SLLIW:
  case YSX::SRLIW:
  case YSX::SRAIW:
    // shiftimm rd, zero, N => addi rd, zero, 0
    if (MI.getOperand(1).getReg() == YSX::X0) {
      MI.getOperand(2).setImm(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::SLTU:
    // sltu rd, zero, zero => addi rd, zero, 0
    if (MI.getOperand(1).getReg() == YSX::X0 &&
        MI.getOperand(2).getReg() == YSX::X0) {
      MI.getOperand(2).ChangeToImmediate(0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::SLTIU:
    // sltiu rd, zero, NZC => addi rd, zero, 1
    // sltiu rd, zero, 0 => addi rd, zero, 0
    if (MI.getOperand(1).getReg() == YSX::X0) {
      MI.getOperand(2).setImm(MI.getOperand(2).getImm() != 0);
      MI.setDesc(get(YSX::ADDI));
      return true;
    }
    break;
  case YSX::BEQ:
  case YSX::BNE:
    // b{eq,ne} zero, rs, imm => b{eq,ne} rs, zero, imm
    if (MI.getOperand(0).getReg() == YSX::X0) {
      MachineOperand MO0 = MI.getOperand(0);
      MI.removeOperand(0);
      MI.insert(MI.operands_begin() + 1, {MO0});
    }
    break;
  case YSX::BLTU:
    // bltu zero, rs, imm => bne rs, zero, imm
    if (MI.getOperand(0).getReg() == YSX::X0) {
      MachineOperand MO0 = MI.getOperand(0);
      MI.removeOperand(0);
      MI.insert(MI.operands_begin() + 1, {MO0});
      MI.setDesc(get(YSX::BNE));
    }
    break;
  case YSX::BGEU:
    // bgeu zero, rs, imm => beq rs, zero, imm
    if (MI.getOperand(0).getReg() == YSX::X0) {
      MachineOperand MO0 = MI.getOperand(0);
      MI.removeOperand(0);
      MI.insert(MI.operands_begin() + 1, {MO0});
      MI.setDesc(get(YSX::BEQ));
    }
    break;
  }
  return false;
}

// clang-format off
#define CASE_WIDEOP_OPCODE_COMMON(OP, LMUL)                                    \
  YSX::PseudoV##OP##_##LMUL##_TIED

#define CASE_WIDEOP_OPCODE_LMULS(OP)                                           \
  CASE_WIDEOP_OPCODE_COMMON(OP, MF8):                                          \
  case CASE_WIDEOP_OPCODE_COMMON(OP, MF4):                                     \
  case CASE_WIDEOP_OPCODE_COMMON(OP, MF2):                                     \
  case CASE_WIDEOP_OPCODE_COMMON(OP, M1):                                      \
  case CASE_WIDEOP_OPCODE_COMMON(OP, M2):                                      \
  case CASE_WIDEOP_OPCODE_COMMON(OP, M4)

#define CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, LMUL)                             \
  case YSX::PseudoV##OP##_##LMUL##_TIED:                                     \
    NewOpc = YSX::PseudoV##OP##_##LMUL;                                      \
    break;

#define CASE_WIDEOP_CHANGE_OPCODE_LMULS(OP)                                    \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF8)                                    \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF4)                                    \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF2)                                    \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, M1)                                     \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, M2)                                     \
  CASE_WIDEOP_CHANGE_OPCODE_COMMON(OP, M4)

// FP Widening Ops may by SEW aware. Create SEW aware cases for these cases.
#define CASE_FP_WIDEOP_OPCODE_COMMON(OP, LMUL, SEW)                            \
  YSX::PseudoV##OP##_##LMUL##_##SEW##_TIED

#define CASE_FP_WIDEOP_OPCODE_LMULS(OP)                                        \
  CASE_FP_WIDEOP_OPCODE_COMMON(OP, MF4, E16):                                  \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, MF2, E16):                             \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, MF2, E32):                             \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M1, E16):                              \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M1, E32):                              \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M2, E16):                              \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M2, E32):                              \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M4, E16):                              \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M4, E32)                               \

#define CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, LMUL, SEW)                     \
  case YSX::PseudoV##OP##_##LMUL##_##SEW##_TIED:                             \
    NewOpc = YSX::PseudoV##OP##_##LMUL##_##SEW;                              \
    break;

#define CASE_FP_WIDEOP_CHANGE_OPCODE_LMULS(OP)                                 \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF4, E16)                            \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF2, E16)                            \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF2, E32)                            \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M1, E16)                             \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M1, E32)                             \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M2, E16)                             \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M2, E32)                             \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M4, E16)                             \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M4, E32)                             \

#define CASE_FP_WIDEOP_OPCODE_LMULS_ALT(OP)                                    \
  CASE_FP_WIDEOP_OPCODE_COMMON(OP, MF4, E16):                                  \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, MF2, E16):                             \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M1, E16):                              \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M2, E16):                              \
  case CASE_FP_WIDEOP_OPCODE_COMMON(OP, M4, E16)

#define CASE_FP_WIDEOP_CHANGE_OPCODE_LMULS_ALT(OP)                             \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF4, E16)                            \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, MF2, E16)                            \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M1, E16)                             \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M2, E16)                             \
  CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON(OP, M4, E16)
// clang-format on

MachineInstr *YSXInstrInfo::convertToThreeAddress(MachineInstr &MI,
                                                    LiveVariables *LV,
                                                    LiveIntervals *LIS) const {
#if 0
  MachineInstrBuilder MIB;
  switch (MI.getOpcode()) {
  default:
    return nullptr;
  case CASE_FP_WIDEOP_OPCODE_LMULS_ALT(FWADD_ALT_WV):
  case CASE_FP_WIDEOP_OPCODE_LMULS_ALT(FWSUB_ALT_WV):
  case CASE_FP_WIDEOP_OPCODE_LMULS(FWADD_WV):
  case CASE_FP_WIDEOP_OPCODE_LMULS(FWSUB_WV): {
    assert(YSXII::hasVecPolicyOp(MI.getDesc().TSFlags) &&
           MI.getNumExplicitOperands() == 7 &&
           "Expect 7 explicit operands rd, rs2, rs1, rm, vl, sew, policy");
    // If the tail policy is undisturbed we can't convert.
    if ((MI.getOperand(YSXII::getVecPolicyOpNum(MI.getDesc())).getImm() &
         1) == 0)
      return nullptr;
    // clang-format off
    unsigned NewOpc;
    switch (MI.getOpcode()) {
    default:
      llvm_unreachable("Unexpected opcode");
    CASE_FP_WIDEOP_CHANGE_OPCODE_LMULS(FWADD_WV)
    CASE_FP_WIDEOP_CHANGE_OPCODE_LMULS(FWSUB_WV)
    CASE_FP_WIDEOP_CHANGE_OPCODE_LMULS_ALT(FWADD_ALT_WV)
    CASE_FP_WIDEOP_CHANGE_OPCODE_LMULS_ALT(FWSUB_ALT_WV)
    }
    // clang-format on

    MachineBasicBlock &MBB = *MI.getParent();
    MIB = BuildMI(MBB, MI, MI.getDebugLoc(), get(NewOpc))
              .add(MI.getOperand(0))
              .addReg(MI.getOperand(0).getReg(), RegState::Undef)
              .add(MI.getOperand(1))
              .add(MI.getOperand(2))
              .add(MI.getOperand(3))
              .add(MI.getOperand(4))
              .add(MI.getOperand(5))
              .add(MI.getOperand(6));
    break;
  }
  case CASE_WIDEOP_OPCODE_LMULS(WADD_WV):
  case CASE_WIDEOP_OPCODE_LMULS(WADDU_WV):
  case CASE_WIDEOP_OPCODE_LMULS(WSUB_WV):
  case CASE_WIDEOP_OPCODE_LMULS(WSUBU_WV): {
    // If the tail policy is undisturbed we can't convert.
    assert(YSXII::hasVecPolicyOp(MI.getDesc().TSFlags) &&
           MI.getNumExplicitOperands() == 6);
    if ((MI.getOperand(YSXII::getVecPolicyOpNum(MI.getDesc())).getImm() &
         1) == 0)
      return nullptr;

    // clang-format off
    unsigned NewOpc;
    switch (MI.getOpcode()) {
    default:
      llvm_unreachable("Unexpected opcode");
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(WADD_WV)
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(WADDU_WV)
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(WSUB_WV)
    CASE_WIDEOP_CHANGE_OPCODE_LMULS(WSUBU_WV)
    }
    // clang-format on

    MachineBasicBlock &MBB = *MI.getParent();
    MIB = BuildMI(MBB, MI, MI.getDebugLoc(), get(NewOpc))
              .add(MI.getOperand(0))
              .addReg(MI.getOperand(0).getReg(), RegState::Undef)
              .add(MI.getOperand(1))
              .add(MI.getOperand(2))
              .add(MI.getOperand(3))
              .add(MI.getOperand(4))
              .add(MI.getOperand(5));
    break;
  }
  }
  MIB.copyImplicitOps(MI);

  if (LV) {
    unsigned NumOps = MI.getNumOperands();
    for (unsigned I = 1; I < NumOps; ++I) {
      MachineOperand &Op = MI.getOperand(I);
      if (Op.isReg() && Op.isKill())
        LV->replaceKillInstruction(Op.getReg(), MI, *MIB);
    }
  }

  if (LIS) {
    SlotIndex Idx = LIS->ReplaceMachineInstrInMaps(MI, *MIB);

    if (MI.getOperand(0).isEarlyClobber()) {
      // Use operand 1 was tied to early-clobber def operand 0, so its live
      // interval could have ended at an early-clobber slot. Now they are not
      // tied we need to update it to the normal register slot.
      LiveInterval &LI = LIS->getInterval(MI.getOperand(1).getReg());
      LiveRange::Segment *S = LI.getSegmentContaining(Idx);
      if (S->end == Idx.getRegSlot(true))
        S->end = Idx.getRegSlot();
    }
  }

  return MIB;
#endif
  return nullptr;
}

#undef CASE_WIDEOP_OPCODE_COMMON
#undef CASE_WIDEOP_OPCODE_LMULS
#undef CASE_WIDEOP_CHANGE_OPCODE_COMMON
#undef CASE_WIDEOP_CHANGE_OPCODE_LMULS
#undef CASE_FP_WIDEOP_OPCODE_COMMON
#undef CASE_FP_WIDEOP_OPCODE_LMULS
#undef CASE_FP_WIDEOP_CHANGE_OPCODE_COMMON
#undef CASE_FP_WIDEOP_CHANGE_OPCODE_LMULS

void YSXInstrInfo::mulImm(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator II, const DebugLoc &DL,
                            Register DestReg, uint32_t Amount,
                            MachineInstr::MIFlag Flag) const {
  MachineRegisterInfo &MRI = MF.getRegInfo();
  if (llvm::has_single_bit<uint32_t>(Amount)) {
    uint32_t ShiftAmount = Log2_32(Amount);
    if (ShiftAmount == 0)
      return;
    BuildMI(MBB, II, DL, get(YSX::SLLI), DestReg)
        .addReg(DestReg, RegState::Kill)
        .addImm(ShiftAmount)
        .setMIFlag(Flag);
  } else if (llvm::has_single_bit<uint32_t>(Amount - 1)) {
    Register ScaledRegister = MRI.createVirtualRegister(&YSX::GPRRegClass);
    uint32_t ShiftAmount = Log2_32(Amount - 1);
    BuildMI(MBB, II, DL, get(YSX::SLLI), ScaledRegister)
        .addReg(DestReg)
        .addImm(ShiftAmount)
        .setMIFlag(Flag);
    BuildMI(MBB, II, DL, get(YSX::ADD), DestReg)
        .addReg(ScaledRegister, RegState::Kill)
        .addReg(DestReg, RegState::Kill)
        .setMIFlag(Flag);
  } else if (llvm::has_single_bit<uint32_t>(Amount + 1)) {
    Register ScaledRegister = MRI.createVirtualRegister(&YSX::GPRRegClass);
    uint32_t ShiftAmount = Log2_32(Amount + 1);
    BuildMI(MBB, II, DL, get(YSX::SLLI), ScaledRegister)
        .addReg(DestReg)
        .addImm(ShiftAmount)
        .setMIFlag(Flag);
    BuildMI(MBB, II, DL, get(YSX::SUB), DestReg)
        .addReg(ScaledRegister, RegState::Kill)
        .addReg(DestReg, RegState::Kill)
        .setMIFlag(Flag);
  } else if (STI.hasStdExtZmmul()) {
    Register N = MRI.createVirtualRegister(&YSX::GPRRegClass);
    movImm(MBB, II, DL, N, Amount, Flag);
    BuildMI(MBB, II, DL, get(YSX::MUL), DestReg)
        .addReg(DestReg, RegState::Kill)
        .addReg(N, RegState::Kill)
        .setMIFlag(Flag);
  } else {
    Register Acc;
    uint32_t PrevShiftAmount = 0;
    for (uint32_t ShiftAmount = 0; Amount >> ShiftAmount; ShiftAmount++) {
      if (Amount & (1U << ShiftAmount)) {
        if (ShiftAmount)
          BuildMI(MBB, II, DL, get(YSX::SLLI), DestReg)
              .addReg(DestReg, RegState::Kill)
              .addImm(ShiftAmount - PrevShiftAmount)
              .setMIFlag(Flag);
        if (Amount >> (ShiftAmount + 1)) {
          // If we don't have an accmulator yet, create it and copy DestReg.
          if (!Acc) {
            Acc = MRI.createVirtualRegister(&YSX::GPRRegClass);
            BuildMI(MBB, II, DL, get(TargetOpcode::COPY), Acc)
                .addReg(DestReg)
                .setMIFlag(Flag);
          } else {
            BuildMI(MBB, II, DL, get(YSX::ADD), Acc)
                .addReg(Acc, RegState::Kill)
                .addReg(DestReg)
                .setMIFlag(Flag);
          }
        }
        PrevShiftAmount = ShiftAmount;
      }
    }
    assert(Acc && "Expected valid accumulator");
    BuildMI(MBB, II, DL, get(YSX::ADD), DestReg)
        .addReg(DestReg, RegState::Kill)
        .addReg(Acc, RegState::Kill)
        .setMIFlag(Flag);
  }
}

ArrayRef<std::pair<MachineMemOperand::Flags, const char *>>
YSXInstrInfo::getSerializableMachineMemOperandTargetFlags() const {
  static const std::pair<MachineMemOperand::Flags, const char *> TargetFlags[] =
      {{MONontemporalBit0, "ysx-nontemporal-domain-bit-0"},
       {MONontemporalBit1, "ysx-nontemporal-domain-bit-1"}};
  return ArrayRef(TargetFlags);
}

unsigned YSXInstrInfo::getTailDuplicateSize(CodeGenOptLevel OptLevel) const {
  return OptLevel >= CodeGenOptLevel::Aggressive
             ? STI.getTailDupAggressiveThreshold()
             : 2;
}

bool YSX::isYSXVecSpill(const MachineInstr &MI) {
  return false;
}

/// Return true if \p MI is a copy that will be lowered to one or more vmvNr.vs.
bool YSX::isVectorCopy(const TargetRegisterInfo *TRI,
                         const MachineInstr &MI) {
  return false;
}

std::optional<std::pair<unsigned, unsigned>>
YSX::isYSXVecSpillForZvlsseg(unsigned Opcode) {
  return std::nullopt;
}

bool YSX::hasEqualFRM(const MachineInstr &MI1, const MachineInstr &MI2) {
  return false;
}

std::optional<unsigned>
YSX::getVectorLowDemandedScalarBits(unsigned Opcode, unsigned Log2SEW) {
  return std::nullopt;
}

unsigned YSX::getYSXVecMCOpcode(unsigned YSXVecPseudoOpcode) {
  return 0;
}

unsigned YSX::getDestLog2EEW(const MCInstrDesc &Desc, unsigned Log2SEW) {
  unsigned DestEEW =
      (Desc.TSFlags & YSXII::DestEEWMask) >> YSXII::DestEEWShift;
  // EEW = 1
  if (DestEEW == 0)
    return 0;
  // EEW = SEW * n
  unsigned Scaled = Log2SEW + (DestEEW - 1);
  assert(Scaled >= 3 && Scaled <= 6);
  return Scaled;
}

static std::optional<int64_t> getEffectiveImm(const MachineOperand &MO) {
  assert(MO.isImm() || MO.getReg().isVirtual());
  if (MO.isImm())
    return MO.getImm();
  const MachineInstr *Def =
      MO.getParent()->getMF()->getRegInfo().getVRegDef(MO.getReg());
  int64_t Imm;
  if (isLoadImm(Def, Imm))
    return Imm;
  return std::nullopt;
}

/// Given two VL operands, do we know that LHS <= RHS? Must be used in SSA form.
bool YSX::isVLKnownLE(const MachineOperand &LHS, const MachineOperand &RHS) {
  assert((LHS.isImm() || LHS.getParent()->getMF()->getRegInfo().isSSA()) &&
         (RHS.isImm() || RHS.getParent()->getMF()->getRegInfo().isSSA()));
  if (LHS.isReg() && RHS.isReg() && LHS.getReg().isVirtual() &&
      LHS.getReg() == RHS.getReg())
    return true;
  if (RHS.isImm() && RHS.getImm() == YSX::VLMaxSentinel)
    return true;
  if (LHS.isImm() && LHS.getImm() == 0)
    return true;
  if (LHS.isImm() && LHS.getImm() == YSX::VLMaxSentinel)
    return false;
  std::optional<int64_t> LHSImm = getEffectiveImm(LHS),
                         RHSImm = getEffectiveImm(RHS);
  if (!LHSImm || !RHSImm)
    return false;
  return LHSImm <= RHSImm;
}

namespace {
class YSXPipelinerLoopInfo : public TargetInstrInfo::PipelinerLoopInfo {
  const MachineInstr *LHS;
  const MachineInstr *RHS;
  SmallVector<MachineOperand, 3> Cond;

public:
  YSXPipelinerLoopInfo(const MachineInstr *LHS, const MachineInstr *RHS,
                         const SmallVectorImpl<MachineOperand> &Cond)
      : LHS(LHS), RHS(RHS), Cond(Cond.begin(), Cond.end()) {}

  bool shouldIgnoreForPipelining(const MachineInstr *MI) const override {
    // Make the instructions for loop control be placed in stage 0.
    // The predecessors of LHS/RHS are considered by the caller.
    if (LHS && MI == LHS)
      return true;
    if (RHS && MI == RHS)
      return true;
    return false;
  }

  std::optional<bool> createTripCountGreaterCondition(
      int TC, MachineBasicBlock &MBB,
      SmallVectorImpl<MachineOperand> &CondParam) override {
    // A branch instruction will be inserted as "if (Cond) goto epilogue".
    // Cond is normalized for such use.
    // The predecessors of the branch are assumed to have already been inserted.
    CondParam = Cond;
    return {};
  }

  void setPreheader(MachineBasicBlock *NewPreheader) override {}

  void adjustTripCount(int TripCountAdjust) override {}
};
} // namespace

std::unique_ptr<TargetInstrInfo::PipelinerLoopInfo>
YSXInstrInfo::analyzeLoopForPipelining(MachineBasicBlock *LoopBB) const {
  MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
  SmallVector<MachineOperand, 4> Cond;
  if (analyzeBranch(*LoopBB, TBB, FBB, Cond, /*AllowModify=*/false))
    return nullptr;

  // Infinite loops are not supported
  if (TBB == LoopBB && FBB == LoopBB)
    return nullptr;

  // Must be conditional branch
  if (FBB == nullptr)
    return nullptr;

  assert((TBB == LoopBB || FBB == LoopBB) &&
         "The Loop must be a single-basic-block loop");

  // Normalization for createTripCountGreaterCondition()
  if (TBB == LoopBB)
    reverseBranchCondition(Cond);

  const MachineRegisterInfo &MRI = LoopBB->getParent()->getRegInfo();
  auto FindRegDef = [&MRI](MachineOperand &Op) -> const MachineInstr * {
    if (!Op.isReg())
      return nullptr;
    Register Reg = Op.getReg();
    if (!Reg.isVirtual())
      return nullptr;
    return MRI.getVRegDef(Reg);
  };

  const MachineInstr *LHS = FindRegDef(Cond[1]);
  const MachineInstr *RHS = FindRegDef(Cond[2]);
  if (LHS && LHS->isPHI())
    return nullptr;
  if (RHS && RHS->isPHI())
    return nullptr;

  return std::make_unique<YSXPipelinerLoopInfo>(LHS, RHS, Cond);
}

// FIXME: We should remove this if we have a default generic scheduling model.
bool YSXInstrInfo::isHighLatencyDef(int Opc) const {
  switch (Opc) {
  default:
    return false;
  case YSX::DIV:
  case YSX::DIVW:
  case YSX::DIVU:
  case YSX::DIVUW:
  case YSX::REM:
  case YSX::REMW:
  case YSX::REMU:
  case YSX::REMUW:
    return true;
  }
}

bool YSXInstrInfo::isVRegCopy(const MachineInstr *MI, unsigned LMul) const {
  if (MI->getOpcode() != TargetOpcode::COPY)
    return false;
  const MachineRegisterInfo &MRI = MI->getMF()->getRegInfo();
  const TargetRegisterInfo *TRI = MRI.getTargetRegisterInfo();

  Register DstReg = MI->getOperand(0).getReg();
  const TargetRegisterClass *RC = DstReg.isVirtual()
                                      ? MRI.getRegClass(DstReg)
                                      : TRI->getMinimalPhysRegClass(DstReg);

  if (!YSXRegisterInfo::isYSXVecRegClass(RC))
    return false;

  return !LMul;
}
