//===-- YSXRegisterInfo.cpp - RISC-V Register Information -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the RISC-V implementation of the TargetRegisterInfo class.
//
//===----------------------------------------------------------------------===//

#include "YSXRegisterInfo.h"
#include "YSX.h"
#include "YSXSubtarget.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/ErrorHandling.h"

#define GET_REGINFO_TARGET_DESC
#include "YSXGenRegisterInfo.inc"

using namespace llvm;

static cl::opt<bool> DisableCostPerUse("ysx-disable-cost-per-use",
                                       cl::init(false), cl::Hidden);
static cl::opt<bool>
    DisableRegAllocHints("ysx-disable-regalloc-hints", cl::Hidden,
                         cl::init(false),
                         cl::desc("Disable two address hints for register "
                                  "allocation"));

static_assert(YSX::X1 == YSX::X0 + 1, "Register list not consecutive");
static_assert(YSX::X31 == YSX::X0 + 31, "Register list not consecutive");

YSXRegisterInfo::YSXRegisterInfo(unsigned HwMode)
    : YSXGenRegisterInfo(YSX::X1, /*DwarfFlavour*/0, /*EHFlavor*/0,
                           /*PC*/0, HwMode) {}

const MCPhysReg *
YSXRegisterInfo::getIPRACSRegs(const MachineFunction *MF) const {
  return CSR_IPRA_SaveList;
}

const MCPhysReg *
YSXRegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  auto &Subtarget = MF->getSubtarget<YSXSubtarget>();
  if (MF->getFunction().getCallingConv() == CallingConv::GHC)
    return CSR_NoRegs_SaveList;
  if (MF->getFunction().getCallingConv() == CallingConv::PreserveMost)
    return CSR_RT_MostRegs_SaveList;
  if (MF->getFunction().hasFnAttribute("interrupt"))
    return CSR_Interrupt_SaveList;

  switch (Subtarget.getTargetABI()) {
  default:
    llvm_unreachable("Unrecognized ABI");
  case YSXABI::ABI_ILP32:
  case YSXABI::ABI_LP64:
    return CSR_ILP32_LP64_SaveList;
  }
}

BitVector YSXRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  const YSXFrameLowering *TFI = getFrameLowering(MF);
  BitVector Reserved(getNumRegs());
  auto &Subtarget = MF.getSubtarget<YSXSubtarget>();

  for (size_t Reg = 0; Reg < getNumRegs(); Reg++) {
    // Mark any GPRs requested to be reserved as such
    if (Subtarget.isRegisterReservedByUser(Reg))
      markSuperRegs(Reserved, Reg);

    // Mark all the registers defined as constant in TableGen as reserved.
    if (isConstantPhysReg(Reg))
      markSuperRegs(Reserved, Reg);
  }

  // Use markSuperRegs to ensure any register aliases are also reserved
  markSuperRegs(Reserved, YSX::X2_H); // sp
  markSuperRegs(Reserved, YSX::X3_H); // gp
  markSuperRegs(Reserved, YSX::X4_H); // tp
  if (TFI->hasFP(MF))
    markSuperRegs(Reserved, YSX::X8_H); // fp
  // Reserve the base register if we need to realign the stack and allocate
  // variable-sized objects at runtime.
  if (TFI->hasBP(MF))
    markSuperRegs(Reserved, YSXABI::getBPReg()); // bp

  // Additionally reserve dummy register used to form the register pair
  // beginning with 'x0' for instructions that take register pairs.
  markSuperRegs(Reserved, YSX::DUMMY_REG_PAIR_WITH_X0);

  // There are only 16 GPRs for RVE.
  if (Subtarget.hasStdExtE())
    for (MCPhysReg Reg = YSX::X16_H; Reg <= YSX::X31_H; Reg++)
      markSuperRegs(Reserved, Reg);

  if (MF.getFunction().getCallingConv() == CallingConv::GRAAL) {
    if (Subtarget.hasStdExtE())
      reportFatalUsageError("Graal reserved registers do not exist in RVE");
    markSuperRegs(Reserved, YSX::X23_H);
    markSuperRegs(Reserved, YSX::X27_H);
  }

  // Shadow stack pointer.
  markSuperRegs(Reserved, YSX::SSP);

  // XRemovedSfmmbase
  for (MCPhysReg Reg = YSX::T0; Reg <= YSX::T15; Reg++)
    markSuperRegs(Reserved, Reg);

  assert(checkAllSuperRegsMarked(Reserved));
  return Reserved;
}

