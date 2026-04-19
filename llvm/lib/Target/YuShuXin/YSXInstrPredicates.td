//===-- YSXInstrPredicates.td - Instruction Predicates ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the RISC-V instruction predicates.
//
//===----------------------------------------------------------------------===//

// No vector load/store scheduling predicates are active in the rv64ima subset.
def VLDSX0Pred
    : MCSchedPredicate<FalsePred>;

def SingleElementVecFP64SchedPred : MCSchedPredicate<FalsePred>;

// Returns true if this is the sext.w pattern, addiw rd, rs1, 0.
def isSEXT_W
    : TIIPredicate<"isSEXT_W",
                   MCReturnStatement<CheckAll<[
                     CheckOpcode<[ADDIW]>,
                     CheckIsRegOperand<1>,
                     CheckIsImmOperand<2>,
                     CheckImmOperand<2, 0>
                   ]>>>;

// Returns true if this is the zext.w pattern, adduw rd, rs1, x0.
def isZEXT_W
    : TIIPredicate<"isZEXT_W",
                   MCReturnStatement<FalsePred>>;

// Returns true if this is the zext.b pattern, andi rd, rs1, 255.
def isZEXT_B
    : TIIPredicate<"isZEXT_B",
                   MCReturnStatement<CheckAll<[
                     CheckOpcode<[ANDI]>,
                     CheckIsRegOperand<1>,
                     CheckIsImmOperand<2>,
                     CheckImmOperand<2, 255>
                   ]>>>;

def isSelectPseudo
    : TIIPredicate<"isSelectPseudo",
                   MCReturnStatement<
                     CheckOpcode<[
                       Select_GPR_Using_CC_GPR
                     ]>>>;

def isNonZeroLoadImmediate
    : TIIPredicate<"isNonZeroLoadImmediate",
                   MCReturnStatement<CheckAll<[
                     CheckOpcode<[ADDI]>,
                     CheckIsRegOperand<1>,
                     CheckRegOperand<1, X0>,
                     CheckIsImmOperand<2>,
                     CheckNot<CheckImmOperand<2, 0>>
                   ]>>>;

def isLPAD
    : TIIPredicate<"isLPAD",
                   MCReturnStatement<CheckAll<[
                     CheckOpcode<[AUIPC]>,
                     CheckIsRegOperand<0>,
                     CheckRegOperand<0, X0>,
                   ]>>>;
