//===-- YSX.h - Top-level interface for RISC-V ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// RISC-V back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_YSX_YSX_H
#define LLVM_LIB_TARGET_YSX_YSX_H

#include "MCTargetDesc/YSXBaseInfo.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {
class FunctionPass;
class ModulePass;
class PassRegistry;
class YSXSubtarget;
class YSXTargetMachine;

class YSXCodeGenPreparePass : public PassInfoMixin<YSXCodeGenPreparePass> {
private:
  const YSXTargetMachine *TM;

public:
  YSXCodeGenPreparePass(const YSXTargetMachine *TM) : TM(TM) {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM);
};
FunctionPass *createYSXCodeGenPrepareLegacyPass();
void initializeYSXCodeGenPrepareLegacyPassPass(PassRegistry &);

FunctionPass *createYSXDeadRegisterDefinitionsPass();
void initializeYSXDeadRegisterDefinitionsPass(PassRegistry &);

FunctionPass *createYSXIndirectBranchTrackingPass();
void initializeYSXIndirectBranchTrackingPass(PassRegistry &);

FunctionPass *createYSXLandingPadSetupPass();
void initializeYSXLandingPadSetupPass(PassRegistry &);

FunctionPass *createYSXISelDag(YSXTargetMachine &TM,
                                 CodeGenOptLevel OptLevel);

FunctionPass *createYSXLateBranchOptPass();
void initializeYSXLateBranchOptPass(PassRegistry &);

FunctionPass *createYSXMakeCompressibleOptPass();
void initializeYSXMakeCompressibleOptPass(PassRegistry &);

FunctionPass *createYSXGatherScatterLoweringPass();
void initializeYSXGatherScatterLoweringPass(PassRegistry &);

FunctionPass *createYSXVectorPeepholePass();
void initializeYSXVectorPeepholePass(PassRegistry &);

FunctionPass *createYSXOptWInstrsPass();
void initializeYSXOptWInstrsPass(PassRegistry &);

FunctionPass *createYSXFoldMemOffsetPass();
void initializeYSXFoldMemOffsetPass(PassRegistry &);

FunctionPass *createYSXMergeBaseOffsetOptPass();
void initializeYSXMergeBaseOffsetOptPass(PassRegistry &);

FunctionPass *createYSXExpandPseudoPass();
void initializeYSXExpandPseudoPass(PassRegistry &);

FunctionPass *createYSXPreRAExpandPseudoPass();
void initializeYSXPreRAExpandPseudoPass(PassRegistry &);

FunctionPass *createYSXExpandAtomicPseudoPass();
void initializeYSXExpandAtomicPseudoPass(PassRegistry &);

FunctionPass *createYSXInsertVSETVLIPass();
void initializeYSXInsertVSETVLIPass(PassRegistry &);
extern char &YSXInsertVSETVLIID;

FunctionPass *createYSXPostRAExpandPseudoPass();
void initializeYSXPostRAExpandPseudoPass(PassRegistry &);
FunctionPass *createYSXInsertReadWriteCSRPass();
void initializeYSXInsertReadWriteCSRPass(PassRegistry &);

FunctionPass *createYSXInsertWriteVXRMPass();
void initializeYSXInsertWriteVXRMPass(PassRegistry &);

FunctionPass *createYSXRedundantCopyEliminationPass();
void initializeYSXRedundantCopyEliminationPass(PassRegistry &);

FunctionPass *createYSXMoveMergePass();
void initializeYSXMoveMergePass(PassRegistry &);

FunctionPass *createYSXPushPopOptimizationPass();
void initializeYSXPushPopOptPass(PassRegistry &);
FunctionPass *createYSXLoadStoreOptPass();
void initializeYSXLoadStoreOptPass(PassRegistry &);

FunctionPass *createYSXPreAllocZilsdOptPass();
void initializeYSXPreAllocZilsdOptPass(PassRegistry &);

FunctionPass *createYSXZacasABIFixPass();
void initializeYSXZacasABIFixPass(PassRegistry &);

void initializeYSXDAGToDAGISelLegacyPass(PassRegistry &);

ModulePass *createYSXPromoteConstantPass();
void initializeYSXPromoteConstantPass(PassRegistry &);

FunctionPass *createYSXVLOptimizerPass();
void initializeYSXVLOptimizerPass(PassRegistry &);

FunctionPass *createYSXVMV0EliminationPass();
void initializeYSXVMV0EliminationPass(PassRegistry &);

void initializeYSXAsmPrinterPass(PassRegistry &);
} // namespace llvm

#endif
