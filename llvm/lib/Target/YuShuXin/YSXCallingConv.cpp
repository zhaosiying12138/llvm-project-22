//===-- YSXCallingConv.cpp - RISC-V Custom CC Routines ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the custom routines for the RISC-V Calling Convention.
//
//===----------------------------------------------------------------------===//

#include "YSXCallingConv.h"
#include "YSXSubtarget.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCRegister.h"

using namespace llvm;

// Calling Convention Implementation.
// The expectations for frontend ABI lowering vary from target to target.
// Ideally, an LLVM frontend would be able to avoid worrying about many ABI
// details, but this is a longer term goal. For now, we simply try to keep the
// role of the frontend as simple and well-defined as possible. The rules can
// be summarised as:
// * Never split up large scalar arguments. We handle them here.
// * If a hardfloat calling convention is being used, and the struct may be
// passed in a pair of registers (fp+fp, int+fp), and both registers are
// available, then pass as two separate arguments. If either the GPRs or FPRs
// are exhausted, then pass according to the rule below.
// * If a struct could never be passed in registers or directly in a stack
// slot (as it is larger than 2*XLEN and the floating point rules don't
// apply), then pass it using a pointer with the byval attribute.
// * If a struct is less than 2*XLEN, then coerce to either a two-element
// word-sized array or a 2*XLEN scalar (depending on alignment).
// * The frontend can determine whether a struct is returned by reference or
// not based on its size and fields. If it will be returned by reference, the
// frontend must modify the prototype so a pointer with the sret annotation is
// passed as the first argument. This is not necessary for large scalar
// returns.
// * Struct return values and varargs should be coerced to structs containing
// register-size fields in the same situations they would be for fixed
// arguments.

static const MCPhysReg ArgFPR16s[] = {YSX::F10_H, YSX::F11_H, YSX::F12_H,
                                      YSX::F13_H, YSX::F14_H, YSX::F15_H,
                                      YSX::F16_H, YSX::F17_H};
static const MCPhysReg ArgFPR32s[] = {YSX::F10_F, YSX::F11_F, YSX::F12_F,
                                      YSX::F13_F, YSX::F14_F, YSX::F15_F,
                                      YSX::F16_F, YSX::F17_F};
static const MCPhysReg ArgFPR64s[] = {YSX::F10_D, YSX::F11_D, YSX::F12_D,
                                      YSX::F13_D, YSX::F14_D, YSX::F15_D,
                                      YSX::F16_D, YSX::F17_D};
// This is an interim calling convention and it may be changed in the future.
static const MCPhysReg ArgVRs[] = {
    YSX::V8,  YSX::V9,  YSX::V10, YSX::V11, YSX::V12, YSX::V13,
    YSX::V14, YSX::V15, YSX::V16, YSX::V17, YSX::V18, YSX::V19,
    YSX::V20, YSX::V21, YSX::V22, YSX::V23};
static const MCPhysReg ArgVRM2s[] = {YSX::V8M2,  YSX::V10M2, YSX::V12M2,
                                     YSX::V14M2, YSX::V16M2, YSX::V18M2,
                                     YSX::V20M2, YSX::V22M2};
static const MCPhysReg ArgVRM4s[] = {YSX::V8M4, YSX::V12M4, YSX::V16M4,
                                     YSX::V20M4};
static const MCPhysReg ArgVRM8s[] = {YSX::V8M8, YSX::V16M8};
static const MCPhysReg ArgVRN2M1s[] = {
    YSX::V8_V9,   YSX::V9_V10,  YSX::V10_V11, YSX::V11_V12,
    YSX::V12_V13, YSX::V13_V14, YSX::V14_V15, YSX::V15_V16,
    YSX::V16_V17, YSX::V17_V18, YSX::V18_V19, YSX::V19_V20,
    YSX::V20_V21, YSX::V21_V22, YSX::V22_V23};
static const MCPhysReg ArgVRN3M1s[] = {
    YSX::V8_V9_V10,   YSX::V9_V10_V11,  YSX::V10_V11_V12,
    YSX::V11_V12_V13, YSX::V12_V13_V14, YSX::V13_V14_V15,
    YSX::V14_V15_V16, YSX::V15_V16_V17, YSX::V16_V17_V18,
    YSX::V17_V18_V19, YSX::V18_V19_V20, YSX::V19_V20_V21,
    YSX::V20_V21_V22, YSX::V21_V22_V23};
static const MCPhysReg ArgVRN4M1s[] = {
    YSX::V8_V9_V10_V11,   YSX::V9_V10_V11_V12,  YSX::V10_V11_V12_V13,
    YSX::V11_V12_V13_V14, YSX::V12_V13_V14_V15, YSX::V13_V14_V15_V16,
    YSX::V14_V15_V16_V17, YSX::V15_V16_V17_V18, YSX::V16_V17_V18_V19,
    YSX::V17_V18_V19_V20, YSX::V18_V19_V20_V21, YSX::V19_V20_V21_V22,
    YSX::V20_V21_V22_V23};
