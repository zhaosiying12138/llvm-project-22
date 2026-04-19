//===-- YSXBaseInfo.h - Top level definitions for RISC-V MC ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains small standalone enum definitions for the RISC-V target
// useful for the compiler back-end and the MC libraries.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_YSX_MCTARGETDESC_YSXBASEINFO_H
#define LLVM_LIB_TARGET_YSX_MCTARGETDESC_YSXBASEINFO_H

#include "MCTargetDesc/YSXMCTargetDesc.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/TargetParser/YSXISAInfo.h"
#include "llvm/TargetParser/YSXTargetParser.h"
#include "llvm/TargetParser/SubtargetFeature.h"

namespace llvm {

// YSXII - This namespace holds all of the target specific flags that
// instruction info tracks. All definitions must match YSXInstrFormats.td.
namespace YSXII {
enum {
  InstFormatPseudo = 0,
  InstFormatR = 1,
  InstFormatR4 = 2,
  InstFormatI = 3,
  InstFormatS = 4,
  InstFormatB = 5,
  InstFormatU = 6,
  InstFormatJ = 7,
  InstFormatOther = 31,

  InstFormatMask = 31,
  InstFormatShift = 0,

  // Indicates that the result can be considered sign extended from bit 31. Some
  // instructions with this flag aren't W instructions, but are either sign
  // extended from a smaller size, always outputs a small integer, or put zeros
  // in bits 63:31. Used by the SExtWRemoval pass.
  IsSignExtendingOpWShift = InstFormatShift + 5,
  IsSignExtendingOpWMask = 1ULL << IsSignExtendingOpWShift,
};

// Helper functions to read TSFlags.
/// \returns the format of the instruction.
static inline unsigned getFormat(uint64_t TSFlags) {
  return (TSFlags & InstFormatMask) >> InstFormatShift;
}
static inline MCRegister
getTailExpandUseRegNo(const FeatureBitset &FeatureBits) {
  return YSX::X6;
}

// RISC-V Specific Machine Operand Flags
enum {
  MO_None = 0,
  MO_CALL = 1,
  MO_LO = 3,
  MO_HI = 4,
  MO_PCREL_LO = 5,
  MO_PCREL_HI = 6,
  MO_GOT_HI = 7,
  MO_TPREL_LO = 8,
  MO_TPREL_HI = 9,
  MO_TPREL_ADD = 10,
  MO_TLS_GOT_HI = 11,
  MO_TLS_GD_HI = 12,
  MO_TLSDESC_HI = 13,
  MO_TLSDESC_LOAD_LO = 14,
  MO_TLSDESC_ADD_LO = 15,
  MO_TLSDESC_CALL = 16,

  // Used to differentiate between target-specific "direct" flags and "bitmask"
  // flags. A machine operand can only have one "direct" flag, but can have
  // multiple "bitmask" flags.
  MO_DIRECT_FLAG_MASK = 31
};
} // namespace YSXII

namespace YSXOp {
enum OperandType : unsigned {
  OPERAND_FIRST_YSX_IMM = MCOI::OPERAND_FIRST_TARGET,
  OPERAND_UIMM1 = OPERAND_FIRST_YSX_IMM,
  OPERAND_UIMM2,
  OPERAND_UIMM2_LSB0,
  OPERAND_UIMM3,
  OPERAND_UIMM4,
  OPERAND_UIMM5,
  OPERAND_UIMM5_NONZERO,
  OPERAND_UIMM5_GT3,
  OPERAND_UIMM5_PLUS1,
  OPERAND_UIMM5_GE6_PLUS1,
  OPERAND_UIMM5_LSB0,
  OPERAND_UIMM5_SLIST,
  OPERAND_UIMM6,
  OPERAND_UIMM6_LSB0,
  OPERAND_UIMM7,
  OPERAND_UIMM7_LSB00,
  OPERAND_UIMM7_LSB000,
  OPERAND_UIMM8_LSB00,
  OPERAND_UIMM8,
  OPERAND_UIMM8_LSB000,
  OPERAND_UIMM8_GE32,
  OPERAND_UIMM9_LSB000,
  OPERAND_UIMM9,
  OPERAND_UIMM10,
  OPERAND_UIMM10_LSB00_NONZERO,
  OPERAND_UIMM11,
  OPERAND_UIMM12,
  OPERAND_UIMM14_LSB00,
  OPERAND_UIMM16,
  OPERAND_UIMM16_NONZERO,
  OPERAND_UIMMLOG2XLEN,
  OPERAND_UIMMLOG2XLEN_NONZERO,
  OPERAND_UIMM32,
  OPERAND_UIMM48,
  OPERAND_UIMM64,
  OPERAND_THREE,
  OPERAND_FOUR,
  OPERAND_IMM5_ZIBI,
  OPERAND_SIMM5,
  OPERAND_SIMM5_NONZERO,
  OPERAND_SIMM5_PLUS1,
  OPERAND_SIMM6,
  OPERAND_SIMM6_NONZERO,
  OPERAND_SIMM8_UNSIGNED,
  OPERAND_SIMM10,
  OPERAND_SIMM10_LSB0000_NONZERO,
  OPERAND_SIMM10_UNSIGNED,
  OPERAND_SIMM11,
  OPERAND_SIMM12_LSB00000,
  OPERAND_SIMM16,
  OPERAND_SIMM16_NONZERO,
  OPERAND_SIMM20_LI,
  OPERAND_SIMM26,
  OPERAND_CLUI_IMM,
  OPERAND_VTYPEI10,
  OPERAND_VTYPEI11,
  OPERAND_RVKRNUM,
  OPERAND_RVKRNUM_0_7,
  OPERAND_RVKRNUM_1_10,
  OPERAND_RVKRNUM_2_14,
  // Condition code used by select and short forward branch pseudos.
  OPERAND_COND_CODE,
  // Ordering for atomic pseudos.
  OPERAND_ATOMIC_ORDERING,
  OPERAND_LAST_YSX_IMM = OPERAND_ATOMIC_ORDERING,