bool YSXRegisterInfo::isAsmClobberable(const MachineFunction &MF,
                                         MCRegister PhysReg) const {
  return !MF.getSubtarget().isRegisterReservedByUser(PhysReg);
}

const uint32_t *YSXRegisterInfo::getNoPreservedMask() const {
  return CSR_NoRegs_RegMask;
}

void YSXRegisterInfo::adjustReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator II,
                                  const DebugLoc &DL, Register DestReg,
                                  Register SrcReg, StackOffset Offset,
                                  MachineInstr::MIFlag Flag,
                                  MaybeAlign RequiredAlign) const {

  if (DestReg == SrcReg && !Offset.getFixed() && !Offset.getScalable())
    return;

  MachineFunction &MF = *MBB.getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  const YSXSubtarget &ST = MF.getSubtarget<YSXSubtarget>();
  const YSXInstrInfo *TII = ST.getInstrInfo();

  bool KillSrcReg = false;

  if (Offset.getScalable())
    reportFatalUsageError("YSX does not support scalable stack offsets");

  int64_t Val = Offset.getFixed();
  if (DestReg == SrcReg && Val == 0)
    return;

  const uint64_t Align = RequiredAlign.valueOrOne().value();

  if (isInt<12>(Val)) {
    BuildMI(MBB, II, DL, TII->get(YSX::ADDI), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrcReg))
        .addImm(Val)
        .setMIFlag(Flag);
    return;
  }

  // Try to split the offset across two ADDIs. We need to keep the intermediate
  // result aligned after each ADDI.  We need to determine the maximum value we
  // can put in each ADDI. In the negative direction, we can use -2048 which is
  // always sufficiently aligned. In the positive direction, we need to find the
  // largest 12-bit immediate that is aligned.  Exclude -4096 since it can be
  // created with LUI.
  assert(Align < 2048 && "Required alignment too large");
  int64_t MaxPosAdjStep = 2048 - Align;
  if (Val > -4096 && Val <= (2 * MaxPosAdjStep)) {
    int64_t FirstAdj = Val < 0 ? -2048 : MaxPosAdjStep;
    Val -= FirstAdj;
    BuildMI(MBB, II, DL, TII->get(YSX::ADDI), DestReg)
        .addReg(SrcReg, getKillRegState(KillSrcReg))
        .addImm(FirstAdj)
        .setMIFlag(Flag);
    BuildMI(MBB, II, DL, TII->get(YSX::ADDI), DestReg)
        .addReg(DestReg, RegState::Kill)
        .addImm(Val)
        .setMIFlag(Flag);
    return;
  }

  // Use shNadd if doing so lets us materialize a 12 bit immediate with a single
  // instruction.  This saves 1 instruction over the full lui/addi+add fallback
  // path.  We avoid anything which can be done with a single lui as it might
  // be compressible.  Note that the sh1add case is fully covered by the 2x addi
  // case just above and is thus omitted.
  unsigned Opc = YSX::ADD;
  if (Val < 0) {
    Val = -Val;
    Opc = YSX::SUB;
  }

  Register ScratchReg = MRI.createVirtualRegister(&YSX::GPRRegClass);
  TII->movImm(MBB, II, DL, ScratchReg, Val, Flag);
  BuildMI(MBB, II, DL, TII->get(Opc), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrcReg))
      .addReg(ScratchReg, RegState::Kill)
      .setMIFlag(Flag);
}

