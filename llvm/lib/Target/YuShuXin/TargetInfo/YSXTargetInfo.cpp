//===-- YSXTargetInfo.cpp - RISC-V Target Implementation ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "TargetInfo/YSXTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
using namespace llvm;

Target &llvm::getTheYSX64Target() {
  static Target TheYSX64Target;
  return TheYSX64Target;
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYSXTargetInfo() {
  RegisterTarget<Triple::ysx64, /*HasJIT=*/true> Y(
      getTheYSX64Target(), "ysx64", "64-bit YuShuXin", "YSX");
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYuShuXinTargetInfo() {
  LLVMInitializeYSXTargetInfo();
}
