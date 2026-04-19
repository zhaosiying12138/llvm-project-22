//===-- YSXFrameLowering.cpp - RISC-V Frame Information -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the RISC-V implementation of TargetFrameLowering class.
//
//===----------------------------------------------------------------------===//

#include "YSXFrameLowering.h"
#include "MCTargetDesc/YSXBaseInfo.h"
#include "YSXMachineFunctionInfo.h"
#include "YSXSubtarget.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/CodeGen/CFIInstBuilder.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/IR/DiagnosticInfo.h"

#include <algorithm>

#define DEBUG_TYPE "ysx-frame"

using namespace llvm;

static Align getABIStackAlignment(YSXABI::ABI ABI) {
  return Align(16);
}

YSXFrameLowering::YSXFrameLowering(const YSXSubtarget &STI)
    : TargetFrameLowering(
          StackGrowsDown, getABIStackAlignment(STI.getTargetABI()),
          /*LocalAreaOffset=*/0,
          /*TransientStackAlignment=*/getABIStackAlignment(STI.getTargetABI())),
      STI(STI) {}

// The register used to hold the frame pointer.
static constexpr MCPhysReg FramePointerReg = YSX::X8;

// The register used to hold the stack pointer.
static constexpr MCPhysReg SPReg = YSX::X2;

// The register used to hold the return address.
static constexpr MCPhysReg RAReg = YSX::X1;

// List of CSRs that are given a fixed location by save/restore libcalls.
static const MCPhysReg FixedCSRFIMap[] = {
    /*ra*/ RAReg,      /*s0*/ FramePointerReg,      /*s1*/ YSX::X9,
    /*s2*/ YSX::X18, /*s3*/ YSX::X19, /*s4*/ YSX::X20,
    /*s5*/ YSX::X21, /*s6*/ YSX::X22, /*s7*/ YSX::X23,
    /*s8*/ YSX::X24, /*s9*/ YSX::X25, /*s10*/ YSX::X26,
    /*s11*/ YSX::X27};

/// Returns true if DWARF CFI instructions ("frame moves") should be emitted.
static bool needsDwarfCFI(const MachineFunction &MF) {
  return MF.needsFrameMoves();
}

