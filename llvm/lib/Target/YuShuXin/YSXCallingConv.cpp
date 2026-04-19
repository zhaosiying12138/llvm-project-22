//===-- YSXCallingConv.cpp - YuShuXin Custom CC Routines -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "YSXCallingConv.h"
#include "YSXSubtarget.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/ErrorHandling.h"

#include <algorithm>

using namespace llvm;

ArrayRef<MCPhysReg> YSX::getArgGPRs(const YSXABI::ABI ABI) {
  static const MCPhysReg ArgGPRs[] = {YSX::X10, YSX::X11, YSX::X12, YSX::X13,
                                      YSX::X14, YSX::X15, YSX::X16, YSX::X17};
  return ArrayRef(ArgGPRs);
}

// Pass a 2*XLEN argument that has been split into two XLEN values through
// registers or the stack as required by the LP64 integer ABI.
static bool CC_YSXAssign2XLen(unsigned XLen, CCState &State, CCValAssign VA1,
                              ISD::ArgFlagsTy ArgFlags1, unsigned ValNo2,
                              MVT ValVT2, MVT LocVT2) {
  unsigned XLenInBytes = XLen / 8;
  const auto &Subtarget =
      State.getMachineFunction().getSubtarget<YSXSubtarget>();
  ArrayRef<MCPhysReg> ArgRegs = YSX::getArgGPRs(Subtarget.getTargetABI());

  if (MCRegister Reg = State.AllocateReg(ArgRegs)) {
    State.addLoc(CCValAssign::getReg(VA1.getValNo(), VA1.getValVT(), Reg,
                                     VA1.getLocVT(), CCValAssign::Full));
  } else {
    Align StackAlign =
        std::max(Align(XLenInBytes), ArgFlags1.getNonZeroOrigAlign());
    State.addLoc(
        CCValAssign::getMem(VA1.getValNo(), VA1.getValVT(),
                            State.AllocateStack(XLenInBytes, StackAlign),
                            VA1.getLocVT(), CCValAssign::Full));
    State.addLoc(CCValAssign::getMem(
        ValNo2, ValVT2, State.AllocateStack(XLenInBytes, Align(XLenInBytes)),
        LocVT2, CCValAssign::Full));
    return false;
  }

  if (MCRegister Reg = State.AllocateReg(ArgRegs))
    State.addLoc(
        CCValAssign::getReg(ValNo2, ValVT2, Reg, LocVT2, CCValAssign::Full));
  else
    State.addLoc(CCValAssign::getMem(
        ValNo2, ValVT2, State.AllocateStack(XLenInBytes, Align(XLenInBytes)),
        LocVT2, CCValAssign::Full));

  return false;
}

