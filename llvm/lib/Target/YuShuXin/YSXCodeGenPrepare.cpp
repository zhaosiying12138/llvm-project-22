//===----- YSXCodeGenPrepare.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is a YuShuXin-specific version of CodeGenPrepare.
//
//===----------------------------------------------------------------------===//

#include "YSX.h"
#include "YSXTargetMachine.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

using namespace llvm;

#define DEBUG_TYPE "ysx-codegenprepare"
#define PASS_NAME "YuShuXin CodeGenPrepare"

namespace {
class YSXCodeGenPrepare : public InstVisitor<YSXCodeGenPrepare, bool> {
  Function &F;
  const YSXSubtarget *ST;
  bool EnableTransforms;

public:
  YSXCodeGenPrepare(Function &F, const YSXSubtarget *ST,
                    bool EnableTransforms)
      : F(F), ST(ST), EnableTransforms(EnableTransforms) {}
  bool run();
  bool visitInstruction(Instruction &I) { return false; }
  bool visitAnd(BinaryOperator &BO);
};
} // namespace

namespace {
class YSXUnsupportedIRGuardLegacyPass : public ModulePass {
public:
  static char ID;

  YSXUnsupportedIRGuardLegacyPass() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;
  StringRef getPassName() const override {
    return "YuShuXin Unsupported IR Guard";
  }
};

class YSXCodeGenPrepareLegacyPass : public FunctionPass {
public:
  static char ID;

  YSXCodeGenPrepareLegacyPass() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;
  StringRef getPassName() const override { return PASS_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<TargetPassConfig>();
  }
};
} // namespace

static bool isUnsupportedRISCVIntrinsic(const Instruction &I) {
  const auto *II = dyn_cast<IntrinsicInst>(&I);
  if (!II)
    return false;

  const Function *Callee = II->getCalledFunction();
  if (!Callee || !Callee->getName().starts_with("llvm.riscv."))
    return false;

  StringRef Name = Callee->getName();
  return !Name.starts_with("llvm.riscv.masked.atomicrmw") &&
         !Name.starts_with("llvm.riscv.masked.cmpxchg");
}

static bool usesScalableType(const Instruction &I) {
  if (I.getType()->isScalableTy())
    return true;

  for (const Use &U : I.operands())
    if (U->getType()->isScalableTy())
      return true;

  if (const auto *AI = dyn_cast<AllocaInst>(&I))
    return AI->getAllocatedType()->isScalableTy();

  if (const auto *GEP = dyn_cast<GetElementPtrInst>(&I))
    return GEP->getSourceElementType()->isScalableTy();

  return false;
}

static bool diagnoseUnsupportedVectorIR(Function &F) {
  auto Diagnose = [&](const Twine &Message, const Instruction *I = nullptr) {
    F.getContext().diagnose(DiagnosticInfoUnsupported(
        F, Message, I ? I->getDebugLoc() : DiagnosticLocation()));
  };

  if (F.getReturnType()->isScalableTy()) {
    Diagnose("YuShuXin only supports rv64ima and does not support scalable "
             "vector IR");
    return true;
  }

  for (Argument &Arg : F.args()) {
    if (!Arg.getType()->isScalableTy())
      continue;
    Diagnose("YuShuXin only supports rv64ima and does not support scalable "
             "vector IR");
    return true;
  }

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (isUnsupportedRISCVIntrinsic(I)) {
        Diagnose("YuShuXin only supports rv64ima and does not support RISC-V "
                 "target intrinsics other than masked atomics",
                 &I);
        return true;
      }

      if (usesScalableType(I)) {
        Diagnose("YuShuXin only supports rv64ima and does not support scalable "
                 "vector IR",
                 &I);
        return true;
      }
    }
  }

  return false;
}