// For now we use x3, a.k.a gp, as pointer to shadow call stack.
// User should not use x3 in their asm.
static void emitSCSPrologue(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI,
                            const DebugLoc &DL) {
  const auto &STI = MF.getSubtarget<YSXSubtarget>();
  bool HasHWShadowStack = false;
  bool HasSWShadowStack =
      MF.getFunction().hasFnAttribute(Attribute::ShadowCallStack);
  if (!HasHWShadowStack && !HasSWShadowStack)
    return;

  const llvm::YSXRegisterInfo *TRI = STI.getRegisterInfo();

  // Do not save RA to the SCS if it's not saved to the regular stack,
  // i.e. RA is not at risk of being overwritten.
  std::vector<CalleeSavedInfo> &CSI = MF.getFrameInfo().getCalleeSavedInfo();
  if (llvm::none_of(
          CSI, [&](CalleeSavedInfo &CSR) { return CSR.getReg() == RAReg; }))
    return;

  const YSXInstrInfo *TII = STI.getInstrInfo();

  Register SCSPReg = YSXABI::getSCSPReg();

  bool IsRV64 = STI.is64Bit();
  int64_t SlotSize = STI.getXLen() / 8;
  // Store return address to shadow call stack
  // addi    gp, gp, [4|8]
  // s[w|d]  ra, -[4|8](gp)
  BuildMI(MBB, MI, DL, TII->get(YSX::ADDI))
      .addReg(SCSPReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(SlotSize)
      .setMIFlag(MachineInstr::FrameSetup);
  BuildMI(MBB, MI, DL, TII->get(IsRV64 ? YSX::SD : YSX::SW))
      .addReg(RAReg)
      .addReg(SCSPReg)
      .addImm(-SlotSize)
      .setMIFlag(MachineInstr::FrameSetup);

  if (!needsDwarfCFI(MF))
    return;

  // Emit a CFI instruction that causes SlotSize to be subtracted from the value
  // of the shadow stack pointer when unwinding past this frame.
  char DwarfSCSReg = TRI->getDwarfRegNum(SCSPReg, /*IsEH*/ true);
  assert(DwarfSCSReg < 32 && "SCS Register should be < 32 (X3).");

  char Offset = static_cast<char>(-SlotSize) & 0x7f;
  const char CFIInst[] = {
      dwarf::DW_CFA_val_expression,
      DwarfSCSReg, // register
      2,           // length
      static_cast<char>(unsigned(dwarf::DW_OP_breg0 + DwarfSCSReg)),
      Offset, // addend (sleb128)
  };

  CFIInstBuilder(MBB, MI, MachineInstr::FrameSetup)
      .buildEscape(StringRef(CFIInst, sizeof(CFIInst)));
}

static void emitSCSEpilogue(MachineFunction &MF, MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI,
                            const DebugLoc &DL) {
  const auto &STI = MF.getSubtarget<YSXSubtarget>();
  bool HasHWShadowStack = false;
  bool HasSWShadowStack =
      MF.getFunction().hasFnAttribute(Attribute::ShadowCallStack);
  if (!HasHWShadowStack && !HasSWShadowStack)
    return;

  // See emitSCSPrologue() above.
  std::vector<CalleeSavedInfo> &CSI = MF.getFrameInfo().getCalleeSavedInfo();
  if (llvm::none_of(
          CSI, [&](CalleeSavedInfo &CSR) { return CSR.getReg() == RAReg; }))
    return;

  const YSXInstrInfo *TII = STI.getInstrInfo();

  Register SCSPReg = YSXABI::getSCSPReg();

  bool IsRV64 = STI.is64Bit();
  int64_t SlotSize = STI.getXLen() / 8;
  // Load return address from shadow call stack
  // l[w|d]  ra, -[4|8](gp)
  // addi    gp, gp, -[4|8]
  BuildMI(MBB, MI, DL, TII->get(IsRV64 ? YSX::LD : YSX::LW))
      .addReg(RAReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(-SlotSize)
      .setMIFlag(MachineInstr::FrameDestroy);
  BuildMI(MBB, MI, DL, TII->get(YSX::ADDI))
      .addReg(SCSPReg, RegState::Define)
      .addReg(SCSPReg)
      .addImm(-SlotSize)
      .setMIFlag(MachineInstr::FrameDestroy);
  if (needsDwarfCFI(MF)) {
    // Restore the SCS pointer
    CFIInstBuilder(MBB, MI, MachineInstr::FrameDestroy).buildRestore(SCSPReg);
  }
}

// Insert instruction to swap mscratchsw with sp
static void emitSiFiveCLICStackSwap(MachineFunction &MF, MachineBasicBlock &MBB,
                                    MachineBasicBlock::iterator MBBI,
                                    const DebugLoc &DL) {
  (void)MF;
  (void)MBB;
  (void)MBBI;
  (void)DL;
}

static void
createSiFivePreemptibleInterruptFrameEntries(MachineFunction &MF,
                                             YSXMachineFunctionInfo &RVFI) {
  (void)MF;
  (void)RVFI;
}

static void emitSiFiveCLICPreemptibleSaves(MachineFunction &MF,
                                           MachineBasicBlock &MBB,
                                           MachineBasicBlock::iterator MBBI,
                                           const DebugLoc &DL) {
  (void)MF;
  (void)MBB;
  (void)MBBI;
  (void)DL;
}

static void emitSiFiveCLICPreemptibleRestores(MachineFunction &MF,
                                              MachineBasicBlock &MBB,
                                              MachineBasicBlock::iterator MBBI,
                                              const DebugLoc &DL) {
  (void)MF;
  (void)MBB;
  (void)MBBI;
  (void)DL;
}

// Get the ID of the libcall used for spilling and restoring callee saved
// registers. The ID is representative of the number of registers saved or
// restored by the libcall, except it is zero-indexed - ID 0 corresponds to a
// single register.
static int getLibCallID(const MachineFunction &MF,
                        const std::vector<CalleeSavedInfo> &CSI) {
  const auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();

  if (CSI.empty() || !RVFI->useSaveRestoreLibCalls(MF))
    return -1;

  MCRegister MaxReg;
  for (auto &CS : CSI)
    // assignCalleeSavedSpillSlots assigns negative frame indexes to
    // registers which can be saved by libcall.
    if (CS.getFrameIdx() < 0)
      MaxReg = std::max(MaxReg.id(), CS.getReg().id());

  if (!MaxReg)
    return -1;

  switch (MaxReg.id()) {
  default:
    llvm_unreachable("Something has gone wrong!");
    // clang-format off
  case /*s11*/ YSX::X27: return 12;
  case /*s10*/ YSX::X26: return 11;
  case /*s9*/  YSX::X25: return 10;
  case /*s8*/  YSX::X24: return 9;
  case /*s7*/  YSX::X23: return 8;
  case /*s6*/  YSX::X22: return 7;
  case /*s5*/  YSX::X21: return 6;
  case /*s4*/  YSX::X20: return 5;
  case /*s3*/  YSX::X19: return 4;
  case /*s2*/  YSX::X18: return 3;
  case /*s1*/  YSX::X9:  return 2;
  case /*s0*/  FramePointerReg:  return 1;
  case /*ra*/  RAReg:  return 0;
    // clang-format on
  }
}

// Get the name of the libcall used for spilling callee saved registers.
// If this function will not use save/restore libcalls, then return a nullptr.
static const char *
getSpillLibCallName(const MachineFunction &MF,
                    const std::vector<CalleeSavedInfo> &CSI) {
  static const char *const SpillLibCalls[] = {
    "__ysx_save_0",
    "__ysx_save_1",
    "__ysx_save_2",
    "__ysx_save_3",
    "__ysx_save_4",
    "__ysx_save_5",
    "__ysx_save_6",
    "__ysx_save_7",
    "__ysx_save_8",
    "__ysx_save_9",
    "__ysx_save_10",
    "__ysx_save_11",
    "__ysx_save_12"
  };

  int LibCallID = getLibCallID(MF, CSI);
  if (LibCallID == -1)
    return nullptr;
  return SpillLibCalls[LibCallID];
}

// Get the name of the libcall used for restoring callee saved registers.
// If this function will not use save/restore libcalls, then return a nullptr.
static const char *
getRestoreLibCallName(const MachineFunction &MF,
                      const std::vector<CalleeSavedInfo> &CSI) {
  static const char *const RestoreLibCalls[] = {
    "__ysx_restore_0",
    "__ysx_restore_1",
    "__ysx_restore_2",
    "__ysx_restore_3",
    "__ysx_restore_4",
    "__ysx_restore_5",
    "__ysx_restore_6",
    "__ysx_restore_7",
    "__ysx_restore_8",
    "__ysx_restore_9",
    "__ysx_restore_10",
    "__ysx_restore_11",
    "__ysx_restore_12"
  };

  int LibCallID = getLibCallID(MF, CSI);
  if (LibCallID == -1)
    return nullptr;
  return RestoreLibCalls[LibCallID];
}

// Return true if the specified function should have a dedicated frame
// pointer register.  This is true if frame pointer elimination is
// disabled, if it needs dynamic stack realignment, if the function has
// variable sized allocas, or if the frame address is taken.
bool YSXFrameLowering::hasFPImpl(const MachineFunction &MF) const {
  const TargetRegisterInfo *RegInfo = MF.getSubtarget().getRegisterInfo();

  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MF.getTarget().Options.DisableFramePointerElim(MF) ||
         RegInfo->hasStackRealignment(MF) || MFI.hasVarSizedObjects() ||
         MFI.isFrameAddressTaken();
}

bool YSXFrameLowering::hasBP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *TRI = STI.getRegisterInfo();

  // If we do not reserve stack space for outgoing arguments in prologue,
  // we will adjust the stack pointer before call instruction. After the
  // adjustment, we can not use SP to access the stack objects for the
  // arguments. Instead, use BP to access these stack objects.
  return (MFI.hasVarSizedObjects() ||
          (!hasReservedCallFrame(MF) && (!MFI.isMaxCallFrameSizeComputed() ||
                                         MFI.getMaxCallFrameSize() != 0))) &&
         TRI->hasStackRealignment(MF);
}

// Determines the size of the frame and maximum call frame size.
void YSXFrameLowering::determineFrameLayout(MachineFunction &MF) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();

  // Get the number of bytes to allocate from the FrameInfo.
  uint64_t FrameSize = MFI.getStackSize();

  // Get the alignment.
  Align StackAlign = getStackAlign();

  // Make sure the frame is aligned.
  FrameSize = alignTo(FrameSize, StackAlign);

  // Update frame info.
  MFI.setStackSize(FrameSize);

}

static SmallVector<CalleeSavedInfo, 8>
getUnmanagedCSI(const MachineFunction &MF,
                const std::vector<CalleeSavedInfo> &CSI) {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  SmallVector<CalleeSavedInfo, 8> NonLibcallCSI;

  for (auto &CS : CSI) {
    int FI = CS.getFrameIdx();
    if (FI >= 0 && MFI.getStackID(FI) == TargetStackID::Default)
      NonLibcallCSI.push_back(CS);
  }

  return NonLibcallCSI;
}

static SmallVector<CalleeSavedInfo, 8>
getPushOrLibCallsSavedInfo(const MachineFunction &MF,
                           const std::vector<CalleeSavedInfo> &CSI) {
  auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();

  SmallVector<CalleeSavedInfo, 8> PushOrLibCallsCSI;
  if (!RVFI->useSaveRestoreLibCalls(MF))
    return PushOrLibCallsCSI;

  for (const auto &CS : CSI) {
    if (llvm::is_contained(FixedCSRFIMap, CS.getReg()))
      PushOrLibCallsCSI.push_back(CS);
  }

  return PushOrLibCallsCSI;
}

