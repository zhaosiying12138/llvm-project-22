//===-- YSXFixupKinds.h - RISC-V Specific Fixup Entries -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_YSX_MCTARGETDESC_YSXFIXUPKINDS_H
#define LLVM_LIB_TARGET_YSX_MCTARGETDESC_YSXFIXUPKINDS_H

#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCFixup.h"

#undef YSX

namespace llvm::YSX {
enum Fixups {
  // 20-bit fixup corresponding to %hi(foo) for instructions like lui
  fixup_ysx_hi20 = FirstTargetFixupKind,
  // 12-bit fixup corresponding to %lo(foo) for instructions like addi
  fixup_ysx_lo12_i,
  // 12-bit fixup corresponding to foo-bar for instructions like addi
  fixup_ysx_12_i,
  // 12-bit fixup corresponding to %lo(foo) for the S-type store instructions
  fixup_ysx_lo12_s,
  // 20-bit fixup corresponding to %pcrel_hi(foo) for instructions like auipc
  fixup_ysx_pcrel_hi20,
  // 12-bit fixup corresponding to %pcrel_lo(foo) for instructions like addi
  fixup_ysx_pcrel_lo12_i,
  // 12-bit fixup corresponding to %pcrel_lo(foo) for the S-type store
  // instructions
  fixup_ysx_pcrel_lo12_s,
  // 20-bit fixup for symbol references in the jal instruction
  fixup_ysx_jal,
  // 12-bit fixup for symbol references in the branch instructions
  fixup_ysx_branch,
  // Fixup representing a legacy no-pic function call attached to the auipc
  // instruction in a pair composed of adjacent auipc+jalr instructions.
  fixup_ysx_call,
  // Fixup representing a function call attached to the auipc instruction in a
  // pair composed of adjacent auipc+jalr instructions.
  fixup_ysx_call_plt,

  // Used as a sentinel, must be the last
  fixup_ysx_invalid,
  NumTargetFixupKinds = fixup_ysx_invalid - FirstTargetFixupKind
};
} // end namespace llvm::YSX

#endif
