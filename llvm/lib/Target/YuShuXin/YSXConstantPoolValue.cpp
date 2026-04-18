//===------- YSXConstantPoolValue.cpp - RISC-V constantpool value -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the RISC-V specific constantpool value class.
//
//===----------------------------------------------------------------------===//

#include "YSXConstantPoolValue.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

YSXConstantPoolValue::YSXConstantPoolValue(Type *Ty, const GlobalValue *GV)
    : MachineConstantPoolValue(Ty), GV(GV), Kind(YSXCPKind::GlobalValue) {}

YSXConstantPoolValue::YSXConstantPoolValue(LLVMContext &C, StringRef S)
    : MachineConstantPoolValue(Type::getInt64Ty(C)), S(S),
      Kind(YSXCPKind::ExtSymbol) {}

YSXConstantPoolValue *YSXConstantPoolValue::Create(const GlobalValue *GV) {
  return new YSXConstantPoolValue(GV->getType(), GV);
}

YSXConstantPoolValue *YSXConstantPoolValue::Create(LLVMContext &C,
                                                       StringRef S) {
  return new YSXConstantPoolValue(C, S);
}

int YSXConstantPoolValue::getExistingMachineCPValue(MachineConstantPool *CP,
                                                      Align Alignment) {
  const std::vector<MachineConstantPoolEntry> &Constants = CP->getConstants();
  for (unsigned i = 0, e = Constants.size(); i != e; ++i) {
    if (Constants[i].isMachineConstantPoolEntry() &&
        Constants[i].getAlign() >= Alignment) {
      auto *CPV =
          static_cast<YSXConstantPoolValue *>(Constants[i].Val.MachineCPVal);
      if (equals(CPV))
        return i;
    }
  }

  return -1;
}

void YSXConstantPoolValue::addSelectionDAGCSEId(FoldingSetNodeID &ID) {
  if (isGlobalValue())
    ID.AddPointer(GV);
  else {
    assert(isExtSymbol() && "unrecognized constant pool type");
    ID.AddString(S);
  }
}

void YSXConstantPoolValue::print(raw_ostream &O) const {
  if (isGlobalValue())
    O << GV->getName();
  else {
    assert(isExtSymbol() && "unrecognized constant pool type");
    O << S;
  }
}

bool YSXConstantPoolValue::equals(const YSXConstantPoolValue *A) const {
  if (isGlobalValue() && A->isGlobalValue())
    return GV == A->GV;
  if (isExtSymbol() && A->isExtSymbol())
    return S == A->S;

  return false;
}