// Allocate stack space and probe it if necessary.
void YSXFrameLowering::allocateStack(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI,
                                       MachineFunction &MF, uint64_t Offset,
                                       uint64_t RealStackSize, bool EmitCFI,
                                       bool NeedProbe, uint64_t ProbeSize,
                                       bool DynAllocation,
                                       MachineInstr::MIFlag Flag) const {
  DebugLoc DL;
  const YSXRegisterInfo *RI = STI.getRegisterInfo();
  const YSXInstrInfo *TII = STI.getInstrInfo();
  bool IsRV64 = STI.is64Bit();
  CFIInstBuilder CFIBuilder(MBB, MBBI, MachineInstr::FrameSetup);

  // Simply allocate the stack if it's not big enough to require a probe.
  if (!NeedProbe || Offset <= ProbeSize) {
    RI->adjustReg(MBB, MBBI, DL, SPReg, SPReg, StackOffset::getFixed(-Offset),
                  Flag, getStackAlign());

    if (EmitCFI)
      CFIBuilder.buildDefCFAOffset(RealStackSize);

    if (NeedProbe && DynAllocation) {
      // s[d|w] zero, 0(sp)
      BuildMI(MBB, MBBI, DL, TII->get(IsRV64 ? YSX::SD : YSX::SW))
          .addReg(YSX::X0)
          .addReg(SPReg)
          .addImm(0)
          .setMIFlags(Flag);
    }

    return;
  }

  // Unroll the probe loop depending on the number of iterations.
  if (Offset < ProbeSize * 5) {
    uint64_t CFAAdjust = RealStackSize - Offset;

    uint64_t CurrentOffset = 0;
    while (CurrentOffset + ProbeSize <= Offset) {
      RI->adjustReg(MBB, MBBI, DL, SPReg, SPReg,
                    StackOffset::getFixed(-ProbeSize), Flag, getStackAlign());
      // s[d|w] zero, 0(sp)
      BuildMI(MBB, MBBI, DL, TII->get(IsRV64 ? YSX::SD : YSX::SW))
          .addReg(YSX::X0)
          .addReg(SPReg)
          .addImm(0)
          .setMIFlags(Flag);

      CurrentOffset += ProbeSize;
      if (EmitCFI)
        CFIBuilder.buildDefCFAOffset(CurrentOffset + CFAAdjust);
    }

    uint64_t Residual = Offset - CurrentOffset;
    if (Residual) {
      RI->adjustReg(MBB, MBBI, DL, SPReg, SPReg,
                    StackOffset::getFixed(-Residual), Flag, getStackAlign());
      if (EmitCFI)
        CFIBuilder.buildDefCFAOffset(RealStackSize);

      if (DynAllocation) {
        // s[d|w] zero, 0(sp)
        BuildMI(MBB, MBBI, DL, TII->get(IsRV64 ? YSX::SD : YSX::SW))
            .addReg(YSX::X0)
            .addReg(SPReg)
            .addImm(0)
            .setMIFlags(Flag);
      }
    }

    return;
  }

  // Emit a variable-length allocation probing loop.
  uint64_t RoundedSize = alignDown(Offset, ProbeSize);
  uint64_t Residual = Offset - RoundedSize;

  Register TargetReg = YSX::X6;
  // SUB TargetReg, SP, RoundedSize
  RI->adjustReg(MBB, MBBI, DL, TargetReg, SPReg,
                StackOffset::getFixed(-RoundedSize), Flag, getStackAlign());

  if (EmitCFI) {
    // Set the CFA register to TargetReg.
    CFIBuilder.buildDefCFA(TargetReg, RoundedSize);
  }

  // It will be expanded to a probe loop in `inlineStackProbe`.
  BuildMI(MBB, MBBI, DL, TII->get(YSX::PROBED_STACKALLOC)).addReg(TargetReg);

  if (EmitCFI) {
    // Set the CFA register back to SP.
    CFIBuilder.buildDefCFARegister(SPReg);
  }

  if (Residual) {
    RI->adjustReg(MBB, MBBI, DL, SPReg, SPReg, StackOffset::getFixed(-Residual),
                  Flag, getStackAlign());
    if (DynAllocation) {
      // s[d|w] zero, 0(sp)
      BuildMI(MBB, MBBI, DL, TII->get(IsRV64 ? YSX::SD : YSX::SW))
          .addReg(YSX::X0)
          .addReg(SPReg)
          .addImm(0)
          .setMIFlags(Flag);
    }
  }

  if (EmitCFI)
    CFIBuilder.buildDefCFAOffset(Offset);
}

