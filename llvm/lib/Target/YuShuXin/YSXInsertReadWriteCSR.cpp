//===-- YSXInsertReadWriteCSR.cpp - YuShuXin CSR insertion pass -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "YSX.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;

#define DEBUG_TYPE "ysx-insert-read-write-csr"
#define YSX_INSERT_READ_WRITE_CSR_NAME "YSX Insert Read/Write CSR Pass"

namespace {

class YSXInsertReadWriteCSR : public MachineFunctionPass {
public:
  static char ID;

  YSXInsertReadWriteCSR() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override { return false; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return YSX_INSERT_READ_WRITE_CSR_NAME;
  }
};

} // end anonymous namespace

char YSXInsertReadWriteCSR::ID = 0;

INITIALIZE_PASS(YSXInsertReadWriteCSR, DEBUG_TYPE,
                YSX_INSERT_READ_WRITE_CSR_NAME, false, false)

FunctionPass *llvm::createYSXInsertReadWriteCSRPass() {
  return new YSXInsertReadWriteCSR();
}
