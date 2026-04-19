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
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveVariables.h"
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

static cl::opt<MachineTraceStrategy> ForceMachineCombinerStrategy(
    "ysx-force-machine-combiner-strategy", cl::Hidden,
    cl::desc("Force machine combiner to use a specific strategy for machine "
             "trace metrics evaluation."),
    cl::init(MachineTraceStrategy::TS_NumStrategies),
    cl::values(clEnumValN(MachineTraceStrategy::TS_Local, "local",
                          "Local strategy."),
               clEnumValN(MachineTraceStrategy::TS_MinInstrCount, "min-instr",
                          "MinInstrCount strategy.")));

YSXInstrInfo::YSXInstrInfo(const YSXSubtarget &STI)
    : YSXGenInstrInfo(STI, RegInfo, YSX::ADJCALLSTACKDOWN, YSX::ADJCALLSTACKUP),
      RegInfo(STI.getHwMode()), STI(STI) {}

#define GET_INSTRINFO_HELPERS
#include "YSXGenInstrInfo.inc"

MCInst YSXInstrInfo::getNop() const {
  return MCInstBuilder(YSX::ADDI).addReg(YSX::X0).addReg(YSX::X0).addImm(0);
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

bool YSXInstrInfo::isReMaterializableImpl(const MachineInstr &MI) const {
  return TargetInstrInfo::isReMaterializableImpl(MI);
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

  if (YSX::GPRPairRegClass.contains(DstReg, SrcReg)) {

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
}

void YSXInstrInfo::movImm(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator MBBI, const DebugLoc &DL,
                          Register DstReg, uint64_t Val,
                          MachineInstr::MIFlag Flag, bool DstRenamable,
                          bool DstIsDead) const {
  Register SrcReg = YSX::X0;

  YSXMatInt::InstSeq Seq = YSXMatInt::generateInstSeq(Val, STI);
  assert(!Seq.empty());

  bool SrcRenamable = false;
  unsigned Num = 0;

  for (const YSXMatInt::Inst &Inst : Seq) {
    bool LastItem = ++Num == Seq.size();
    unsigned DstRegState = getDeadRegState(DstIsDead && LastItem) |
                           getRenamableRegState(DstRenamable);
    unsigned SrcRegState =
        getKillRegState(SrcReg != YSX::X0) | getRenamableRegState(SrcRenamable);
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
unsigned YSXInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                    MachineBasicBlock *TBB,
                                    MachineBasicBlock *FBB,
                                    ArrayRef<MachineOperand> Cond,
                                    const DebugLoc &DL, int *BytesAdded) const {
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
  Register TmpGPR =
      RS->scavengeRegisterBackwards(*RC, MI.getIterator(),
                                    /*RestoreAfter=*/false, /*SpAdj=*/0,
                                    /*AllowSpill=*/false);
  if (TmpGPR.isValid())
    RS->setRegUsed(TmpGPR);
  else {
    // The case when there is no scavenged register needs special handling.

    // Pick s11 because it doesn't make a difference.
    TmpGPR = YSX::X27;

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
  case YSX::BNE:
    Cond[0].setImm(YSX::BEQ);
    break;
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

  if (Opcode == TargetOpcode::BUNDLE)
    return getInstBundleLength(MI);

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

      return 4 * Num;
    }
    return 68;
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
    return (MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == YSX::X0) ||
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
    SmallVectorImpl<MachineInstr *> &InsInstrs) const {}

bool YSXInstrInfo::hasReassociableOperands(const MachineInstr &Inst,
                                           const MachineBasicBlock *MBB) const {
  return TargetInstrInfo::hasReassociableOperands(Inst, MBB);
}

void YSXInstrInfo::getReassociateOperandIndices(
    const MachineInstr &Root, unsigned Pattern,
    std::array<unsigned, 5> &OperandIndices) const {
  TargetInstrInfo::getReassociateOperandIndices(Root, Pattern, OperandIndices);
}

bool YSXInstrInfo::hasReassociableSibling(const MachineInstr &Inst,
                                          bool &Commuted) const {
  return TargetInstrInfo::hasReassociableSibling(Inst, Commuted);
}

bool YSXInstrInfo::isAssociativeAndCommutative(const MachineInstr &Inst,
                                               bool Invert) const {
  unsigned Opc = Inst.getOpcode();
  if (Invert) {
    auto InverseOpcode = getInverseOpcode(Opc);
    if (!InverseOpcode)
      return false;
    Opc = *InverseOpcode;
  }

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

std::optional<unsigned> YSXInstrInfo::getInverseOpcode(unsigned Opcode) const {
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

CombinerObjective YSXInstrInfo::getCombinerObjective(unsigned Pattern) const {
  return TargetInstrInfo::getCombinerObjective(Pattern);
}

bool YSXInstrInfo::getMachineCombinerPatterns(
    MachineInstr &Root, SmallVectorImpl<unsigned> &Patterns,
    bool DoRegPressureReduce) const {
  return TargetInstrInfo::getMachineCombinerPatterns(Root, Patterns,
                                                     DoRegPressureReduce);
}

void YSXInstrInfo::genAlternativeCodeSequence(
    MachineInstr &Root, unsigned Pattern,
    SmallVectorImpl<MachineInstr *> &InsInstrs,
    SmallVectorImpl<MachineInstr *> &DelInstrs,
    DenseMap<Register, unsigned> &InstrIdxForVirtReg) const {
  TargetInstrInfo::genAlternativeCodeSequence(Root, Pattern, InsInstrs,
                                              DelInstrs, InstrIdxForVirtReg);
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
        case YSXOp::OPERAND_COND_CODE:
          Ok = Imm >= 0 && Imm < YSXCC::COND_INVALID;
          break;
        case YSXOp::OPERAND_ATOMIC_ORDERING:
          Ok = isValidAtomicOrdering(Imm);
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
    }
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
  unsigned InstrSize = 4;
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
    CallOverhead = 4 + InstrSize;
    // Using tail call we move ret instruction from caller to callee.
    FrameOverhead = 0;
  } else {
    // call t0, function = 8 bytes.
    CallOverhead = 8;
    FrameOverhead = InstrSize;
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
  It = MBB.insert(It, BuildMI(MF, DebugLoc(), get(YSX::PseudoCALLReg), YSX::X5)
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
std::string
YSXInstrInfo::createMIROperandComment(const MachineInstr &MI,
                                      const MachineOperand &Op, unsigned OpIdx,
                                      const TargetRegisterInfo *TRI) const {
  // Print a generic comment for this operand if there is one.
  std::string GenericComment =
      TargetInstrInfo::createMIROperandComment(MI, Op, OpIdx, TRI);
  if (!GenericComment.empty())
    return GenericComment;

  // If not, we must have an immediate operand.
  if (!Op.isImm())
    return std::string();

  return std::string();
}

bool YSXInstrInfo::findCommutedOpIndices(const MachineInstr &MI,
                                         unsigned &SrcOpIdx1,
                                         unsigned &SrcOpIdx2) const {
  const MCInstrDesc &Desc = MI.getDesc();
  if (!Desc.isCommutable())
    return false;

  return TargetInstrInfo::findCommutedOpIndices(MI, SrcOpIdx1, SrcOpIdx2);
}

MachineInstr *YSXInstrInfo::commuteInstructionImpl(MachineInstr &MI, bool NewMI,
                                                   unsigned OpIdx1,
                                                   unsigned OpIdx2) const {
  return TargetInstrInfo::commuteInstructionImpl(MI, NewMI, OpIdx1, OpIdx2);
}

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

MachineInstr *YSXInstrInfo::convertToThreeAddress(MachineInstr &MI,
                                                  LiveVariables *LV,
                                                  LiveIntervals *LIS) const {
  return nullptr;
}

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