void YSXFrameLowering::emitPrologue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();
  const YSXRegisterInfo *RI = STI.getRegisterInfo();
  MachineBasicBlock::iterator MBBI = MBB.begin();

  Register BPReg = YSXABI::getBPReg();

  // Debug location must be unknown since the first debug location is used
  // to determine the end of the prologue.
  DebugLoc DL;

  // All calls are tail calls in GHC calling conv, and functions have no
  // prologue/epilogue.
  if (MF.getFunction().getCallingConv() == CallingConv::GHC)
    return;

  // SiFive CLIC needs to swap `sp` into `sf.mscratchcsw`
  emitSiFiveCLICStackSwap(MF, MBB, MBBI, DL);

  // Emit prologue for shadow call stack.
  emitSCSPrologue(MF, MBB, MBBI, DL);

  // Skip past all callee-saved register spill instructions.
  while (MBBI != MBB.end() && MBBI->getFlag(MachineInstr::FrameSetup))
    ++MBBI;

  // Determine the correct frame layout
  determineFrameLayout(MF);

  const auto &CSI = MFI.getCalleeSavedInfo();

  // Skip to before the spills of scalar callee-saved registers
  // FIXME: assumes exactly one instruction is used to restore each
  // callee-saved register.
  MBBI = std::prev(MBBI, getUnmanagedCSI(MF, CSI).size());
  CFIInstBuilder CFIBuilder(MBB, MBBI, MachineInstr::FrameSetup);
  bool NeedsDwarfCFI = needsDwarfCFI(MF);

  // If libcalls are used to spill and restore callee-saved registers, the frame
  // has two sections; the opaque section managed by the libcalls, and the
  // section managed by MachineFrameInfo which can also hold callee saved
  // registers in fixed stack slots, both of which have negative frame indices.
  // This gets even more complicated when incoming arguments are passed via the
  // stack, as these too have negative frame indices. An example is detailed
  // below:
  //
  //  | incoming arg | <- FI[-3]
  //  | libcallspill |
  //  | calleespill  | <- FI[-2]
  //  | calleespill  | <- FI[-1]
  //  | this_frame   | <- FI[0]
  //
  // For negative frame indices, the offset from the frame pointer will differ
  // depending on which of these groups the frame index applies to.
  // The following calculates the correct offset knowing the number of callee
  // saved registers spilt by the two methods.
  if (int LibCallRegs = getLibCallID(MF, MFI.getCalleeSavedInfo()) + 1) {
    // Calculate the size of the frame managed by the libcall. The stack
    // alignment of these libcalls should be the same as how we set it in
    // getABIStackAlignment.
    unsigned LibCallFrameSize =
        alignTo((STI.getXLen() / 8) * LibCallRegs, getStackAlign());
    RVFI->setLibCallStackSize(LibCallFrameSize);

    if (NeedsDwarfCFI) {
      CFIBuilder.buildDefCFAOffset(LibCallFrameSize);
      for (const CalleeSavedInfo &CS : getPushOrLibCallsSavedInfo(MF, CSI))
        CFIBuilder.buildOffset(CS.getReg(),
                               MFI.getObjectOffset(CS.getFrameIdx()));
    }
  }

  // FIXME (note copied from Lanai): This appears to be overallocating.  Needs
  // investigation. Get the number of bytes to allocate from the FrameInfo.
  uint64_t RealStackSize = MFI.getStackSize();
  uint64_t StackSize = RealStackSize - RVFI->getReservedSpillsSize();

  // Early exit if there is no need to allocate on the stack
  if (RealStackSize == 0 && !MFI.adjustsStack())
    return;

  // If the stack pointer has been marked as reserved, then produce an error if
  // the frame requires stack allocation
  if (STI.isRegisterReservedByUser(SPReg))
    MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(), "Stack pointer required, but has been reserved."});

  uint64_t FirstSPAdjustAmount = getFirstSPAdjustAmount(MF);
  // Split the SP adjustment to reduce the offsets of callee saved spill.
  if (FirstSPAdjustAmount) {
    StackSize = FirstSPAdjustAmount;
    RealStackSize = FirstSPAdjustAmount;
  }

  // Allocate space on the stack if necessary.
  auto &Subtarget = MF.getSubtarget<YSXSubtarget>();
  const YSXTargetLowering *TLI = Subtarget.getTargetLowering();
  bool NeedProbe = TLI->hasInlineStackProbe(MF);
  uint64_t ProbeSize = TLI->getStackProbeSize(MF, getStackAlign());
  bool DynAllocation =
      MF.getInfo<YSXMachineFunctionInfo>()->hasDynamicAllocation();
  if (StackSize != 0)
    allocateStack(MBB, MBBI, MF, StackSize, RealStackSize, NeedsDwarfCFI,
                  NeedProbe, ProbeSize, DynAllocation,
                  MachineInstr::FrameSetup);

  // Save SiFive CLIC CSRs into Stack
  emitSiFiveCLICPreemptibleSaves(MF, MBB, MBBI, DL);

  // The frame pointer is callee-saved, and code has been generated for us to
  // save it to the stack. We need to skip over the storing of callee-saved
  // registers as the frame pointer must be modified after it has been saved
  // to the stack, not before.
  // FIXME: assumes exactly one instruction is used to save each callee-saved
  // register.
  std::advance(MBBI, getUnmanagedCSI(MF, CSI).size());
  CFIBuilder.setInsertPoint(MBBI);

  // Iterate over list of callee-saved registers and emit .cfi_offset
  // directives.
  if (NeedsDwarfCFI)
    for (const CalleeSavedInfo &CS : getUnmanagedCSI(MF, CSI))
      CFIBuilder.buildOffset(CS.getReg(),
                             MFI.getObjectOffset(CS.getFrameIdx()));

  // Generate new FP.
  if (hasFP(MF)) {
    if (STI.isRegisterReservedByUser(FramePointerReg))
      MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
          MF.getFunction(), "Frame pointer required, but has been reserved."});
    // The frame pointer does need to be reserved from register allocation.
    assert(MF.getRegInfo().isReserved(FramePointerReg) && "FP not reserved");

    // Some stack management variants automatically keep FP updated, so we don't
    // need an instruction to do so.
    if (!RVFI->hasImplicitFPUpdates(MF)) {
      RI->adjustReg(
          MBB, MBBI, DL, FramePointerReg, SPReg,
          StackOffset::getFixed(RealStackSize - RVFI->getVarArgsSaveSize()),
          MachineInstr::FrameSetup, getStackAlign());
    }

    if (NeedsDwarfCFI)
      CFIBuilder.buildDefCFA(FramePointerReg, RVFI->getVarArgsSaveSize());
  }

  uint64_t SecondSPAdjustAmount = 0;
  // Emit the second SP adjustment after saving callee saved registers.
  if (FirstSPAdjustAmount) {
    SecondSPAdjustAmount = MFI.getStackSize() - FirstSPAdjustAmount;
    assert(SecondSPAdjustAmount > 0 &&
           "SecondSPAdjustAmount should be greater than zero");

    allocateStack(MBB, MBBI, MF, SecondSPAdjustAmount,
                  MFI.getStackSize(), NeedsDwarfCFI && !hasFP(MF),
                  NeedProbe, ProbeSize, DynAllocation,
                  MachineInstr::FrameSetup);
  }

  if (hasFP(MF)) {
    // Realign Stack
    const YSXRegisterInfo *RI = STI.getRegisterInfo();
    if (RI->hasStackRealignment(MF)) {
      Align MaxAlignment = MFI.getMaxAlign();

      const YSXInstrInfo *TII = STI.getInstrInfo();
      if (isInt<12>(-(int)MaxAlignment.value())) {
        BuildMI(MBB, MBBI, DL, TII->get(YSX::ANDI), SPReg)
            .addReg(SPReg)
            .addImm(-(int)MaxAlignment.value())
            .setMIFlag(MachineInstr::FrameSetup);
      } else {
        unsigned ShiftAmount = Log2(MaxAlignment);
        Register VR =
            MF.getRegInfo().createVirtualRegister(&YSX::GPRRegClass);
        BuildMI(MBB, MBBI, DL, TII->get(YSX::SRLI), VR)
            .addReg(SPReg)
            .addImm(ShiftAmount)
            .setMIFlag(MachineInstr::FrameSetup);
        BuildMI(MBB, MBBI, DL, TII->get(YSX::SLLI), SPReg)
            .addReg(VR)
            .addImm(ShiftAmount)
            .setMIFlag(MachineInstr::FrameSetup);
      }
      if (NeedProbe) {
        // Do a probe if the align + size allocated just passed the probe size
        // and was not yet probed.
        if (SecondSPAdjustAmount < ProbeSize &&
            SecondSPAdjustAmount + MaxAlignment.value() >= ProbeSize) {
          bool IsRV64 = STI.is64Bit();
          BuildMI(MBB, MBBI, DL, TII->get(IsRV64 ? YSX::SD : YSX::SW))
              .addReg(YSX::X0)
              .addReg(SPReg)
              .addImm(0)
              .setMIFlags(MachineInstr::FrameSetup);
        }
      }
      // FP will be used to restore the frame in the epilogue, so we need
      // another base register BP to record SP after re-alignment. SP will
      // track the current stack after allocating variable sized objects.
      if (hasBP(MF)) {
        // move BP, SP
        BuildMI(MBB, MBBI, DL, TII->get(YSX::ADDI), BPReg)
            .addReg(SPReg)
            .addImm(0)
            .setMIFlag(MachineInstr::FrameSetup);
      }
    }
  }
}

void YSXFrameLowering::deallocateStack(MachineFunction &MF,
                                         MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MBBI,
                                         const DebugLoc &DL,
                                         uint64_t &StackSize,
                                         int64_t CFAOffset) const {
  const YSXRegisterInfo *RI = STI.getRegisterInfo();

  RI->adjustReg(MBB, MBBI, DL, SPReg, SPReg, StackOffset::getFixed(StackSize),
                MachineInstr::FrameDestroy, getStackAlign());
  StackSize = 0;

  if (needsDwarfCFI(MF))
    CFIInstBuilder(MBB, MBBI, MachineInstr::FrameDestroy)
        .buildDefCFAOffset(CFAOffset);
}