static const MCPhysReg ArgVRN5M1s[] = {
    YSX::V8_V9_V10_V11_V12,   YSX::V9_V10_V11_V12_V13,
    YSX::V10_V11_V12_V13_V14, YSX::V11_V12_V13_V14_V15,
    YSX::V12_V13_V14_V15_V16, YSX::V13_V14_V15_V16_V17,
    YSX::V14_V15_V16_V17_V18, YSX::V15_V16_V17_V18_V19,
    YSX::V16_V17_V18_V19_V20, YSX::V17_V18_V19_V20_V21,
    YSX::V18_V19_V20_V21_V22, YSX::V19_V20_V21_V22_V23};
static const MCPhysReg ArgVRN6M1s[] = {
    YSX::V8_V9_V10_V11_V12_V13,   YSX::V9_V10_V11_V12_V13_V14,
    YSX::V10_V11_V12_V13_V14_V15, YSX::V11_V12_V13_V14_V15_V16,
    YSX::V12_V13_V14_V15_V16_V17, YSX::V13_V14_V15_V16_V17_V18,
    YSX::V14_V15_V16_V17_V18_V19, YSX::V15_V16_V17_V18_V19_V20,
    YSX::V16_V17_V18_V19_V20_V21, YSX::V17_V18_V19_V20_V21_V22,
    YSX::V18_V19_V20_V21_V22_V23};
static const MCPhysReg ArgVRN7M1s[] = {
    YSX::V8_V9_V10_V11_V12_V13_V14,   YSX::V9_V10_V11_V12_V13_V14_V15,
    YSX::V10_V11_V12_V13_V14_V15_V16, YSX::V11_V12_V13_V14_V15_V16_V17,
    YSX::V12_V13_V14_V15_V16_V17_V18, YSX::V13_V14_V15_V16_V17_V18_V19,
    YSX::V14_V15_V16_V17_V18_V19_V20, YSX::V15_V16_V17_V18_V19_V20_V21,
    YSX::V16_V17_V18_V19_V20_V21_V22, YSX::V17_V18_V19_V20_V21_V22_V23};
static const MCPhysReg ArgVRN8M1s[] = {YSX::V8_V9_V10_V11_V12_V13_V14_V15,
                                       YSX::V9_V10_V11_V12_V13_V14_V15_V16,
                                       YSX::V10_V11_V12_V13_V14_V15_V16_V17,
                                       YSX::V11_V12_V13_V14_V15_V16_V17_V18,
                                       YSX::V12_V13_V14_V15_V16_V17_V18_V19,
                                       YSX::V13_V14_V15_V16_V17_V18_V19_V20,
                                       YSX::V14_V15_V16_V17_V18_V19_V20_V21,
                                       YSX::V15_V16_V17_V18_V19_V20_V21_V22,
                                       YSX::V16_V17_V18_V19_V20_V21_V22_V23};
static const MCPhysReg ArgVRN2M2s[] = {YSX::V8M2_V10M2,  YSX::V10M2_V12M2,
                                       YSX::V12M2_V14M2, YSX::V14M2_V16M2,
                                       YSX::V16M2_V18M2, YSX::V18M2_V20M2,
                                       YSX::V20M2_V22M2};
static const MCPhysReg ArgVRN3M2s[] = {
    YSX::V8M2_V10M2_V12M2,  YSX::V10M2_V12M2_V14M2,
    YSX::V12M2_V14M2_V16M2, YSX::V14M2_V16M2_V18M2,
    YSX::V16M2_V18M2_V20M2, YSX::V18M2_V20M2_V22M2};
static const MCPhysReg ArgVRN4M2s[] = {
    YSX::V8M2_V10M2_V12M2_V14M2, YSX::V10M2_V12M2_V14M2_V16M2,
    YSX::V12M2_V14M2_V16M2_V18M2, YSX::V14M2_V16M2_V18M2_V20M2,
    YSX::V16M2_V18M2_V20M2_V22M2};
static const MCPhysReg ArgVRN2M4s[] = {YSX::V8M4_V12M4, YSX::V12M4_V16M4,
                                       YSX::V16M4_V20M4};

ArrayRef<MCPhysReg> YSX::getArgGPRs(const YSXABI::ABI ABI) {
  // The GPRs used for passing arguments in the ILP32* and LP64* ABIs, except
  // the ILP32E ABI.
  static const MCPhysReg ArgIGPRs[] = {YSX::X10, YSX::X11, YSX::X12,
                                       YSX::X13, YSX::X14, YSX::X15,
                                       YSX::X16, YSX::X17};
  // The GPRs used for passing arguments in the ILP32E/LP64E ABI.
  static const MCPhysReg ArgEGPRs[] = {YSX::X10, YSX::X11, YSX::X12,
                                       YSX::X13, YSX::X14, YSX::X15};

  if (ABI == YSXABI::ABI_ILP32E || ABI == YSXABI::ABI_LP64E)
    return ArrayRef(ArgEGPRs);

  return ArrayRef(ArgIGPRs);
}