  OPERAND_UIMM20_LUI,
  OPERAND_UIMM20_AUIPC,

  // Simm12 or constant pool, global, basicblock, etc.
  OPERAND_SIMM12_LO,

  OPERAND_BARE_SIMM32,

};
} // namespace YSXOp

// Describes the predecessor/successor bits used in the FENCE instruction.
namespace YSXFenceField {
enum FenceField {
  I = 8,
  O = 4,
  R = 2,
  W = 1
};
}

namespace YSXExceptFlags {
enum ExceptionFlag {
  NX = 0x01, // Inexact
  UF = 0x02, // Underflow
  OF = 0x04, // Overflow
  DZ = 0x08, // Divide by zero
  NV = 0x10, // Invalid operation
  ALL = 0x1F // Mask for all accrued exception flags
};
}

namespace YSXSysReg {
struct SysReg {
  const char Name[32];
  unsigned Encoding;
  // FIXME: add these additional fields when needed.
  // Privilege Access: Read, Write, Read-Only.
  // unsigned ReadWrite;
  // Privilege Mode: User, System or Machine.
  // unsigned Mode;
  // Check field name.
  // unsigned Extra;
  // Register number without the privilege bits.
  // unsigned Number;
  FeatureBitset FeaturesRequired;
  bool IsAltName;
  bool IsDeprecatedName;

  bool haveRequiredFeatures(const FeatureBitset &ActiveFeatures) const {
    // No required feature associated with the system register.
    if (FeaturesRequired.none())
      return true;
    return (FeaturesRequired & ActiveFeatures) == FeaturesRequired;
  }
};

inline ArrayRef<SysReg> lookupSysRegByEncoding(unsigned) { return {}; }
inline const SysReg *lookupSysRegByName(StringRef) { return nullptr; }
} // end namespace YSXSysReg

namespace YSXInsnOpcode {
struct YSXOpcode {
  char Name[10];
  uint8_t Value;
};

#define GET_YSXOpcodesList_DECL
#include "YSXGenSearchableTables.inc"
} // end namespace YSXInsnOpcode

namespace YSXABI {

enum ABI {
  ABI_LP64,
  ABI_Unknown
};

// Returns the target ABI, or else a StringError if the requested ABIName is
// not supported for the given TT and FeatureBits combination.
ABI computeTargetABI(const Triple &TT, const FeatureBitset &FeatureBits,
                     StringRef ABIName);

ABI getTargetABI(StringRef ABIName);

// Returns the register used to hold the stack pointer after realignment.
MCRegister getBPReg();

// Returns the register holding shadow call stack pointer.
MCRegister getSCSPReg();

} // namespace YSXABI

namespace YSXFeatures {

// Validates if the given combination of features are valid for the target
// triple. Exits with report_fatal_error if not.
void validate(const Triple &TT, const FeatureBitset &FeatureBits);

bool isValidYSXISAInfo(const YSXISAInfo &ISAInfo);

llvm::Expected<std::unique_ptr<YSXISAInfo>>
parseFeatureBits(bool IsRV64, const FeatureBitset &FeatureBits);

} // namespace YSXFeatures

} // namespace llvm

#endif