void YSXFrameLowering::emitEpilogue(MachineFunction &MF,
                                      MachineBasicBlock &MBB) const {
  const YSXRegisterInfo *RI = STI.getRegisterInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();

  // All calls are tail calls in GHC calling conv, and functions have no
  // prologue/epilogue.
  if (MF.getFunction().getCallingConv() == CallingConv::GHC)
    return;

  // Get the insert location for the epilogue. If there were no terminators in
  // the block, get the last instruction.
  MachineBasicBlock::iterator MBBI = MBB.end();
  DebugLoc DL;
  if (!MBB.empty()) {
    MBBI = MBB.getLastNonDebugInstr();
    if (MBBI != MBB.end())
      DL = MBBI->getDebugLoc();

    MBBI = MBB.getFirstTerminator();

    // Skip to before the restores of all callee-saved registers.
    while (MBBI != MBB.begin() &&
           std::prev(MBBI)->getFlag(MachineInstr::FrameDestroy))
      --MBBI;
  }

  const auto &CSI = MFI.getCalleeSavedInfo();

  // Skip to before the restores of scalar callee-saved registers
  // FIXME: assumes exactly one instruction is used to restore each
  // callee-saved register.
  auto FirstScalarCSRRestoreInsn = MBBI;
  CFIInstBuilder CFIBuilder(MBB, FirstScalarCSRRestoreInsn,
                            MachineInstr::FrameDestroy);
  bool NeedsDwarfCFI = needsDwarfCFI(MF);

  uint64_t FirstSPAdjustAmount = getFirstSPAdjustAmount(MF);
  uint64_t RealStackSize = FirstSPAdjustAmount ? FirstSPAdjustAmount
                                               : MFI.getStackSize();
  uint64_t StackSize = FirstSPAdjustAmount ? FirstSPAdjustAmount
                                           : MFI.getStackSize() -
                                                 RVFI->getReservedSpillsSize();
  uint64_t FPOffset = RealStackSize - RVFI->getVarArgsSaveSize();

  bool RestoreSPFromFP = RI->hasStackRealignment(MF) ||
                         MFI.hasVarSizedObjects() || !hasReservedCallFrame(MF);

  if (FirstSPAdjustAmount) {
    uint64_t SecondSPAdjustAmount =
        MFI.getStackSize() - FirstSPAdjustAmount;
    assert(SecondSPAdjustAmount > 0 &&
           "SecondSPAdjustAmount should be greater than zero");

    // If RestoreSPFromFP the stack pointer will be restored using the frame
    // pointer value.
    if (!RestoreSPFromFP)
      RI->adjustReg(MBB, FirstScalarCSRRestoreInsn, DL, SPReg, SPReg,
                    StackOffset::getFixed(SecondSPAdjustAmount),
                    MachineInstr::FrameDestroy, getStackAlign());

    if (NeedsDwarfCFI && !hasFP(MF))
      CFIBuilder.buildDefCFAOffset(FirstSPAdjustAmount);
  }

  // Restore the stack pointer using the value of the frame pointer. Only
  // necessary if the stack pointer was modified, meaning the stack size is
  // unknown.
  //
  // In order to make sure the stack point is right through the EH region,
  // we also need to restore stack pointer from the frame pointer if we
  // don't preserve stack space within prologue/epilogue for outgoing variables,
  // normally it's just checking the variable sized object is present or not
  // is enough, but we also don't preserve that at prologue/epilogue when
  // have vector objects in stack.
  if (RestoreSPFromFP) {
    assert(hasFP(MF) && "frame pointer should not have been eliminated");
    RI->adjustReg(MBB, FirstScalarCSRRestoreInsn, DL, SPReg, FramePointerReg,
                  StackOffset::getFixed(-FPOffset), MachineInstr::FrameDestroy,
                  getStackAlign());
  }

  if (NeedsDwarfCFI && hasFP(MF))
    CFIBuilder.buildDefCFA(SPReg, RealStackSize);

  // Skip to after the restores of scalar callee-saved registers
  // FIXME: assumes exactly one instruction is used to restore each
  // callee-saved register.
  MBBI = std::next(FirstScalarCSRRestoreInsn, getUnmanagedCSI(MF, CSI).size());
  CFIBuilder.setInsertPoint(MBBI);

  if (getLibCallID(MF, CSI) != -1) {
    // tail __ysx_restore_[0-12] instruction is considered as a terminator,
    // therefore it is unnecessary to place any CFI instructions after it. Just
    // deallocate stack if needed and return.
    if (StackSize != 0)
      deallocateStack(MF, MBB, MBBI, DL, StackSize,
                      RVFI->getLibCallStackSize());

    // Emit epilogue for shadow call stack.
    emitSCSEpilogue(MF, MBB, MBBI, DL);
    return;
  }

  // Recover callee-saved registers.
  if (NeedsDwarfCFI)
    for (const CalleeSavedInfo &CS : getUnmanagedCSI(MF, CSI))
      CFIBuilder.buildRestore(CS.getReg());

  emitSiFiveCLICPreemptibleRestores(MF, MBB, MBBI, DL);

  if (StackSize != 0)
    deallocateStack(MF, MBB, MBBI, DL, StackSize, 0);

  // Emit epilogue for shadow call stack.
  emitSCSEpilogue(MF, MBB, MBBI, DL);

  // SiFive CLIC needs to swap `sf.mscratchcsw` into `sp`
  emitSiFiveCLICStackSwap(MF, MBB, MBBI, DL);
}

StackOffset
YSXFrameLowering::getFrameIndexReference(const MachineFunction &MF, int FI,
                                           Register &FrameReg) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *RI = MF.getSubtarget().getRegisterInfo();
  const auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();

  // Callee-saved registers should be referenced relative to the stack
  // pointer (positive offset), otherwise use the frame pointer (negative
  // offset).
  const auto &CSI = getUnmanagedCSI(MF, MFI.getCalleeSavedInfo());
  int MinCSFI = 0;
  int MaxCSFI = -1;
  assert(MFI.getStackID(FI) == TargetStackID::Default &&
         "Unexpected stack ID for the frame object.");
  assert(getOffsetOfLocalArea() == 0 && "LocalAreaOffset is not 0!");
  StackOffset Offset =
      StackOffset::getFixed(MFI.getObjectOffset(FI) + MFI.getOffsetAdjustment());

  uint64_t FirstSPAdjustAmount = getFirstSPAdjustAmount(MF);

  if (CSI.size()) {
    MinCSFI = CSI[0].getFrameIdx();
    MaxCSFI = CSI[CSI.size() - 1].getFrameIdx();
  }

  if (FI >= MinCSFI && FI <= MaxCSFI) {
    FrameReg = SPReg;

    if (FirstSPAdjustAmount)
      Offset += StackOffset::getFixed(FirstSPAdjustAmount);
    else
      Offset += StackOffset::getFixed(MFI.getStackSize());
    return Offset;
  }

  if (RI->hasStackRealignment(MF) && !MFI.isFixedObjectIndex(FI)) {
    // If the stack was realigned, the frame pointer is set in order to allow
    // SP to be restored, so we need another base register to record the stack
    // after realignment.
    // |--------------------------| -- <-- FP
    // | callee-allocated save    | | <----|
    // | area for register varargs| |      |
    // |--------------------------| |      |
    // | callee-saved registers   | |      |
    // |--------------------------| --     |
    // | realignment (the size of | |      |
    // | this area is not counted | |      |
    // | in MFI.getStackSize())   | |      |
    // |--------------------------| --     |-- MFI.getStackSize()
    // | scalar local variables   | | <----'
    // |--------------------------| -- <-- BP (if var sized objects present)
    // | VarSize objects          | |
    // |--------------------------| -- <-- SP
    if (hasBP(MF)) {
      FrameReg = YSXABI::getBPReg();
    } else {
      // VarSize objects must be empty in this case!
      assert(!MFI.hasVarSizedObjects());
      FrameReg = SPReg;
    }
  } else {
    FrameReg = RI->getFrameRegister(MF);
  }

  if (FrameReg == FramePointerReg) {
    Offset += StackOffset::getFixed(RVFI->getVarArgsSaveSize());
    return Offset;
  }

  // This case handles indexing off both SP and BP.
  // If indexing off SP, there must not be any var sized objects
  assert(FrameReg == YSXABI::getBPReg() || !MFI.hasVarSizedObjects());

  if (MFI.isFixedObjectIndex(FI))
    assert(!RI->hasStackRealignment(MF) &&
           "Can't index across variable sized realign");
  Offset += StackOffset::getFixed(MFI.getStackSize());
  return Offset;
}