static ArrayRef<MCPhysReg> getArgGPR16s(const YSXABI::ABI ABI) {
  // The GPRs used for passing arguments in the ILP32* and LP64* ABIs, except
  // the ILP32E ABI.
  static const MCPhysReg ArgIGPRs[] = {YSX::X10_H, YSX::X11_H, YSX::X12_H,
                                       YSX::X13_H, YSX::X14_H, YSX::X15_H,
                                       YSX::X16_H, YSX::X17_H};
  // The GPRs used for passing arguments in the ILP32E/LP64E ABI.
  static const MCPhysReg ArgEGPRs[] = {YSX::X10_H, YSX::X11_H,
                                       YSX::X12_H, YSX::X13_H,
                                       YSX::X14_H, YSX::X15_H};

  if (ABI == YSXABI::ABI_ILP32E || ABI == YSXABI::ABI_LP64E)
    return ArrayRef(ArgEGPRs);

  return ArrayRef(ArgIGPRs);
}

static ArrayRef<MCPhysReg> getArgGPR32s(const YSXABI::ABI ABI) {
  // The GPRs used for passing arguments in the ILP32* and LP64* ABIs, except
  // the ILP32E ABI.
  static const MCPhysReg ArgIGPRs[] = {YSX::X10_W, YSX::X11_W, YSX::X12_W,
                                       YSX::X13_W, YSX::X14_W, YSX::X15_W,
                                       YSX::X16_W, YSX::X17_W};
  // The GPRs used for passing arguments in the ILP32E/LP64E ABI.
  static const MCPhysReg ArgEGPRs[] = {YSX::X10_W, YSX::X11_W,
                                       YSX::X12_W, YSX::X13_W,
                                       YSX::X14_W, YSX::X15_W};

  if (ABI == YSXABI::ABI_ILP32E || ABI == YSXABI::ABI_LP64E)
    return ArrayRef(ArgEGPRs);

  return ArrayRef(ArgIGPRs);
}

static ArrayRef<MCPhysReg> getFastCCArgGPRs(const YSXABI::ABI ABI) {
  // The GPRs used for passing arguments in the FastCC, X5 and X6 might be used
  // for save-restore libcall, so we don't use them.
  // Don't use X7 for fastcc, since Zicfilp uses X7 as the label register.
  static const MCPhysReg FastCCIGPRs[] = {
      YSX::X10, YSX::X11, YSX::X12, YSX::X13, YSX::X14, YSX::X15,
      YSX::X16, YSX::X17, YSX::X28, YSX::X29, YSX::X30, YSX::X31};

  // The GPRs used for passing arguments in the FastCC when using ILP32E/LP64E.
  static const MCPhysReg FastCCEGPRs[] = {YSX::X10, YSX::X11, YSX::X12,
                                          YSX::X13, YSX::X14, YSX::X15};

  if (ABI == YSXABI::ABI_ILP32E || ABI == YSXABI::ABI_LP64E)
    return ArrayRef(FastCCEGPRs);

  return ArrayRef(FastCCIGPRs);
}

static ArrayRef<MCPhysReg> getFastCCArgGPRF16s(const YSXABI::ABI ABI) {
  // The GPRs used for passing arguments in the FastCC, X5 and X6 might be used
  // for save-restore libcall, so we don't use them.
  // Don't use X7 for fastcc, since Zicfilp uses X7 as the label register.
  static const MCPhysReg FastCCIGPRs[] = {
      YSX::X10_H, YSX::X11_H, YSX::X12_H, YSX::X13_H,
      YSX::X14_H, YSX::X15_H, YSX::X16_H, YSX::X17_H,
      YSX::X28_H, YSX::X29_H, YSX::X30_H, YSX::X31_H};

  // The GPRs used for passing arguments in the FastCC when using ILP32E/LP64E.
  static const MCPhysReg FastCCEGPRs[] = {YSX::X10_H, YSX::X11_H,
                                          YSX::X12_H, YSX::X13_H,
                                          YSX::X14_H, YSX::X15_H};

  if (ABI == YSXABI::ABI_ILP32E || ABI == YSXABI::ABI_LP64E)
    return ArrayRef(FastCCEGPRs);

  return ArrayRef(FastCCIGPRs);
}

static ArrayRef<MCPhysReg> getFastCCArgGPRF32s(const YSXABI::ABI ABI) {
  // The GPRs used for passing arguments in the FastCC, X5 and X6 might be used
  // for save-restore libcall, so we don't use them.
  // Don't use X7 for fastcc, since Zicfilp uses X7 as the label register.
  static const MCPhysReg FastCCIGPRs[] = {
      YSX::X10_W, YSX::X11_W, YSX::X12_W, YSX::X13_W,
      YSX::X14_W, YSX::X15_W, YSX::X16_W, YSX::X17_W,
      YSX::X28_W, YSX::X29_W, YSX::X30_W, YSX::X31_W};

  // The GPRs used for passing arguments in the FastCC when using ILP32E/LP64E.
  static const MCPhysReg FastCCEGPRs[] = {YSX::X10_W, YSX::X11_W,
                                          YSX::X12_W, YSX::X13_W,
                                          YSX::X14_W, YSX::X15_W};

  if (ABI == YSXABI::ABI_ILP32E || ABI == YSXABI::ABI_LP64E)
    return ArrayRef(FastCCEGPRs);

  return ArrayRef(FastCCIGPRs);
}

