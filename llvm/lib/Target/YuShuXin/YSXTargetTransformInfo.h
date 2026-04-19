//===- YSXTargetTransformInfo.h - YSX target TTI ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_YSX_YSXTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_YSX_YSXTARGETTRANSFORMINFO_H

#include "YSXSubtarget.h"
#include "YSXTargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

namespace llvm {

class YSXTTIImpl final : public BasicTTIImplBase<YSXTTIImpl> {
  using BaseT = BasicTTIImplBase<YSXTTIImpl>;

  const YSXSubtarget *ST;
  const YSXTargetLowering *TLI;

public:
  explicit YSXTTIImpl(const YSXTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  const YSXSubtarget *getST() const { return ST; }
  const YSXTargetLowering *getTLI() const { return TLI; }

  bool supportsScalableVectors() const override { return false; }
  bool enableScalableVectorization() const override { return false; }
  bool preferPredicateOverEpilogue(TailFoldingInfo *) const override {
    return false;
  }
  TailFoldingStyle
  getPreferredTailFoldingStyle(bool /*IVUpdateMayOverflow*/) const override {
    return TailFoldingStyle::None;
  }

  TypeSize
  getRegisterBitWidth(TargetTransformInfo::RegisterKind K) const override {
    if (K == TargetTransformInfo::RGK_Scalar)
      return TypeSize::getFixed(ST->getXLen());
    return TypeSize::getFixed(0);
  }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_YSX_YSXTARGETTRANSFORMINFO_H