// Try to optimize (i64 (and (zext/sext (i32 X), C1))) if C1 has bit 31 set,
// but bits 63:32 are zero. If we know that bit 31 of X is 0, we can fill
// the upper 32 bits with ones.
bool YSXCodeGenPrepare::visitAnd(BinaryOperator &BO) {
  if (!ST->is64Bit())
    return false;

  if (!BO.getType()->isIntegerTy(64))
    return false;

  using namespace PatternMatch;

  // Left hand side should be a zext nneg.
  Value *LHSSrc;
  if (!match(BO.getOperand(0), m_NNegZExt(m_Value(LHSSrc))))
    return false;

  if (!LHSSrc->getType()->isIntegerTy(32))
    return false;

  // Right hand side should be a constant.
  Value *RHS = BO.getOperand(1);

  auto *CI = dyn_cast<ConstantInt>(RHS);
  if (!CI)
    return false;
  uint64_t C = CI->getZExtValue();

  // Look for constants that fit in 32 bits but not simm12, and can be made
  // into simm12 by sign extending bit 31. This will allow use of ANDI.
  // TODO: Is worth making simm32?
  if (!isUInt<32>(C) || isInt<12>(C) || !isInt<12>(SignExtend64<32>(C)))
    return false;

  // Sign extend the constant and replace the And operand.
  C = SignExtend64<32>(C);
  BO.setOperand(1, ConstantInt::get(RHS->getType(), C));

  return true;
}

bool YSXCodeGenPrepare::run() {
  if (!EnableTransforms)
    return false;

  bool MadeChange = false;
  for (auto &BB : F)
    for (Instruction &I : llvm::make_early_inc_range(BB))
      MadeChange |= visit(I);

  return MadeChange;
}

bool YSXUnsupportedIRGuardLegacyPass::runOnModule(Module &M) {
  bool MadeChange = false;

  for (Function &F : M) {
    if (F.isDeclaration() || !diagnoseUnsupportedVectorIR(F))
      continue;

    F.deleteBody();
    MadeChange = true;
  }

  return MadeChange;
}

bool YSXCodeGenPrepareLegacyPass::runOnFunction(Function &F) {
  if (skipFunction(F))
    return false;

  auto &TPC = getAnalysis<TargetPassConfig>();
  auto &TM = TPC.getTM<YSXTargetMachine>();
  auto ST = &TM.getSubtarget<YSXSubtarget>(F);
  bool EnableTransforms = TPC.getOptLevel() != CodeGenOptLevel::None;

  YSXCodeGenPrepare YSXCGP(F, ST, EnableTransforms);
  return YSXCGP.run();
}

INITIALIZE_PASS(YSXUnsupportedIRGuardLegacyPass, "ysx-unsupported-ir-guard",
                "YuShuXin Unsupported IR Guard", false, false)

INITIALIZE_PASS_BEGIN(YSXCodeGenPrepareLegacyPass, DEBUG_TYPE, PASS_NAME,
                      false, false)
INITIALIZE_PASS_DEPENDENCY(TargetPassConfig)
INITIALIZE_PASS_END(YSXCodeGenPrepareLegacyPass, DEBUG_TYPE, PASS_NAME, false,
                    false)

char YSXUnsupportedIRGuardLegacyPass::ID = 0;
char YSXCodeGenPrepareLegacyPass::ID = 0;

ModulePass *llvm::createYSXUnsupportedIRGuardPass() {
  return new YSXUnsupportedIRGuardLegacyPass();
}

FunctionPass *llvm::createYSXCodeGenPrepareLegacyPass() {
  return new YSXCodeGenPrepareLegacyPass();
}

PreservedAnalyses YSXCodeGenPreparePass::run(Function &F,
                                               FunctionAnalysisManager &FAM) {
  auto ST = &TM->getSubtarget<YSXSubtarget>(F);
  bool Changed =
      YSXCodeGenPrepare(F, ST, TM->getOptLevel() != CodeGenOptLevel::None)
          .run();
  if (!Changed)
    return PreservedAnalyses::all();

  PreservedAnalyses PA = PreservedAnalyses::none();
  PA.preserveSet<CFGAnalyses>();
  return PA;
}
