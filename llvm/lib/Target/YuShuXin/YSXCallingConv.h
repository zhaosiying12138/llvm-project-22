//===-- YSXCallingConv.h - RISC-V Custom CC Routines ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the custom routines for the RISC-V Calling Convention.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/YSXBaseInfo.h"
#include "llvm/CodeGen/CallingConvLower.h"

namespace llvm {

/// YSXCCAssignFn - This target-specific function extends the default
/// CCValAssign with additional information used to lower RISC-V calling
/// conventions.
typedef bool YSXCCAssignFn(unsigned ValNo, MVT ValVT, MVT LocVT,
                             CCValAssign::LocInfo LocInfo,
                             ISD::ArgFlagsTy ArgFlags, CCState &State,
                             bool IsRet, Type *OrigTy);

bool CC_YSX(unsigned ValNo, MVT ValVT, MVT LocVT,
              CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
              CCState &State, bool IsRet, Type *OrigTy);

bool CC_YSX_FastCC(unsigned ValNo, MVT ValVT, MVT LocVT,
                     CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                     CCState &State, bool IsRet, Type *OrigTy);

bool CC_YSX_GHC(unsigned ValNo, MVT ValVT, MVT LocVT,
                  CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                  Type *OrigTy, CCState &State);

namespace YSX {

ArrayRef<MCPhysReg> getArgGPRs(const YSXABI::ABI ABI);

} // end namespace YSX

} // end namespace llvm
