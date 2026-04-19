//===-- YSXCallingConv.td - Calling Conventions RISC-V ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This describes the calling conventions for the YuShuXin rv64ima target.
//
//===----------------------------------------------------------------------===//

// The RISC-V calling convention is handled with custom code in
// YSXISelLowering.cpp (CC_YSX).

def CSR_ILP32_LP64
    : CalleeSavedRegs<(add X1, X8, X9, (sequence "X%u", 18, 27))>;

// Needed for implementation of YSXRegisterInfo::getNoPreservedMask()
def CSR_NoRegs : CalleeSavedRegs<(add)>;

def CSR_IPRA : CalleeSavedRegs<(add X1)>;

// Interrupt handler needs to save/restore all registers that are used,
// both Caller and Callee saved registers.
def CSR_Interrupt : CalleeSavedRegs<(add X1, (sequence "X%u", 5, 31))>;

def CSR_RT_MostRegs : CalleeSavedRegs<(sub CSR_Interrupt, X6, X7, X28)>;