bool YSXRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                            int SPAdj, unsigned FIOperandNum,
                                            RegScavenger *RS) const {
  assert(SPAdj == 0 && "Unexpected non-zero SPAdj value");

  MachineInstr &MI = *II;
  MachineFunction &MF = *MI.getParent()->getParent();
  MachineRegisterInfo &MRI = MF.getRegInfo();
  DebugLoc DL = MI.getDebugLoc();

  int FrameIndex = MI.getOperand(FIOperandNum).getIndex();
  Register FrameReg;
  StackOffset Offset =
      getFrameLowering(MF)->getFrameIndexReference(MF, FrameIndex, FrameReg);
  Offset += StackOffset::getFixed(MI.getOperand(FIOperandNum + 1).getImm());

  if (!isInt<32>(Offset.getFixed())) {
    reportFatalUsageError(
        "Frame offsets outside of the signed 32-bit range not supported");
  }

  int64_t Val = Offset.getFixed();
  int64_t Lo12 = SignExtend64<12>(Val);
  unsigned Opc = MI.getOpcode();

  if (Opc == YSX::ADDI && !isInt<12>(Val)) {
    // We chose to emit the canonical immediate sequence rather than folding
    // the offset into the using add under the theory that doing so doesn't
    // save dynamic instruction count and some target may fuse the canonical
    // 32 bit immediate sequence.  We still need to clear the portion of the
    // offset encoded in the immediate.
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(0);
  } else {
    // We can encode an add with 12 bit signed immediate in the immediate
    // operand of our user instruction.  As a result, the remaining
    // offset can by construction, at worst, a LUI and a ADD.
    MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Lo12);
    Offset =
        StackOffset::get((uint64_t)Val - (uint64_t)Lo12, Offset.getScalable());
  }

  if (Offset.getScalable() || Offset.getFixed()) {
    Register DestReg;
    if (MI.getOpcode() == YSX::ADDI)
      DestReg = MI.getOperand(0).getReg();
    else
      DestReg = MRI.createVirtualRegister(&YSX::GPRRegClass);
    adjustReg(*II->getParent(), II, DL, DestReg, FrameReg, Offset,
              MachineInstr::NoFlags, std::nullopt);
    MI.getOperand(FIOperandNum).ChangeToRegister(DestReg, /*IsDef*/false,
                                                 /*IsImp*/false,
                                                 /*IsKill*/true);
  } else {
    MI.getOperand(FIOperandNum).ChangeToRegister(FrameReg, /*IsDef*/false,
                                                 /*IsImp*/false,
                                                 /*IsKill*/false);
  }

  // If after materializing the adjustment, we have a pointless ADDI, remove it
  if (MI.getOpcode() == YSX::ADDI &&
      MI.getOperand(0).getReg() == MI.getOperand(1).getReg() &&
      MI.getOperand(2).getImm() == 0) {
    MI.eraseFromParent();
    return true;
  }

  return false;
}

bool YSXRegisterInfo::requiresVirtualBaseRegisters(
    const MachineFunction &MF) const {
  return true;
}

// Returns true if the instruction's frame index reference would be better
// served by a base register other than FP or SP.
// Used by LocalStackSlotAllocation pass to determine which frame index
// references it should create new base registers for.
bool YSXRegisterInfo::needsFrameBaseReg(MachineInstr *MI,
                                          int64_t Offset) const {
  unsigned FIOperandNum = 0;
  for (; !MI->getOperand(FIOperandNum).isFI(); FIOperandNum++)
    assert(FIOperandNum < MI->getNumOperands() &&
           "Instr doesn't have FrameIndex operand");

  // For RISC-V, The machine instructions that include a FrameIndex operand
  // are load/store, ADDI instructions.
  unsigned MIFrm = YSXII::getFormat(MI->getDesc().TSFlags);
  if (MIFrm != YSXII::InstFormatI && MIFrm != YSXII::InstFormatS)
    return false;
  // We only generate virtual base registers for loads and stores, so
  // return false for everything else.
  if (!MI->mayLoad() && !MI->mayStore())
    return false;

  const MachineFunction &MF = *MI->getMF();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const YSXFrameLowering *TFI = getFrameLowering(MF);
  const MachineRegisterInfo &MRI = MF.getRegInfo();

  if (TFI->hasFP(MF) && !shouldRealignStack(MF)) {
    auto &Subtarget = MF.getSubtarget<YSXSubtarget>();
    // Estimate the stack size used to store callee saved registers(
    // excludes reserved registers).
    unsigned CalleeSavedSize = 0;
    for (const MCPhysReg *R = MRI.getCalleeSavedRegs(); MCPhysReg Reg = *R;
         ++R) {
      if (Subtarget.isRegisterReservedByUser(Reg))
        continue;

      if (YSX::GPRRegClass.contains(Reg))
        CalleeSavedSize += getSpillSize(YSX::GPRRegClass);
    }

    int64_t MaxFPOffset = Offset - CalleeSavedSize;
    return !isFrameOffsetLegal(MI, YSX::X8, MaxFPOffset);
  }

  // Assume 128 bytes spill slots size to estimate the maximum possible
  // offset relative to the stack pointer.
  // FIXME: The 128 is copied from ARM. We should run some statistics and pick a
  // real one for RISC-V.
  int64_t MaxSPOffset = Offset + 128;
  MaxSPOffset += MFI.getLocalFrameSize();
  return !isFrameOffsetLegal(MI, YSX::X2, MaxSPOffset);
}