// Pass a 2*XLEN argument that has been split into two XLEN values through
// registers or the stack as necessary.
static bool CC_YSXAssign2XLen(unsigned XLen, CCState &State, CCValAssign VA1,
                                ISD::ArgFlagsTy ArgFlags1, unsigned ValNo2,
                                MVT ValVT2, MVT LocVT2,
                                ISD::ArgFlagsTy ArgFlags2, bool EABI) {
  unsigned XLenInBytes = XLen / 8;
  const YSXSubtarget &STI =
      State.getMachineFunction().getSubtarget<YSXSubtarget>();
  ArrayRef<MCPhysReg> ArgGPRs = YSX::getArgGPRs(STI.getTargetABI());

  if (MCRegister Reg = State.AllocateReg(ArgGPRs)) {
    // At least one half can be passed via register.
    State.addLoc(CCValAssign::getReg(VA1.getValNo(), VA1.getValVT(), Reg,
                                     VA1.getLocVT(), CCValAssign::Full));
  } else {
    // Both halves must be passed on the stack, with proper alignment.
    // TODO: To be compatible with GCC's behaviors, we force them to have 4-byte
    // alignment. This behavior may be changed when RV32E/ILP32E is ratified.
    Align StackAlign(XLenInBytes);
    if (!EABI || XLen != 32)
      StackAlign = std::max(StackAlign, ArgFlags1.getNonZeroOrigAlign());
    State.addLoc(
        CCValAssign::getMem(VA1.getValNo(), VA1.getValVT(),
                            State.AllocateStack(XLenInBytes, StackAlign),
                            VA1.getLocVT(), CCValAssign::Full));
    State.addLoc(CCValAssign::getMem(
        ValNo2, ValVT2, State.AllocateStack(XLenInBytes, Align(XLenInBytes)),
        LocVT2, CCValAssign::Full));
    return false;
  }

  if (MCRegister Reg = State.AllocateReg(ArgGPRs)) {
    // The second half can also be passed via register.
    State.addLoc(
        CCValAssign::getReg(ValNo2, ValVT2, Reg, LocVT2, CCValAssign::Full));
  } else {
    // The second half is passed via the stack, without additional alignment.
    State.addLoc(CCValAssign::getMem(
        ValNo2, ValVT2, State.AllocateStack(XLenInBytes, Align(XLenInBytes)),
        LocVT2, CCValAssign::Full));
  }

  return false;
}

static MCRegister allocateRVVReg(MVT ValVT, unsigned ValNo, CCState &State,
                                 const YSXTargetLowering &TLI) {
  const TargetRegisterClass *RC = TLI.getRegClassFor(ValVT);
  if (RC == &YSX::VRRegClass) {
    // Assign the first mask argument to V0.
    // This is an interim calling convention and it may be changed in the
    // future.
    if (ValVT.getVectorElementType() == MVT::i1)
      if (MCRegister Reg = State.AllocateReg(YSX::V0))
        return Reg;
    return State.AllocateReg(ArgVRs);
  }
  if (RC == &YSX::VRM2RegClass)
    return State.AllocateReg(ArgVRM2s);
  if (RC == &YSX::VRM4RegClass)
    return State.AllocateReg(ArgVRM4s);
  if (RC == &YSX::VRM8RegClass)
    return State.AllocateReg(ArgVRM8s);
  if (RC == &YSX::VRN2M1RegClass)
    return State.AllocateReg(ArgVRN2M1s);
  if (RC == &YSX::VRN3M1RegClass)
    return State.AllocateReg(ArgVRN3M1s);
  if (RC == &YSX::VRN4M1RegClass)
    return State.AllocateReg(ArgVRN4M1s);
  if (RC == &YSX::VRN5M1RegClass)
    return State.AllocateReg(ArgVRN5M1s);
  if (RC == &YSX::VRN6M1RegClass)
    return State.AllocateReg(ArgVRN6M1s);
  if (RC == &YSX::VRN7M1RegClass)
    return State.AllocateReg(ArgVRN7M1s);
  if (RC == &YSX::VRN8M1RegClass)
    return State.AllocateReg(ArgVRN8M1s);
  if (RC == &YSX::VRN2M2RegClass)
    return State.AllocateReg(ArgVRN2M2s);
  if (RC == &YSX::VRN3M2RegClass)
    return State.AllocateReg(ArgVRN3M2s);
  if (RC == &YSX::VRN4M2RegClass)
    return State.AllocateReg(ArgVRN4M2s);
  if (RC == &YSX::VRN2M4RegClass)
    return State.AllocateReg(ArgVRN2M4s);
  llvm_unreachable("Unhandled register class for ValueType");
}