void YSXFrameLowering::determineCalleeSaves(MachineFunction &MF,
                                              BitVector &SavedRegs,
                                              RegScavenger *RS) const {
  TargetFrameLowering::determineCalleeSaves(MF, SavedRegs, RS);

  // Unconditionally spill RA and FP only if the function uses a frame
  // pointer.
  if (hasFP(MF)) {
    SavedRegs.set(RAReg);
    SavedRegs.set(FramePointerReg);
  }
  // Mark BP as used if function has dedicated base pointer.
  if (hasBP(MF))
    SavedRegs.set(YSXABI::getBPReg());

  auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();
  // SiFive Preemptible Interrupt Handlers need additional frame entries
  createSiFivePreemptibleInterruptFrameEntries(MF, *RVFI);
}

static unsigned estimateFunctionSizeInBytes(const MachineFunction &MF,
                                            const YSXInstrInfo &TII) {
  unsigned FnSize = 0;
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      // Far branches over 20-bit offset will be relaxed in branch relaxation
      // pass. In the worst case, conditional branches will be relaxed into
      // the following instruction sequence. Unconditional branches are
      // relaxed in the same way, with the exception that there is no first
      // branch instruction.
      //
      //        foo
      //        bne     t5, t6, .rev_cond # `TII->getInstSizeInBytes(MI)` bytes
      //        sd      s11, 0(sp)        # 4 bytes
      //        jump    .restore, s11     # 8 bytes
      // .rev_cond
      //        bar
      //        j       .dest_bb          # 4 bytes
      // .restore:
      //        ld      s11, 0(sp)        # 4 bytes
      // .dest:
      //        baz
      if (MI.isConditionalBranch())
        FnSize += TII.getInstSizeInBytes(MI);
      if (MI.isConditionalBranch() || MI.isUnconditionalBranch()) {
        FnSize += 4 + 8 + 4 + 4;
        continue;
      }

      FnSize += TII.getInstSizeInBytes(MI);
    }
  }
  return FnSize;
}

void YSXFrameLowering::processFunctionBeforeFrameFinalized(
    MachineFunction &MF, RegScavenger *RS) const {
  const YSXRegisterInfo *RegInfo =
      MF.getSubtarget<YSXSubtarget>().getRegisterInfo();
  const YSXInstrInfo *TII = MF.getSubtarget<YSXSubtarget>().getInstrInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterClass *RC = &YSX::GPRRegClass;
  auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();

  unsigned ScavSlotsNum = 0;

  // estimateStackSize has been observed to under-estimate the final stack
  // size, so give ourselves wiggle-room by checking for stack size
  // representable an 11-bit signed field rather than 12-bits.
  if (!isInt<11>(MFI.estimateStackSize(MF)))
    ScavSlotsNum = 1;

  // Far branches over 20-bit offset require a spill slot for scratch register.
  bool IsLargeFunction = !isInt<20>(estimateFunctionSizeInBytes(MF, *TII));
  if (IsLargeFunction)
    ScavSlotsNum = std::max(ScavSlotsNum, 1u);

  for (unsigned I = 0; I < ScavSlotsNum; I++) {
    int FI = MFI.CreateSpillStackObject(RegInfo->getSpillSize(*RC),
                                        RegInfo->getSpillAlign(*RC));
    RS->addScavengingFrameIndex(FI);

    if (IsLargeFunction && RVFI->getBranchRelaxationScratchFrameIndex() == -1)
      RVFI->setBranchRelaxationScratchFrameIndex(FI);
  }

  unsigned Size = RVFI->getReservedSpillsSize();
  for (const auto &Info : MFI.getCalleeSavedInfo()) {
    int FrameIdx = Info.getFrameIdx();
    if (FrameIdx < 0 || MFI.getStackID(FrameIdx) != TargetStackID::Default)
      continue;

    Size += MFI.getObjectSize(FrameIdx);
  }
  RVFI->setCalleeSavedStackSize(Size);
}

bool YSXFrameLowering::hasReservedCallFrame(const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

// Eliminate ADJCALLSTACKDOWN, ADJCALLSTACKUP pseudo instructions.
MachineBasicBlock::iterator YSXFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  DebugLoc DL = MI->getDebugLoc();

  if (!hasReservedCallFrame(MF)) {
    // If space has not been reserved for a call frame, ADJCALLSTACKDOWN and
    // ADJCALLSTACKUP must be converted to instructions manipulating the stack
    // pointer. This is necessary when there is a variable length stack
    // allocation (e.g. alloca), which means it's not possible to allocate
    // space for outgoing arguments from within the function prologue.
    int64_t Amount = MI->getOperand(0).getImm();

    if (Amount != 0) {
      // Ensure the stack remains aligned after adjustment.
      Amount = alignSPAdjust(Amount);

      if (MI->getOpcode() == YSX::ADJCALLSTACKDOWN)
        Amount = -Amount;

      const YSXTargetLowering *TLI =
          MF.getSubtarget<YSXSubtarget>().getTargetLowering();
      int64_t ProbeSize = TLI->getStackProbeSize(MF, getStackAlign());
      if (TLI->hasInlineStackProbe(MF) && -Amount >= ProbeSize) {
        // When stack probing is enabled, the decrement of SP may need to be
        // probed. We can handle both the decrement and the probing in
        // allocateStack.
        bool DynAllocation =
            MF.getInfo<YSXMachineFunctionInfo>()->hasDynamicAllocation();
        allocateStack(MBB, MI, MF, -Amount, -Amount,
                      needsDwarfCFI(MF) && !hasFP(MF),
                      /*NeedProbe=*/true, ProbeSize, DynAllocation,
                      MachineInstr::NoFlags);
      } else {
        const YSXRegisterInfo &RI = *STI.getRegisterInfo();
        RI.adjustReg(MBB, MI, DL, SPReg, SPReg, StackOffset::getFixed(Amount),
                     MachineInstr::NoFlags, getStackAlign());
      }
    }
  }

  return MBB.erase(MI);
}