// Determine whether a given base register plus offset immediate is
// encodable to resolve a frame index.
bool YSXRegisterInfo::isFrameOffsetLegal(const MachineInstr *MI,
                                           Register BaseReg,
                                           int64_t Offset) const {
  unsigned FIOperandNum = 0;
  while (!MI->getOperand(FIOperandNum).isFI()) {
    FIOperandNum++;
    assert(FIOperandNum < MI->getNumOperands() &&
           "Instr does not have a FrameIndex operand!");
  }

  Offset += getFrameIndexInstrOffset(MI, FIOperandNum);
  return isInt<12>(Offset);
}

// Insert defining instruction(s) for a pointer to FrameIdx before
// insertion point I.
// Return materialized frame pointer.
Register YSXRegisterInfo::materializeFrameBaseRegister(MachineBasicBlock *MBB,
                                                         int FrameIdx,
                                                         int64_t Offset) const {
  MachineBasicBlock::iterator MBBI = MBB->begin();
  DebugLoc DL;
  if (MBBI != MBB->end())
    DL = MBBI->getDebugLoc();
  MachineFunction *MF = MBB->getParent();
  MachineRegisterInfo &MFI = MF->getRegInfo();
  const TargetInstrInfo *TII = MF->getSubtarget().getInstrInfo();

  Register BaseReg = MFI.createVirtualRegister(&YSX::GPRRegClass);
  BuildMI(*MBB, MBBI, DL, TII->get(YSX::ADDI), BaseReg)
      .addFrameIndex(FrameIdx)
      .addImm(Offset);
  return BaseReg;
}

// Resolve a frame index operand of an instruction to reference the
// indicated base register plus offset instead.
void YSXRegisterInfo::resolveFrameIndex(MachineInstr &MI, Register BaseReg,
                                          int64_t Offset) const {
  unsigned FIOperandNum = 0;
  while (!MI.getOperand(FIOperandNum).isFI()) {
    FIOperandNum++;
    assert(FIOperandNum < MI.getNumOperands() &&
           "Instr does not have a FrameIndex operand!");
  }

  Offset += getFrameIndexInstrOffset(&MI, FIOperandNum);
  // FrameIndex Operands are always represented as a
  // register followed by an immediate.
  MI.getOperand(FIOperandNum).ChangeToRegister(BaseReg, false);
  MI.getOperand(FIOperandNum + 1).ChangeToImmediate(Offset);
}

// Get the offset from the referenced frame index in the instruction,
// if there is one.
int64_t YSXRegisterInfo::getFrameIndexInstrOffset(const MachineInstr *MI,
                                                    int Idx) const {
  assert((YSXII::getFormat(MI->getDesc().TSFlags) == YSXII::InstFormatI ||
          YSXII::getFormat(MI->getDesc().TSFlags) == YSXII::InstFormatS) &&
         "The MI must be I or S format.");
  assert(MI->getOperand(Idx).isFI() && "The Idx'th operand of MI is not a "
                                       "FrameIndex operand");
  return MI->getOperand(Idx + 1).getImm();
}

Register YSXRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  const TargetFrameLowering *TFI = getFrameLowering(MF);
  return TFI->hasFP(MF) ? YSX::X8 : YSX::X2;
}

StringRef YSXRegisterInfo::getRegAsmName(MCRegister Reg) const {
  return TargetRegisterInfo::getRegAsmName(Reg);
}

const uint32_t *
YSXRegisterInfo::getCallPreservedMask(const MachineFunction & MF,
                                        CallingConv::ID CC) const {
  auto &Subtarget = MF.getSubtarget<YSXSubtarget>();

  if (CC == CallingConv::GHC)
    return CSR_NoRegs_RegMask;
  YSXABI::ABI ABI = Subtarget.getTargetABI();
  if (CC == CallingConv::PreserveMost)
    return CSR_RT_MostRegs_RegMask;
  switch (ABI) {
  default:
    llvm_unreachable("Unrecognized ABI");
  case YSXABI::ABI_ILP32:
  case YSXABI::ABI_LP64:
    return CSR_ILP32_LP64_RegMask;
  }
}

const TargetRegisterClass *
YSXRegisterInfo::getLargestLegalSuperClass(const TargetRegisterClass *RC,
                                             const MachineFunction &) const {
  return RC;
}

