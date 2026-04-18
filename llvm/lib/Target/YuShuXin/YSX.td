//===-- YSX.td - Describe the RISC-V Target Machine ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// RISC-V subtarget features and instruction predicates.
//===----------------------------------------------------------------------===//

include "YSXFeatures.td"

//===----------------------------------------------------------------------===//
// Named operands for CSR instructions.
//===----------------------------------------------------------------------===//

include "YSXSystemOperands.td"

//===----------------------------------------------------------------------===//
// Registers, calling conventions, instruction descriptions.
//===----------------------------------------------------------------------===//

include "YSXRegisterInfo.td"
include "YSXSchedule.td"
include "YSXCallingConv.td"
include "YSXInstrInfo.td"

//===----------------------------------------------------------------------===//
// Instruction predicates
//===----------------------------------------------------------------------===//

include "YSXInstrPredicates.td"

//===----------------------------------------------------------------------===//
// RISC-V macro fusions.
//===----------------------------------------------------------------------===//

include "YSXMacroFusion.td"

//===----------------------------------------------------------------------===//
// RISC-V Scheduling Models
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// RISC-V processors supported.
//===----------------------------------------------------------------------===//

include "YSXProcessors.td"

//===----------------------------------------------------------------------===//
// Define the RISC-V target.
//===----------------------------------------------------------------------===//

def YSXInstrInfo : InstrInfo {
  let guessInstructionProperties = 0;
}

def YSXAsmParser : AsmParser {
  let ShouldEmitMatchRegisterAltName = 1;
  let AllowDuplicateRegisterNames = 1;
}

def YSXAsmWriter : AsmWriter {
  int PassSubtarget = 1;
}

defm : RemapAllTargetPseudoPointerOperands<GPR>;

def YSX : Target {
  let InstructionSet = YSXInstrInfo;
  let AssemblyParsers = [YSXAsmParser];
  let AssemblyWriters = [YSXAsmWriter];
  let AllowRegisterRenaming = 1;
}