// We would like to split the SP adjustment to reduce prologue/epilogue
// as following instructions. In this way, the offset of the callee saved
// register could fit in a single store. Supposed that the first sp adjust
// amount is 2032.
//   add     sp,sp,-2032
//   sw      ra,2028(sp)
//   sw      s0,2024(sp)
//   sw      s1,2020(sp)
//   sw      s3,2012(sp)
//   sw      s4,2008(sp)
//   add     sp,sp,-64
uint64_t
YSXFrameLowering::getFirstSPAdjustAmount(const MachineFunction &MF) const {
  const auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  const std::vector<CalleeSavedInfo> &CSI = MFI.getCalleeSavedInfo();
  uint64_t StackSize = MFI.getStackSize();

  // Disable SplitSPAdjust if save-restore libcall or push/pop are used.
  // The callee-saved registers will be pushed by the save-restore
  // libcalls, so we don't have to split the SP adjustment in this case.
  if (RVFI->getReservedSpillsSize())
    return 0;

  // Return the FirstSPAdjustAmount if the StackSize can not fit in a signed
  // 12-bit and there exists a callee-saved register needing to be pushed.
  if (!isInt<12>(StackSize) && (CSI.size() > 0)) {
    // FirstSPAdjustAmount is chosen at most as (2048 - StackAlign) because
    // 2048 will cause sp = sp + 2048 in the epilogue to be split into multiple
    // instructions. Offsets smaller than 2048 can fit in a single load/store
    // instruction, and we have to stick with the ysx64 16-byte stack
    // alignment. So (2048 - StackAlign) will satisfy the stack alignment.
    const uint64_t StackAlign = getStackAlign().value();

    return 2048 - StackAlign;
  }
  return 0;
}

bool YSXFrameLowering::assignCalleeSavedSpillSlots(
    MachineFunction &MF, const TargetRegisterInfo *TRI,
    std::vector<CalleeSavedInfo> &CSI) const {
  auto *RVFI = MF.getInfo<YSXMachineFunctionInfo>();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  const TargetRegisterInfo *RegInfo = MF.getSubtarget().getRegisterInfo();

  // Preemptible Interrupts have two additional Callee-save Frame Indexes,
  // not tracked by `CSI`.
  if (RVFI->isSiFivePreemptibleInterrupt(MF)) {
    for (int I = 0; I < 2; ++I) {
      int FI = RVFI->getInterruptCSRFrameIndex(I);
      MFI.setIsCalleeSavedObjectIndex(FI, true);
    }
  }

  // Early exit if no callee saved registers are modified!
  if (CSI.empty())
    return true;

  for (auto &CS : CSI) {
    MCRegister Reg = CS.getReg();
    const TargetRegisterClass *RC = RegInfo->getMinimalPhysRegClass(Reg);
    unsigned Size = RegInfo->getSpillSize(*RC);

    if (RVFI->useSaveRestoreLibCalls(MF)) {
      const auto *FII = llvm::find_if(
          FixedCSRFIMap, [&](MCPhysReg P) { return P == CS.getReg(); });
      unsigned RegNum = std::distance(std::begin(FixedCSRFIMap), FII);

      if (FII != std::end(FixedCSRFIMap)) {
        int64_t Offset = -int64_t(RegNum + 1) * Size;
        int FrameIdx = MFI.CreateFixedSpillStackObject(Size, Offset);
        assert(FrameIdx < 0);
        CS.setFrameIdx(FrameIdx);
        continue;
      }
    }

    // Not a fixed slot.
    Align Alignment = RegInfo->getSpillAlign(*RC);
    // We may not be able to satisfy the desired alignment specification of
    // the TargetRegisterClass if the stack alignment is smaller. Use the
    // min.
    Alignment = std::min(Alignment, getStackAlign());
    int FrameIdx = MFI.CreateStackObject(Size, Alignment, true);
    MFI.setIsCalleeSavedObjectIndex(FrameIdx, true);
    CS.setFrameIdx(FrameIdx);
  }

  if (int LibCallRegs = getLibCallID(MF, CSI) + 1) {
    int64_t LibCallFrameSize =
        alignTo((STI.getXLen() / 8) * LibCallRegs, getStackAlign());
    MFI.CreateFixedSpillStackObject(LibCallFrameSize, -LibCallFrameSize);
  }

  return true;
}

bool YSXFrameLowering::spillCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    ArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction *MF = MBB.getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end() && !MI->isDebugInstr())
    DL = MI->getDebugLoc();

  if (const char *SpillLibCall = getSpillLibCallName(*MF, CSI)) {
    // Add spill libcall via non-callee-saved register t0.
    BuildMI(MBB, MI, DL, TII.get(YSX::PseudoCALLReg), YSX::X5)
        .addExternalSymbol(SpillLibCall, YSXII::MO_CALL)
        .setMIFlag(MachineInstr::FrameSetup);

    // Add registers spilled in libcall as liveins.
    for (auto &CS : CSI)
      MBB.addLiveIn(CS.getReg());
  }

  // Manually spill values not spilled by libcall.
  const auto &UnmanagedCSI = getUnmanagedCSI(*MF, CSI);

  auto storeRegsToStackSlots = [&](decltype(UnmanagedCSI) CSInfo) {
    for (auto &CS : CSInfo) {
      // Insert the spill to the stack frame.
      MCRegister Reg = CS.getReg();
      const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
      TII.storeRegToStackSlot(MBB, MI, Reg, !MBB.isLiveIn(Reg),
                              CS.getFrameIdx(), RC, Register(),
                              MachineInstr::FrameSetup);
    }
  };
  storeRegsToStackSlots(UnmanagedCSI);

  return true;
}

bool YSXFrameLowering::restoreCalleeSavedRegisters(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
    MutableArrayRef<CalleeSavedInfo> CSI, const TargetRegisterInfo *TRI) const {
  if (CSI.empty())
    return true;

  MachineFunction *MF = MBB.getParent();
  const TargetInstrInfo &TII = *MF->getSubtarget().getInstrInfo();
  DebugLoc DL;
  if (MI != MBB.end() && !MI->isDebugInstr())
    DL = MI->getDebugLoc();

  // Manually restore values not restored by libcall.
  // Reverse the restore order in epilog.  In addition, the return
  // address will be restored first in the epilogue. It increases
  // the opportunity to avoid the load-to-use data hazard between
  // loading RA and return by RA.  loadRegFromStackSlot can insert
  // multiple instructions.
  const auto &UnmanagedCSI = getUnmanagedCSI(*MF, CSI);

  auto loadRegFromStackSlot = [&](decltype(UnmanagedCSI) CSInfo) {
    for (auto &CS : CSInfo) {
      MCRegister Reg = CS.getReg();
      const TargetRegisterClass *RC = TRI->getMinimalPhysRegClass(Reg);
      TII.loadRegFromStackSlot(MBB, MI, Reg, CS.getFrameIdx(), RC, Register(),
                               YSX::NoSubRegister,
                               MachineInstr::FrameDestroy);
      assert(MI != MBB.begin() &&
             "loadRegFromStackSlot didn't insert any code!");
    }
  };
  loadRegFromStackSlot(UnmanagedCSI);

  const char *RestoreLibCall = getRestoreLibCallName(*MF, CSI);
  if (RestoreLibCall) {
    // Add restore libcall via tail call.
    MachineBasicBlock::iterator NewMI =
        BuildMI(MBB, MI, DL, TII.get(YSX::PseudoTAIL))
            .addExternalSymbol(RestoreLibCall, YSXII::MO_CALL)
            .setMIFlag(MachineInstr::FrameDestroy);

    // Remove trailing returns, since the terminator is now a tail call to the
    // restore function.
    if (MI != MBB.end() && MI->getOpcode() == YSX::PseudoRET) {
      NewMI->copyImplicitOps(*MF, *MI);
      MI->eraseFromParent();
    }
  }
  return true;
}

