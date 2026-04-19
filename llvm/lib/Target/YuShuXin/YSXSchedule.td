//===-- YSXSchedule.td - RISC-V Scheduling Definitions ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Define TII for use in SchedPredicates that target MachineInstr.
def : PredicateProlog<[{
  [[maybe_unused]] const YSXInstrInfo *TII =
    static_cast<const YSXInstrInfo*>(SchedModel->getInstrInfo());
}]>;

/// Define scheduler resources associated with def operands.
def WriteIALU       : SchedWrite;    // 32 or 64-bit integer ALU operations
def WriteIALU32     : SchedWrite;    // 32-bit integer ALU operations on RV64I
def WriteShiftImm   : SchedWrite;    // 32 or 64-bit shift by immediate operations
def WriteShiftImm32 : SchedWrite;    // 32-bit shift by immediate operations on RV64Ix
def WriteShiftReg   : SchedWrite;    // 32 or 64-bit shift by immediate operations
def WriteShiftReg32 : SchedWrite;    // 32-bit shift by immediate operations on RV64Ix
def WriteIDiv       : SchedWrite;    // 32-bit or 64-bit divide
def WriteIDiv32     : SchedWrite;    // 32-bit divide on RV64I
def WriteIRem       : SchedWrite;    // 32-bit or 64-bit remainder
def WriteIRem32     : SchedWrite;    // 32-bit remainder on RV64I
def WriteIMul       : SchedWrite;    // 32-bit or 64-bit multiply
def WriteIMul32     : SchedWrite;    // 32-bit multiply on RV64I
def WriteJmp        : SchedWrite;    // Jump
def WriteJal        : SchedWrite;    // Jump and link
def WriteJalr       : SchedWrite;    // Jump and link register
def WriteNop        : SchedWrite;
def WriteLDB        : SchedWrite;    // Load byte
def WriteLDH        : SchedWrite;    // Load half-word
def WriteLDW        : SchedWrite;    // Load word
def WriteLDD        : SchedWrite;    // Load double-word
def WriteSTB        : SchedWrite;    // Store byte
def WriteSTH        : SchedWrite;    // Store half-word
def WriteSTW        : SchedWrite;    // Store word
def WriteSTD        : SchedWrite;    // Store double-word
def WriteAtomicB    : SchedWrite;    //Atomic memory operation byte size
def WriteAtomicH    : SchedWrite;    //Atomic memory operation halfword size
def WriteAtomicW    : SchedWrite;    //Atomic memory operation word size
def WriteAtomicD    : SchedWrite;    //Atomic memory operation double word size
def WriteAtomicLDW  : SchedWrite;    // Atomic load word
def WriteAtomicLDD  : SchedWrite;    // Atomic load double word
def WriteAtomicSTW  : SchedWrite;    // Atomic store word
def WriteAtomicSTD  : SchedWrite;    // Atomic store double word

/// Define scheduler resources associated with use operands.
def ReadJmp         : SchedRead;
def ReadJalr        : SchedRead;
def ReadMemBase     : SchedRead;
def ReadStoreData   : SchedRead;
def ReadIALU        : SchedRead;
def ReadIALU32      : SchedRead;    // 32-bit integer ALU operations on RV64I
def ReadShiftImm    : SchedRead;
def ReadShiftImm32  : SchedRead;    // 32-bit shift by immediate operations on RV64Ix
def ReadShiftReg    : SchedRead;
def ReadShiftReg32  : SchedRead;    // 32-bit shift by register operations on RV64Ix
def ReadIDiv        : SchedRead;
def ReadIDiv32      : SchedRead;
def ReadIRem        : SchedRead;
def ReadIRem32      : SchedRead;
def ReadIMul        : SchedRead;
def ReadIMul32      : SchedRead;
def ReadAtomicBA    : SchedRead;
def ReadAtomicBD    : SchedRead;
def ReadAtomicHA    : SchedRead;
def ReadAtomicHD    : SchedRead;
def ReadAtomicWA    : SchedRead;
def ReadAtomicWD    : SchedRead;
def ReadAtomicDA    : SchedRead;
def ReadAtomicDD    : SchedRead;
def ReadAtomicLDW   : SchedRead;    // Atomic load word
def ReadAtomicLDD   : SchedRead;    // Atomic load double word
def ReadAtomicSTW   : SchedRead;    // Atomic store word
def ReadAtomicSTD   : SchedRead;    // Atomic store double word

multiclass UnsupportedSchedA {
let Unsupported = true in {
def : WriteRes<WriteAtomicW, []>;
def : WriteRes<WriteAtomicD, []>;
def : WriteRes<WriteAtomicLDW, []>;
def : WriteRes<WriteAtomicLDD, []>;
def : WriteRes<WriteAtomicSTW, []>;
def : WriteRes<WriteAtomicSTD, []>;

def : ReadAdvance<ReadAtomicWA, 0>;
def : ReadAdvance<ReadAtomicWD, 0>;
def : ReadAdvance<ReadAtomicDA, 0>;
def : ReadAdvance<ReadAtomicDD, 0>;
def : ReadAdvance<ReadAtomicLDW, 0>;
def : ReadAdvance<ReadAtomicLDD, 0>;
def : ReadAdvance<ReadAtomicSTW, 0>;
def : ReadAdvance<ReadAtomicSTD, 0>;
} // Unsupported = true
}