static bool CC_YSX_GPROnly(unsigned ValNo, MVT ValVT, MVT LocVT,
                           CCValAssign::LocInfo LocInfo,
                           ISD::ArgFlagsTy ArgFlags, CCState &State, bool IsRet,
                           Type *OrigTy) {
  const auto &MF = State.getMachineFunction();
  const DataLayout &DL = MF.getDataLayout();
  const auto &Subtarget = MF.getSubtarget<YSXSubtarget>();
  assert(Subtarget.is64Bit() && "YSX only supports 64-bit code generation");

  unsigned XLen = Subtarget.getXLen();
  unsigned XLenInBytes = XLen / 8;
  MVT XLenVT = Subtarget.getXLenVT();

  if (LocVT.isScalableVector())
    reportFatalUsageError("YSX rv64ima does not support vector arguments");

  // Scalar returns split into more than two XLEN values must be sret-demoted.
  if (!LocVT.isVector() && IsRet && ValNo > 1)
    return true;

  if (ArgFlags.isByVal()) {
    Align StackAlign =
        std::max(ArgFlags.getNonZeroByValAlign(), Align(XLenInBytes));
    unsigned Size = ArgFlags.getByValSize();
    State.addLoc(CCValAssign::getMem(
        ValNo, ValVT, State.AllocateStack(Size, StackAlign), LocVT, LocInfo));
    return false;
  }

  ArrayRef<MCPhysReg> ArgRegs = YSX::getArgGPRs(Subtarget.getTargetABI());

  // Varargs with 2*XLEN size and alignment must start in an even argument
  // register. This applies before split pieces are assigned.
  unsigned TwoXLenInBytes = 2 * XLenInBytes;
  if (ArgFlags.isVarArg() && OrigTy &&
      ArgFlags.getNonZeroOrigAlign().value() == TwoXLenInBytes &&
      DL.getTypeAllocSize(OrigTy) == TwoXLenInBytes) {
    unsigned RegIdx = State.getFirstUnallocated(ArgRegs);
    if (RegIdx != ArgRegs.size() && RegIdx % 2 == 1)
      State.AllocateReg(ArgRegs);
  }

  SmallVectorImpl<CCValAssign> &PendingLocs = State.getPendingLocs();
  SmallVectorImpl<ISD::ArgFlagsTy> &PendingArgFlags =
      State.getPendingArgFlags();
  assert(PendingLocs.size() == PendingArgFlags.size() &&
         "PendingLocs and PendingArgFlags out of sync");

  if (ValVT.isScalarInteger() && (ArgFlags.isSplit() || !PendingLocs.empty())) {
    LocVT = XLenVT;
    LocInfo = CCValAssign::Indirect;
    PendingLocs.push_back(
        CCValAssign::getPending(ValNo, ValVT, LocVT, LocInfo));
    PendingArgFlags.push_back(ArgFlags);
    if (!ArgFlags.isSplitEnd())
      return false;
  }

  if (ValVT.isScalarInteger() && ArgFlags.isSplitEnd() &&
      PendingLocs.size() <= 2) {
    assert(PendingLocs.size() == 2 && "Unexpected split scalar size");
    CCValAssign VA = PendingLocs[0];
    ISD::ArgFlagsTy AF = PendingArgFlags[0];
    PendingLocs.clear();
    PendingArgFlags.clear();
    return CC_YSXAssign2XLen(XLen, State, VA, AF, ValNo, ValVT, LocVT);
  }

  if (!PendingLocs.empty()) {
    assert(ArgFlags.isSplitEnd() && "Expected final split scalar part");
    assert(PendingLocs.size() > 2 && "Unexpected indirect split scalar size");
    if (MCRegister Reg = State.AllocateReg(ArgRegs)) {
      for (auto &It : PendingLocs) {
        It.convertToReg(Reg);
        State.addLoc(It);
      }
    } else {
      int64_t Offset = State.AllocateStack(XLenInBytes, Align(XLenInBytes));
      for (auto &It : PendingLocs) {
        It.convertToMem(Offset);
        State.addLoc(It);
      }
    }
    PendingLocs.clear();
    PendingArgFlags.clear();
    return false;
  }

  if (LocVT.getStoreSize() <= XLenInBytes) {
    if (MCRegister Reg = State.AllocateReg(ArgRegs)) {
      State.addLoc(CCValAssign::getReg(ValNo, ValVT, Reg, LocVT, LocInfo));
      return false;
    }
  }

  Align StackAlign(XLenInBytes);
  unsigned StoreSize =
      std::max<unsigned>(LocVT.getStoreSize(), StackAlign.value());
  int64_t Offset = State.AllocateStack(StoreSize, StackAlign);
  State.addLoc(CCValAssign::getMem(ValNo, ValVT, Offset, LocVT, LocInfo));
  return false;
}

bool llvm::CC_YSX(unsigned ValNo, MVT ValVT, MVT LocVT,
                  CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                  CCState &State, bool IsRet, Type *OrigTy) {
  return CC_YSX_GPROnly(ValNo, ValVT, LocVT, LocInfo, ArgFlags, State, IsRet,
                        OrigTy);
}

bool llvm::CC_YSX_FastCC(unsigned ValNo, MVT ValVT, MVT LocVT,
                         CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                         CCState &State, bool IsRet, Type *OrigTy) {
  return CC_YSX_GPROnly(ValNo, ValVT, LocVT, LocInfo, ArgFlags, State, IsRet,
                        OrigTy);
}

bool llvm::CC_YSX_GHC(unsigned ValNo, MVT ValVT, MVT LocVT,
                      CCValAssign::LocInfo LocInfo, ISD::ArgFlagsTy ArgFlags,
                      Type *OrigTy, CCState &State) {
  return CC_YSX_GPROnly(ValNo, ValVT, LocVT, LocInfo, ArgFlags, State,
                        /*IsRet=*/false, OrigTy);
}