// Implements the RISC-V calling convention. Returns true upon failure.
bool llvm::CC_YSX(unsigned ValNo, MVT ValVT, MVT LocVT,
                    CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                    CCState &State, bool IsRet, Type *OrigTy) {
  const MachineFunction &MF = State.getMachineFunction();
  const DataLayout &DL = MF.getDataLayout();
  const YSXSubtarget &Subtarget = MF.getSubtarget<YSXSubtarget>();
  const YSXTargetLowering &TLI = *Subtarget.getTargetLowering();

  unsigned XLen = Subtarget.getXLen();
  MVT XLenVT = Subtarget.getXLenVT();

  if (ArgFlags.isNest()) {
    // Static chain parameter must not be passed in normal argument registers,
    // so we assign t2/t3 for it as done in GCC's
    // __builtin_call_with_static_chain
    bool HasCFBranch =
        Subtarget.hasStdExtZicfilp() &&
        MF.getFunction().getParent()->getModuleFlag("cf-protection-branch");

    // Normal: t2, Branch control flow protection: t3
    const auto StaticChainReg = HasCFBranch ? YSX::X28 : YSX::X7;

    YSXABI::ABI ABI = Subtarget.getTargetABI();
    if (HasCFBranch &&
        (ABI == YSXABI::ABI_ILP32E || ABI == YSXABI::ABI_LP64E))
      reportFatalUsageError(
          "Nested functions with control flow protection are not "
          "usable with ILP32E or LP64E ABI.");
    if (MCRegister Reg = State.AllocateReg(StaticChainReg)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  // Any return value split in to more than two values can't be returned
  // directly. Vectors are returned via the available vector registers.
  if (!LocVT.isVector() && IsRet && ValNo > 1)
    return true;

  // UseGPRForF16_F32 if targeting one of the soft-float ABIs, if passing a
  // variadic argument, or if no F16/F32 argument registers are available.
  bool UseGPRForF16_F32 = true;
  // UseGPRForF64 if targeting soft-float ABIs or an FLEN=32 ABI, if passing a
  // variadic argument, or if no F64 argument registers are available.
  bool UseGPRForF64 = true;

  YSXABI::ABI ABI = Subtarget.getTargetABI();
  switch (ABI) {
  default:
    llvm_unreachable("Unexpected ABI");
  case YSXABI::ABI_ILP32:
  case YSXABI::ABI_ILP32E:
  case YSXABI::ABI_LP64:
  case YSXABI::ABI_LP64E:
    break;
  case YSXABI::ABI_ILP32F:
  case YSXABI::ABI_LP64F:
    UseGPRForF16_F32 = ArgFlags.isVarArg();
    break;
  case YSXABI::ABI_ILP32D:
  case YSXABI::ABI_LP64D:
    UseGPRForF16_F32 = ArgFlags.isVarArg();
    UseGPRForF64 = ArgFlags.isVarArg();
    break;
  }

  if ((LocVT == MVT::f16 || LocVT == MVT::bf16) && !UseGPRForF16_F32) {
    if (MCRegister Reg = State.AllocateReg(ArgFPR16s)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f32 && !UseGPRForF16_F32) {
    if (MCRegister Reg = State.AllocateReg(ArgFPR32s)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f64 && !UseGPRForF64) {
    if (MCRegister Reg = State.AllocateReg(ArgFPR64s)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if ((ValVT == MVT::f16 && Subtarget.hasStdExtZhinxmin())) {
    if (MCRegister Reg = State.AllocateReg(getArgGPR16s(ABI))) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (ValVT == MVT::f32 && Subtarget.hasStdExtZfinx()) {
    if (MCRegister Reg = State.AllocateReg(getArgGPR32s(ABI))) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  ArrayRef<MCPhysReg> ArgGPRs = YSX::getArgGPRs(ABI);

  // Zdinx use GPR without a bitcast when possible.
  if (LocVT == MVT::f64 && XLen == 64 && Subtarget.hasStdExtZdinx()) {
    if (MCRegister Reg = State.AllocateReg(ArgGPRs)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  // FP smaller than XLen, uses custom GPR.
  if (LocVT == MVT::f16 || LocVT == MVT::bf16 ||
      (LocVT == MVT::f32 && XLen == 64)) {
    if (MCRegister Reg = State.AllocateReg(ArgGPRs)) {
      LocVT = XLenVT;
      State.addLoc(
          CCValAssign::getCustomReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  // Bitcast FP to GPR if we can use a GPR register.
  if ((XLen == 32 && LocVT == MVT::f32) || (XLen == 64 && LocVT == MVT::f64)) {
    if (MCRegister Reg = State.AllocateReg(ArgGPRs)) {
      LocVT = XLenVT;
      LocInfo = CCValAssign::BCvt;
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  // If this is a variadic argument, the RISC-V calling convention requires
  // that it is assigned an 'even' or 'aligned' register if it has 8-byte
  // alignment (RV32) or 16-byte alignment (RV64). An aligned register should
  // be used regardless of whether the original argument was split during
  // legalisation or not. The argument will not be passed by registers if the
  // original type is larger than 2*XLEN, so the register alignment rule does
  // not apply.
  // TODO: To be compatible with GCC's behaviors, we don't align registers
  // currently if we are using ILP32E calling convention. This behavior may be
  // changed when RV32E/ILP32E is ratified.
  unsigned TwoXLenInBytes = (2 * XLen) / 8;
  if (ArgFlags.isVarArg() && ArgFlags.getNonZeroOrigAlign() == TwoXLenInBytes &&
      DL.getTypeAllocSize(OrigTy) == TwoXLenInBytes &&
      ABI != YSXABI::ABI_ILP32E) {
    unsigned RegIdx = State.getFirstUnallocated(ArgGPRs);
    // Skip 'odd' register if necessary.
    if (RegIdx != std::size(ArgGPRs) && RegIdx % 2 == 1)
      State.AllocateReg(ArgGPRs);
  }

  SmallVectorImpl<CCValAssign> &PendingLocs = State.getPendingLocs();
  SmallVectorImpl<ISD::ArgFlagsTy> &PendingArgFlags =
      State.getPendingArgFlags();

  assert(PendingLocs.size() == PendingArgFlags.size() &&
         "PendingLocs and PendingArgFlags out of sync");

  // Handle passing f64 on RV32D with a soft float ABI or when floating point
  // registers are exhausted.
  if (XLen == 32 && LocVT == MVT::f64) {
    assert(PendingLocs.empty() && "Can't lower f64 if it is split");
    // Depending on available argument GPRS, f64 may be passed in a pair of
    // GPRs, split between a GPR and the stack, or passed completely on the
    // stack. LowerCall/LowerFormalArguments/LowerReturn must recognise these
    // cases.
    MCRegister Reg = State.AllocateReg(ArgGPRs);
    if (!Reg) {
      int64_t StackOffset = State.AllocateStack(8, Align(8));
      State.addLoc(
          CCValAssign::getMem(ValNo, ValVT, StackOffset, LocVT, LocInfo));
      return false;
    }
    LocVT = MVT::i32;
    State.addLoc(CCValAssign::getCustomReg(ValNo, ValVT, Reg, LocVT, LocInfo));
    MCRegister HiReg = State.AllocateReg(ArgGPRs);
    if (HiReg) {
      State.addLoc(
          CCValAssign::getCustomReg(ValNo, ValVT, HiReg, LocVT, LocInfo));
    } else {
      int64_t StackOffset = State.AllocateStack(4, Align(4));
      State.addLoc(
          CCValAssign::getCustomMem(ValNo, ValVT, StackOffset, LocVT, LocInfo));
    }
    return false;
  }

  // Split arguments might be passed indirectly, so keep track of the pending
  // values. Split vectors are passed via a mix of registers and indirectly, so
  // treat them as we would any other argument.
  if (ValVT.isScalarInteger() && (ArgFlags.isSplit() || !PendingLocs.empty())) {
    LocVT = XLenVT;
    LocInfo = CCValAssign::Indirect;
    PendingLocs.push_back(
        CCValAssign::getPending(ValNo, ValVT, LocVT, LocInfo));
    PendingArgFlags.push_back(ArgFlags);
    if (!ArgFlags.isSplitEnd()) {
      return false;
    }
  }

  // If the split argument only had two elements, it should be passed directly
  // in registers or on the stack.
  if (ValVT.isScalarInteger() && ArgFlags.isSplitEnd() &&
      PendingLocs.size() <= 2) {
    assert(PendingLocs.size() == 2 && "Unexpected PendingLocs.size()");
    // Apply the normal calling convention rules to the first half of the
    // split argument.
    CCValAssign VA = PendingLocs[0];
    ISD::ArgFlagsTy AF = PendingArgFlags[0];
    PendingLocs.clear();
    PendingArgFlags.clear();
    return CC_YSXAssign2XLen(
        XLen, State, VA, AF, ValNo, ValVT, LocVT, ArgFlags,
        ABI == YSXABI::ABI_ILP32E || ABI == YSXABI::ABI_LP64E);
  }

  // Allocate to a register if possible, or else a stack slot.
  MCRegister Reg;
  unsigned StoreSizeBytes = XLen / 8;
  Align StackAlign = Align(XLen / 8);

  if (ValVT.isVector() || ValVT.isRISCVVectorTuple()) {
    Reg = allocateRVVReg(ValVT, ValNo, State, TLI);
    if (Reg) {
      // Fixed-length vectors are located in the corresponding scalable-vector
      // container types.
      if (ValVT.isFixedLengthVector()) {
        LocVT = TLI.getContainerForFixedLengthVector(LocVT);
        State.addLoc(
            CCValAssign::getCustomReg(ValNo, ValVT, Reg, LocVT, LocInfo));
        return false;
      }
    } else {
      // For return values, the vector must be passed fully via registers or
      // via the stack.
      // FIXME: The proposed vector ABI only mandates v8-v15 for return values,
      // but we're using all of them.
      if (IsRet)
        return true;
      // Try using a GPR to pass the address
      if ((Reg = State.AllocateReg(ArgGPRs))) {
        LocVT = XLenVT;
        LocInfo = CCValAssign::Indirect;
      } else if (ValVT.isScalableVector()) {
        LocVT = XLenVT;
        LocInfo = CCValAssign::Indirect;
      } else {
        StoreSizeBytes = ValVT.getStoreSize();
        // Align vectors to their element sizes, being careful for vXi1
        // vectors.
        StackAlign = MaybeAlign(ValVT.getScalarSizeInBits() / 8).valueOrOne();
      }
    }
  } else {
    Reg = State.AllocateReg(ArgGPRs);
  }

  int64_t StackOffset =
      Reg ? 0 : State.AllocateStack(StoreSizeBytes, StackAlign);

  // If we reach this point and PendingLocs is non-empty, we must be at the
  // end of a split argument that must be passed indirectly.
  if (!PendingLocs.empty()) {
    assert(ArgFlags.isSplitEnd() && "Expected ArgFlags.isSplitEnd()");
    assert(PendingLocs.size() > 2 && "Unexpected PendingLocs.size()");

    for (auto &It : PendingLocs) {
      if (Reg)
        It.convertToReg(Reg);
      else
        It.convertToMem(StackOffset);
      State.addLoc(It);
    }
    PendingLocs.clear();
    PendingArgFlags.clear();
    return false;
  }

  assert(((ValVT.isFloatingPoint() && !ValVT.isVector()) || LocVT == XLenVT ||
          (TLI.getSubtarget().hasVInstructions() &&
           (ValVT.isVector() || ValVT.isRISCVVectorTuple()))) &&
         "Expected an XLenVT or vector types at this stage");

  if (Reg) {
    State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
    return false;
  }

  State.addLoc(CCValAssign::getMem(ValNo, ValVT, StackOffset, LocVT, LocInfo));
  return false;
}

// FastCC has less than 1% performance improvement for some particular
// benchmark. But theoretically, it may have benefit for some cases.
bool llvm::CC_YSX_FastCC(unsigned ValNo, MVT ValVT, MVT LocVT,
                           CCValAssign::LocInfo LocInfo,
                           ISD::ArgFlagsTy ArgFlags, CCState &State, bool IsRet,
                           Type *OrigTy) {
  const MachineFunction &MF = State.getMachineFunction();
  const YSXSubtarget &Subtarget = MF.getSubtarget<YSXSubtarget>();
  const YSXTargetLowering &TLI = *Subtarget.getTargetLowering();
  YSXABI::ABI ABI = Subtarget.getTargetABI();

  if ((LocVT == MVT::f16 && Subtarget.hasStdExtZfhmin()) ||
      (LocVT == MVT::bf16 && Subtarget.hasStdExtZfbfmin())) {
    static const MCPhysReg FPR16List[] = {
        YSX::F10_H, YSX::F11_H, YSX::F12_H, YSX::F13_H, YSX::F14_H,
        YSX::F15_H, YSX::F16_H, YSX::F17_H, YSX::F0_H,  YSX::F1_H,
        YSX::F2_H,  YSX::F3_H,  YSX::F4_H,  YSX::F5_H,  YSX::F6_H,
        YSX::F7_H,  YSX::F28_H, YSX::F29_H, YSX::F30_H, YSX::F31_H};
    if (MCRegister Reg = State.AllocateReg(FPR16List)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f32 && Subtarget.hasStdExtF()) {
    static const MCPhysReg FPR32List[] = {
        YSX::F10_F, YSX::F11_F, YSX::F12_F, YSX::F13_F, YSX::F14_F,
        YSX::F15_F, YSX::F16_F, YSX::F17_F, YSX::F0_F,  YSX::F1_F,
        YSX::F2_F,  YSX::F3_F,  YSX::F4_F,  YSX::F5_F,  YSX::F6_F,
        YSX::F7_F,  YSX::F28_F, YSX::F29_F, YSX::F30_F, YSX::F31_F};
    if (MCRegister Reg = State.AllocateReg(FPR32List)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f64 && Subtarget.hasStdExtD()) {
    static const MCPhysReg FPR64List[] = {
        YSX::F10_D, YSX::F11_D, YSX::F12_D, YSX::F13_D, YSX::F14_D,
        YSX::F15_D, YSX::F16_D, YSX::F17_D, YSX::F0_D,  YSX::F1_D,
        YSX::F2_D,  YSX::F3_D,  YSX::F4_D,  YSX::F5_D,  YSX::F6_D,
        YSX::F7_D,  YSX::F28_D, YSX::F29_D, YSX::F30_D, YSX::F31_D};
    if (MCRegister Reg = State.AllocateReg(FPR64List)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  MVT XLenVT = Subtarget.getXLenVT();

  // Check if there is an available GPRF16 before hitting the stack.
  if ((LocVT == MVT::f16 && Subtarget.hasStdExtZhinxmin())) {
    if (MCRegister Reg = State.AllocateReg(getFastCCArgGPRF16s(ABI))) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  // Check if there is an available GPRF32 before hitting the stack.
  if (LocVT == MVT::f32 && Subtarget.hasStdExtZfinx()) {
    if (MCRegister Reg = State.AllocateReg(getFastCCArgGPRF32s(ABI))) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  // Check if there is an available GPR before hitting the stack.
  if (LocVT == MVT::f64 && Subtarget.is64Bit() && Subtarget.hasStdExtZdinx()) {
    if (MCRegister Reg = State.AllocateReg(getFastCCArgGPRs(ABI))) {
      if (LocVT.getSizeInBits() != Subtarget.getXLen()) {
        LocVT = XLenVT;
        State.addLoc(
            CCValAssign::getCustomReg(ValNo, ValVT, Reg, LocVT, LocInfo));
        return false;
      }
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  ArrayRef<MCPhysReg> ArgGPRs = getFastCCArgGPRs(ABI);

  if (LocVT.isVector()) {
    if (MCRegister Reg = allocateRVVReg(ValVT, ValNo, State, TLI)) {
      // Fixed-length vectors are located in the corresponding scalable-vector
      // container types.
      if (LocVT.isFixedLengthVector()) {
        LocVT = TLI.getContainerForFixedLengthVector(LocVT);
        State.addLoc(
            CCValAssign::getCustomReg(ValNo, ValVT, Reg, LocVT, LocInfo));
        return false;
      }
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }

    // Pass scalable vectors indirectly. Pass fixed vectors indirectly if we
    // have a free GPR.
    if (LocVT.isScalableVector() ||
        State.getFirstUnallocated(ArgGPRs) != ArgGPRs.size()) {
      LocInfo = CCValAssign::Indirect;
      LocVT = XLenVT;
    }
  }

  if (LocVT == XLenVT) {
    if (MCRegister Reg = State.AllocateReg(getFastCCArgGPRs(ABI))) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == XLenVT || LocVT == MVT::f16 || LocVT == MVT::bf16 ||
      LocVT == MVT::f32 || LocVT == MVT::f64 || LocVT.isFixedLengthVector()) {
    Align StackAlign = MaybeAlign(ValVT.getScalarSizeInBits() / 8).valueOrOne();
    int64_t Offset = State.AllocateStack(LocVT.getStoreSize(), StackAlign);
    State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset, LocVT, LocInfo));
    return false;
  }

  return true; // CC didn't match.
}

bool llvm::CC_YSX_GHC(unsigned ValNo, MVT ValVT, MVT LocVT,
                        CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                        Type *OrigTy, CCState &State) {
  if (ArgFlags.isNest()) {
    report_fatal_error(
        "Attribute 'nest' is not supported in GHC calling convention");
  }

  static const MCPhysReg GPRList[] = {
      YSX::X9,  YSX::X18, YSX::X19, YSX::X20, YSX::X21, YSX::X22,
      YSX::X23, YSX::X24, YSX::X25, YSX::X26, YSX::X27};

  if (LocVT == MVT::i32 || LocVT == MVT::i64) {
    // Pass in STG registers: Base, Sp, Hp, R1, R2, R3, R4, R5, R6, R7, SpLim
    //                        s1    s2  s3  s4  s5  s6  s7  s8  s9  s10 s11
    if (MCRegister Reg = State.AllocateReg(GPRList)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  const YSXSubtarget &Subtarget =
      State.getMachineFunction().getSubtarget<YSXSubtarget>();

  if (LocVT == MVT::f32 && Subtarget.hasStdExtF()) {
    // Pass in STG registers: F1, ..., F6
    //                        fs0 ... fs5
    static const MCPhysReg FPR32List[] = {YSX::F8_F,  YSX::F9_F,
                                          YSX::F18_F, YSX::F19_F,
                                          YSX::F20_F, YSX::F21_F};
    if (MCRegister Reg = State.AllocateReg(FPR32List)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f64 && Subtarget.hasStdExtD()) {
    // Pass in STG registers: D1, ..., D6
    //                        fs6 ... fs11
    static const MCPhysReg FPR64List[] = {YSX::F22_D, YSX::F23_D,
                                          YSX::F24_D, YSX::F25_D,
                                          YSX::F26_D, YSX::F27_D};
    if (MCRegister Reg = State.AllocateReg(FPR64List)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f32 && Subtarget.hasStdExtZfinx()) {
    static const MCPhysReg GPR32List[] = {
        YSX::X9_W,  YSX::X18_W, YSX::X19_W, YSX::X20_W,
        YSX::X21_W, YSX::X22_W, YSX::X23_W, YSX::X24_W,
        YSX::X25_W, YSX::X26_W, YSX::X27_W};
    if (MCRegister Reg = State.AllocateReg(GPR32List)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  if (LocVT == MVT::f64 && Subtarget.hasStdExtZdinx() && Subtarget.is64Bit()) {
    if (MCRegister Reg = State.AllocateReg(GPRList)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  report_fatal_error("No registers left in GHC calling convention");
  return true;
}