bool YSXFrameLowering::enableShrinkWrapping(const MachineFunction &MF) const {
  // Keep the conventional code flow when not optimizing.
  if (MF.getFunction().hasOptNone())
    return false;

  return true;
}

bool YSXFrameLowering::canUseAsPrologue(const MachineBasicBlock &MBB) const {
  MachineBasicBlock *TmpMBB = const_cast<MachineBasicBlock *>(&MBB);
  const MachineFunction *MF = MBB.getParent();
  const auto *RVFI = MF->getInfo<YSXMachineFunctionInfo>();

  if (!RVFI->useSaveRestoreLibCalls(*MF))
    return true;

  // Inserting a call to a __ysx_save libcall requires the use of the register
  // t0 (X5) to hold the return address. Therefore if this register is already
  // used we can't insert the call.

  RegScavenger RS;
  RS.enterBasicBlock(*TmpMBB);
  return !RS.isRegUsed(YSX::X5);
}

bool YSXFrameLowering::canUseAsEpilogue(const MachineBasicBlock &MBB) const {
  const MachineFunction *MF = MBB.getParent();
  MachineBasicBlock *TmpMBB = const_cast<MachineBasicBlock *>(&MBB);
  const auto *RVFI = MF->getInfo<YSXMachineFunctionInfo>();

  if (!RVFI->useSaveRestoreLibCalls(*MF))
    return true;

  // Using the __ysx_restore libcalls to restore CSRs requires a tail call.
  // This means if we still need to continue executing code within this function
  // the restore cannot take place in this basic block.

  if (MBB.succ_size() > 1)
    return false;

  MachineBasicBlock *SuccMBB =
      MBB.succ_empty() ? TmpMBB->getFallThrough() : *MBB.succ_begin();

  // Doing a tail call should be safe if there are no successors, because either
  // we have a returning block or the end of the block is unreachable, so the
  // restore will be eliminated regardless.
  if (!SuccMBB)
    return true;

  // The successor can only contain a return, since we would effectively be
  // replacing the successor with our own tail return at the end of our block.
  return SuccMBB->isReturnBlock() && SuccMBB->size() == 1;
}

bool YSXFrameLowering::isSupportedStackID(TargetStackID::Value ID) const {
  return ID == TargetStackID::Default;
}

// Synthesize the probe loop.
static void emitStackProbeInline(MachineBasicBlock::iterator MBBI, DebugLoc DL,
                                 Register TargetReg) {
  assert(TargetReg != YSX::X2 && "New top of stack cannot already be in SP");

  MachineBasicBlock &MBB = *MBBI->getParent();
  MachineFunction &MF = *MBB.getParent();

  auto &Subtarget = MF.getSubtarget<YSXSubtarget>();
  const YSXInstrInfo *TII = Subtarget.getInstrInfo();
  bool IsRV64 = Subtarget.is64Bit();
  Align StackAlign = Subtarget.getFrameLowering()->getStackAlign();
  const YSXTargetLowering *TLI = Subtarget.getTargetLowering();
  uint64_t ProbeSize = TLI->getStackProbeSize(MF, StackAlign);

  MachineFunction::iterator MBBInsertPoint = std::next(MBB.getIterator());
  MachineBasicBlock *LoopTestMBB =
      MF.CreateMachineBasicBlock(MBB.getBasicBlock());
  MF.insert(MBBInsertPoint, LoopTestMBB);
  MachineBasicBlock *ExitMBB = MF.CreateMachineBasicBlock(MBB.getBasicBlock());
  MF.insert(MBBInsertPoint, ExitMBB);
  MachineInstr::MIFlag Flags = MachineInstr::FrameSetup;
  Register ScratchReg = YSX::X7;

  // ScratchReg = ProbeSize
  TII->movImm(MBB, MBBI, DL, ScratchReg, ProbeSize, Flags);

  // LoopTest:
  //   SUB SP, SP, ProbeSize
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL, TII->get(YSX::SUB), SPReg)
      .addReg(SPReg)
      .addReg(ScratchReg)
      .setMIFlags(Flags);

  //   s[d|w] zero, 0(sp)
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL,
          TII->get(IsRV64 ? YSX::SD : YSX::SW))
      .addReg(YSX::X0)
      .addReg(SPReg)
      .addImm(0)
      .setMIFlags(Flags);

  //  BNE SP, TargetReg, LoopTest
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL, TII->get(YSX::BNE))
      .addReg(SPReg)
      .addReg(TargetReg)
      .addMBB(LoopTestMBB)
      .setMIFlags(Flags);

  ExitMBB->splice(ExitMBB->end(), &MBB, std::next(MBBI), MBB.end());
  ExitMBB->transferSuccessorsAndUpdatePHIs(&MBB);

  LoopTestMBB->addSuccessor(ExitMBB);
  LoopTestMBB->addSuccessor(LoopTestMBB);
  MBB.addSuccessor(LoopTestMBB);
  // Update liveins.
  fullyRecomputeLiveIns({ExitMBB, LoopTestMBB});
}

void YSXFrameLowering::inlineStackProbe(MachineFunction &MF,
                                          MachineBasicBlock &MBB) const {
  // Get the instructions that need to be replaced. We emit at most two of
  // these. Remember them in order to avoid complications coming from the need
  // to traverse the block while potentially creating more blocks.
  SmallVector<MachineInstr *, 4> ToReplace;
  for (MachineInstr &MI : MBB) {
    unsigned Opc = MI.getOpcode();
    if (Opc == YSX::PROBED_STACKALLOC) {
      ToReplace.push_back(&MI);
    }
  }

  for (MachineInstr *MI : ToReplace) {
    if (MI->getOpcode() == YSX::PROBED_STACKALLOC) {
      MachineBasicBlock::iterator MBBI = MI->getIterator();
      DebugLoc DL = MBB.findDebugLoc(MBBI);
      Register TargetReg = MI->getOperand(0).getReg();
      emitStackProbeInline(MBBI, DL, TargetReg);
      MBBI->eraseFromParent();
    }
  }
}

int YSXFrameLowering::getInitialCFAOffset(const MachineFunction &MF) const {
  return 0;
}

Register
YSXFrameLowering::getInitialCFARegister(const MachineFunction &MF) const {
  return YSX::X2;
}