void YSXRegisterInfo::getOffsetOpcodes(const StackOffset &Offset,
                                         SmallVectorImpl<uint64_t> &Ops) const {
  assert(Offset.getScalable() == 0 &&
         "YSX does not support scalable vector frame offsets");

  // Add fixed-sized offset using existing DIExpression interface.
  DIExpression::appendOffset(Ops, Offset.getFixed());
}

unsigned
YSXRegisterInfo::getRegisterCostTableIndex(const MachineFunction &MF) const {
  return MF.getSubtarget<YSXSubtarget>().hasStdExtZca() && !DisableCostPerUse
             ? 1
             : 0;
}

float YSXRegisterInfo::getSpillWeightScaleFactor(
    const TargetRegisterClass *RC) const {
  return getRegClassWeight(RC).RegWeight;
}

// Add two address hints to improve chances of being able to use a compressed
// instruction.
bool YSXRegisterInfo::getRegAllocationHints(
    Register VirtReg, ArrayRef<MCPhysReg> Order,
    SmallVectorImpl<MCPhysReg> &Hints, const MachineFunction &MF,
    const VirtRegMap *VRM, const LiveRegMatrix *Matrix) const {
  const MachineRegisterInfo *MRI = &MF.getRegInfo();

  // Handle RegPairEven/RegPairOdd hints for Zilsd register pairs
  std::pair<unsigned, Register> Hint = MRI->getRegAllocationHint(VirtReg);
  unsigned HintType = Hint.first;
  Register Partner = Hint.second;

  MCRegister TargetReg;
  if (HintType == YSXRI::RegPairEven || HintType == YSXRI::RegPairOdd) {
    // Check if we want the even or odd register of a consecutive pair
    bool WantOdd = (HintType == YSXRI::RegPairOdd);

    // First priority: Check if partner is already allocated
    if (Partner.isVirtual() && VRM && VRM->hasPhys(Partner)) {
      MCRegister PartnerPhys = VRM->getPhys(Partner);
      // Calculate the exact register we need for consecutive pairing
      TargetReg = PartnerPhys.id() + (WantOdd ? 1 : -1);

      // Verify it's valid and available
      if (YSX::GPRRegClass.contains(TargetReg) &&
          is_contained(Order, TargetReg))
        Hints.push_back(TargetReg.id());
    }

    // Second priority: Try to find consecutive register pairs in the allocation
    // order
    for (MCPhysReg PhysReg : Order) {
      // Don't add the hint if we already added above.
      if (TargetReg == PhysReg)
        continue;

      unsigned RegNum = getEncodingValue(PhysReg);
      // Check if this register matches the even/odd requirement
      bool IsOdd = (RegNum % 2 != 0);

      // Don't provide hints that are paired to a reserved register.
      MCRegister Paired = PhysReg + (IsOdd ? -1 : 1);
      if (WantOdd == IsOdd && !MRI->isReserved(Paired))
        Hints.push_back(PhysReg);
    }
  }

  bool BaseImplRetVal = TargetRegisterInfo::getRegAllocationHints(
      VirtReg, Order, Hints, MF, VRM, Matrix);

  if (!VRM || DisableRegAllocHints)
    return BaseImplRetVal;

  // Add any two address hints after any copy hints.
  SmallSet<Register, 4> TwoAddrHints;

  auto tryAddHint = [&](const MachineOperand &VRRegMO, const MachineOperand &MO,
                        bool NeedGPRC) -> void {
    Register Reg = MO.getReg();
    Register PhysReg = Reg.isPhysical() ? Reg : Register(VRM->getPhys(Reg));
    // TODO: Support GPRPair subregisters? Need to be careful with even/odd
    // registers. If the virtual register is an odd register of a pair and the
    // physical register is even (or vice versa), we should not add the hint.
    if (PhysReg && (!NeedGPRC || YSX::GPRCRegClass.contains(PhysReg)) &&
        !MO.getSubReg() && !VRRegMO.getSubReg()) {
      if (!MRI->isReserved(PhysReg) && !is_contained(Hints, PhysReg))
        TwoAddrHints.insert(PhysReg);
    }
  };

  // This is all of the compressible binary instructions. If an instruction
  // needs GPRC register class operands \p NeedGPRC will be set to true.
  auto isCompressible = [](const MachineInstr &, bool &NeedGPRC) {
    NeedGPRC = false;
    return false;
  };

  // Returns true if this operand is compressible. For non-registers it always
  // returns true. Immediate range was already checked in isCompressible.
  // For registers, it checks if the register is a GPRC register. reg-reg
  // instructions that require GPRC need all register operands to be GPRC.
  auto isCompressibleOpnd = [&](const MachineOperand &MO) {
    if (!MO.isReg())
      return true;
    Register Reg = MO.getReg();
    Register PhysReg = Reg.isPhysical() ? Reg : Register(VRM->getPhys(Reg));
    return PhysReg && YSX::GPRCRegClass.contains(PhysReg);
  };

  for (auto &MO : MRI->reg_nodbg_operands(VirtReg)) {
    const MachineInstr &MI = *MO.getParent();
    unsigned OpIdx = MO.getOperandNo();
    bool NeedGPRC;
    if (isCompressible(MI, NeedGPRC)) {
      if (OpIdx == 0 && MI.getOperand(1).isReg()) {
        if (!NeedGPRC || MI.getNumExplicitOperands() < 3 ||
            isCompressibleOpnd(MI.getOperand(2)))
          tryAddHint(MO, MI.getOperand(1), NeedGPRC);
        if (MI.isCommutable() && MI.getOperand(2).isReg() &&
            (!NeedGPRC || isCompressibleOpnd(MI.getOperand(1))))
          tryAddHint(MO, MI.getOperand(2), NeedGPRC);
      } else if (OpIdx == 1 && (!NeedGPRC || MI.getNumExplicitOperands() < 3 ||
                                isCompressibleOpnd(MI.getOperand(2)))) {
        tryAddHint(MO, MI.getOperand(0), NeedGPRC);
      } else if (MI.isCommutable() && OpIdx == 2 &&
                 (!NeedGPRC || isCompressibleOpnd(MI.getOperand(1)))) {
        tryAddHint(MO, MI.getOperand(0), NeedGPRC);
      }
    }

    // Add a hint if it would allow auipc/lui+addi(w) fusion.  We do this even
    // without the fusions explicitly enabled as the impact is rarely negative
    // and some cores do implement this fusion.
    if ((MI.getOpcode() == YSX::ADDIW || MI.getOpcode() == YSX::ADDI) &&
        MI.getOperand(1).isReg()) {
      const MachineBasicBlock &MBB = *MI.getParent();
      MachineBasicBlock::const_iterator I = MI.getIterator();
      // Is the previous instruction a LUI or AUIPC that can be fused?
      if (I != MBB.begin()) {
        I = skipDebugInstructionsBackward(std::prev(I), MBB.begin());
        if ((I->getOpcode() == YSX::LUI || I->getOpcode() == YSX::AUIPC) &&
            I->getOperand(0).getReg() == MI.getOperand(1).getReg()) {
          if (OpIdx == 0)
            tryAddHint(MO, MI.getOperand(1), /*NeedGPRC=*/false);
          else
            tryAddHint(MO, MI.getOperand(0), /*NeedGPRC=*/false);
        }
      }
    }
  }

  for (MCPhysReg OrderReg : Order)
    if (TwoAddrHints.count(OrderReg))
      Hints.push_back(OrderReg);

  return BaseImplRetVal;
}

