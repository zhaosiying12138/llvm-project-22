//===------------------- YSXCustomBehaviour.cpp ---------------*-C++ -* -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "YSXCustomBehaviour.h"
#include "TargetInfo/YSXTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"

using namespace llvm;
using namespace mca;

static InstrumentManager *
createYSXInstrumentManager(const MCSubtargetInfo &STI,
                           const MCInstrInfo &MCII) {
  return new InstrumentManager(STI, MCII, /*EnableInstruments=*/false);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeYSXTargetMCA() {
  TargetRegistry::RegisterInstrumentManager(getTheYSX64Target(),
                                            createYSXInstrumentManager);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYuShuXinTargetMCA() {
  LLVMInitializeYSXTargetMCA();
}
