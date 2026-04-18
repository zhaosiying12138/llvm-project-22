//===-- YSXMCObjectFileInfo.cpp - RISC-V object file properties ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the declarations of the YSXMCObjectFileInfo properties.
//
//===----------------------------------------------------------------------===//

#include "YSXMCObjectFileInfo.h"
#include "YSXMCTargetDesc.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

unsigned
YSXMCObjectFileInfo::getTextSectionAlignment(const MCSubtargetInfo &STI) {
  return STI.hasFeature(YSX::FeatureStdExtZca) ? 2 : 4;
}

unsigned YSXMCObjectFileInfo::getTextSectionAlignment() const {
  return getTextSectionAlignment(*getContext().getSubtargetInfo());
}