void YSXRegisterInfo::updateRegAllocHint(Register Reg, Register NewReg,
                                           MachineFunction &MF) const {
  MachineRegisterInfo *MRI = &MF.getRegInfo();
  std::pair<unsigned, Register> Hint = MRI->getRegAllocationHint(Reg);

  // Handle RegPairEven/RegPairOdd hints for Zilsd register pairs
  if ((Hint.first == YSXRI::RegPairOdd ||
       Hint.first == YSXRI::RegPairEven) &&
      Hint.second.isVirtual()) {
    // If 'Reg' is one of the even/odd register pair and it's now changed
    // (e.g. coalesced) into a different register, the other register of the
    // pair allocation hint must be updated to reflect the relationship change.
    Register Partner = Hint.second;
    std::pair<unsigned, Register> PartnerHint =
        MRI->getRegAllocationHint(Partner);

    // Make sure partner still points to us
    if (PartnerHint.second == Reg) {
      // Update partner to point to NewReg instead of Reg
      MRI->setRegAllocationHint(Partner, PartnerHint.first, NewReg);

      // If NewReg is virtual, set up the reciprocal hint
      // NewReg takes over Reg's role, so it gets the SAME hint type as Reg
      if (NewReg.isVirtual())
        MRI->setRegAllocationHint(NewReg, Hint.first, Partner);
    }
  }
}
