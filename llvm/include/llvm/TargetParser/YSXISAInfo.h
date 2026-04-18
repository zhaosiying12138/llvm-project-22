//===-- YSXISAInfo.h - YSX ISA Information ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGETPARSER_YSXISAINFO_H
#define LLVM_TARGETPARSER_YSXISAINFO_H

#include "llvm/TargetParser/RISCVISAInfo.h"
#include "llvm/TargetParser/RISCVTargetParser.h"

namespace llvm {

using YSXISAInfo = RISCVISAInfo;
namespace YSXVType = RISCVVType;

} // namespace llvm

#endif // LLVM_TARGETPARSER_YSXISAINFO_H
