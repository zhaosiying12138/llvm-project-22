//===-- YSXCallingConv.cpp - YuShuXin Custom CC Routines -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "YSXCallingConv.h"
#include "YSXSubtarget.h"
#include "llvm/Support/ErrorHandling.h"

#include <algorithm>

using namespace llvm;

ArrayRef<MCPhysReg> YSX::getArgGPRs(const YSXABI::ABI ABI) {
  static const MCPhysReg ArgGPRs[] = {YSX::X10, YSX::X11, YSX::X12,
                                      YSX::X13, YSX::X14, YSX::X15,
                                      YSX::X16, YSX::X17};
  return ArrayRef(ArgGPRs);
}

static bool CC_YSX_GPROnly(unsigned ValNo, MVT ValVT, MVT LocVT,
                           CCValAssign::LocInfo LocInfo,
                           ISD::ArgFlagsTy ArgFlags, CCState &State) {
  const auto &MF = State.getMachineFunction();
  const auto &Subtarget = MF.getSubtarget<YSXSubtarget>();
  assert(Subtarget.is64Bit() && "YSX only supports 64-bit code generation");

  if (LocVT.isScalableVector())
    reportFatalUsageError("YSX rv64ima does not support vector arguments");

  if (ArgFlags.isByVal()) {
    Align StackAlign =
        std::max(ArgFlags.getNonZeroByValAlign(), Align(Subtarget.getXLen() / 8));
    unsigned Size = ArgFlags.getByValSize();
    State.addLoc(CCValAssign::getMem(
        ValNo, ValVT, State.AllocateStack(Size, StackAlign), LocVT, LocInfo));
    return false;
  }

  ArrayRef<MCPhysReg> ArgRegs = YSX::getArgGPRs(Subtarget.getTargetABI());
  if (LocVT.getStoreSize() <= Subtarget.getXLen() / 8) {
    if (MCRegister Reg = State.AllocateReg(ArgRegs)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  Align StackAlign(Subtarget.getXLen() / 8);
  unsigned StoreSize = std::max<unsigned>(LocVT.getStoreSize(), StackAlign.value());
  int64_t Offset = State.AllocateStack(StoreSize, StackAlign);
  State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset, LocVT, LocInfo));
  return false;
}

bool llvm::CC_YSX(unsigned ValNo, MVT ValVT, MVT LocVT,
                  CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                  CCState &State, bool IsRet, Type *OrigTy) {
  return CC_YSX_GPROnly(ValNo, ValVT, LocVT, LocInfo, ArgFlags, State);
}

bool llvm::CC_YSX_FastCC(unsigned ValNo, MVT ValVT, MVT LocVT,
                         CCValAssign::LocInfo LocInfo,
                         ISD::ArgFlagsTy ArgFlags, CCState &State, bool IsRet,
                         Type *OrigTy) {
  return CC_YSX_GPROnly(ValNo, ValVT, LocVT, LocInfo, ArgFlags, State);
}

bool llvm::CC_YSX_GHC(unsigned ValNo, MVT ValVT, MVT LocVT,
                      CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                      Type *OrigTy, CCState &State) {
  return CC_YSX_GPROnly(ValNo, ValVT, LocVT, LocInfo, ArgFlags, State);
}
