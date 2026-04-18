//===-- YSXMCExpr.cpp - RISC-V specific MC expression classes -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of the assembly expression modifiers
// accepted by the RISC-V architecture (e.g. ":lo12:", ":gottprel_g1:", ...).
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/YSXAsmBackend.h"
#include "MCTargetDesc/YSXMCAsmInfo.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "ysxmcexpr"

YSX::Specifier YSX::parseSpecifierName(StringRef name) {
  return StringSwitch<YSX::Specifier>(name)
      .Case("lo", YSX::S_LO)
      .Case("hi", ELF::R_RISCV_HI20)
      .Case("pcrel_lo", YSX::S_PCREL_LO)
      .Case("pcrel_hi", ELF::R_RISCV_PCREL_HI20)
      .Case("got_pcrel_hi", ELF::R_RISCV_GOT_HI20)
      .Case("tprel_lo", YSX::S_TPREL_LO)
      .Case("tprel_hi", ELF::R_RISCV_TPREL_HI20)
      .Case("tprel_add", ELF::R_RISCV_TPREL_ADD)
      .Case("tls_ie_pcrel_hi", ELF::R_RISCV_TLS_GOT_HI20)
      .Case("tls_gd_pcrel_hi", ELF::R_RISCV_TLS_GD_HI20)
      .Case("tlsdesc_hi", ELF::R_RISCV_TLSDESC_HI20)
      .Case("tlsdesc_load_lo", ELF::R_RISCV_TLSDESC_LOAD_LO12)
      .Case("tlsdesc_add_lo", ELF::R_RISCV_TLSDESC_ADD_LO12)
      .Case("tlsdesc_call", ELF::R_RISCV_TLSDESC_CALL)
      .Case("qc.abs20", YSX::S_QC_ABS20)
      // Used in data directives
      .Case("pltpcrel", ELF::R_RISCV_PLT32)
      .Case("gotpcrel", ELF::R_RISCV_GOT32_PCREL)
      .Default(0);
}

StringRef YSX::getSpecifierName(Specifier S) {
  switch (S) {
  case YSX::S_None:
    llvm_unreachable("not used as %specifier()");
  case YSX::S_LO:
    return "lo";
  case ELF::R_RISCV_HI20:
    return "hi";
  case YSX::S_PCREL_LO:
    return "pcrel_lo";
  case ELF::R_RISCV_PCREL_HI20:
    return "pcrel_hi";
  case ELF::R_RISCV_GOT_HI20:
    return "got_pcrel_hi";
  case YSX::S_TPREL_LO:
    return "tprel_lo";
  case ELF::R_RISCV_TPREL_HI20:
    return "tprel_hi";
  case ELF::R_RISCV_TPREL_ADD:
    return "tprel_add";
  case ELF::R_RISCV_TLS_GOT_HI20:
    return "tls_ie_pcrel_hi";
  case ELF::R_RISCV_TLSDESC_HI20:
    return "tlsdesc_hi";
  case ELF::R_RISCV_TLSDESC_LOAD_LO12:
    return "tlsdesc_load_lo";
  case ELF::R_RISCV_TLSDESC_ADD_LO12:
    return "tlsdesc_add_lo";
  case ELF::R_RISCV_TLSDESC_CALL:
    return "tlsdesc_call";
  case ELF::R_RISCV_TLS_GD_HI20:
    return "tls_gd_pcrel_hi";
  case ELF::R_RISCV_CALL_PLT:
    return "call_plt";
  case ELF::R_RISCV_32_PCREL:
    return "32_pcrel";
  case ELF::R_RISCV_GOT32_PCREL:
    return "gotpcrel";
  case ELF::R_RISCV_PLT32:
    return "pltpcrel";
  case YSX::S_QC_ABS20:
    return "qc.abs20";
  }
  llvm_unreachable("Invalid ELF symbol kind");
}
