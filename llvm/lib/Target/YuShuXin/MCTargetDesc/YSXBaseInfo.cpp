//===-- YSXBaseInfo.cpp - Top level definitions for RISC-V MC -----------===//
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

#include "YSXBaseInfo.h"
#include "YSXInstrInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/TargetParser.h"
#include "llvm/TargetParser/Triple.h"

namespace llvm {

extern const SubtargetFeatureKV YSXFeatureKV[YSX::NumSubtargetFeatures];

namespace YSXInsnOpcode {
#define GET_YSXOpcodesList_IMPL
#include "YSXGenSearchableTables.inc"
} // namespace YSXInsnOpcode

namespace YSXABI {
ABI computeTargetABI(const Triple &TT, const FeatureBitset &FeatureBits,
                     StringRef ABIName) {
  if (TT.isYSX64()) {
    if (!ABIName.empty() && ABIName != "lp64")
      reportFatalUsageError("YSX only supports the lp64 ABI");
    return ABI_LP64;
  }

  auto TargetABI = getTargetABI(ABIName);
  bool IsRV64 = TT.isArch64Bit();
  bool IsRVE = false;

  if (!ABIName.empty() && TargetABI == ABI_Unknown) {
    errs()
        << "'" << ABIName
        << "' is not a recognized ABI for this target (ignoring target-abi)\n";
  } else if (ABIName.starts_with("ilp32") && IsRV64) {
    errs() << "32-bit ABIs are not supported for 64-bit targets (ignoring "
              "target-abi)\n";
    TargetABI = ABI_Unknown;
  } else if (ABIName.starts_with("lp64") && !IsRV64) {
    errs() << "64-bit ABIs are not supported for 32-bit targets (ignoring "
              "target-abi)\n";
    TargetABI = ABI_Unknown;
  } else if (!IsRV64 && IsRVE && TargetABI != ABI_ILP32E &&
             TargetABI != ABI_Unknown) {
    // TODO: move this checking to YSXTargetLowering and YSXAsmParser
    errs()
        << "Only the ilp32e ABI is supported for RV32E (ignoring target-abi)\n";
    TargetABI = ABI_Unknown;
  } else if (IsRV64 && IsRVE && TargetABI != ABI_LP64E &&
             TargetABI != ABI_Unknown) {
    // TODO: move this checking to YSXTargetLowering and YSXAsmParser
    errs()
        << "Only the lp64e ABI is supported for RV64E (ignoring target-abi)\n";
    TargetABI = ABI_Unknown;
  }

  if ((TargetABI == YSXABI::ABI::ABI_ILP32E ||
       (TargetABI == ABI_Unknown && IsRVE && !IsRV64)) &&
      false)
    reportFatalUsageError("ILP32E cannot be used with the D ISA extension");

  if (TargetABI != ABI_Unknown)
    return TargetABI;

  // If no explicit ABI is given, try to compute the default ABI.
  auto ISAInfo = YSXFeatures::parseFeatureBits(IsRV64, FeatureBits);
  if (!ISAInfo)
    reportFatalUsageError(ISAInfo.takeError());
  return getTargetABI((*ISAInfo)->computeDefaultABI());
}

ABI getTargetABI(StringRef ABIName) {
  auto TargetABI = StringSwitch<ABI>(ABIName)
                       .Case("ilp32", ABI_ILP32)
                       .Case("ilp32f", ABI_ILP32F)
                       .Case("ilp32d", ABI_ILP32D)
                       .Case("ilp32e", ABI_ILP32E)
                       .Case("lp64", ABI_LP64)
                       .Case("lp64f", ABI_LP64F)
                       .Case("lp64d", ABI_LP64D)
                       .Case("lp64e", ABI_LP64E)
                       .Default(ABI_Unknown);
  return TargetABI;
}

// To avoid the BP value clobbered by a function call, we need to choose a
// callee saved register to save the value. RV32E only has X8 and X9 as callee
// saved registers and X8 will be used as fp. So we choose X9 as bp.
MCRegister getBPReg() { return YSX::X9; }

// Returns the register holding shadow call stack pointer.
MCRegister getSCSPReg() { return YSX::X3; }

} // namespace YSXABI

namespace YSXFeatures {

bool isValidYSXISAInfo(const YSXISAInfo &ISAInfo) {
  if (ISAInfo.getXLen() != 64 || !ISAInfo.hasExtension("i") ||
      !ISAInfo.hasExtension("m") || !ISAInfo.hasExtension("a"))
    return false;

  for (const std::string &Feature : ISAInfo.toFeatures(/*AddAllExtensions=*/false,
                                                       /*IgnoreUnknown=*/false)) {
    StringRef Ext = Feature;
    Ext.consume_front("+");
    if (Ext != "i" && Ext != "m" && Ext != "a" && Ext != "zmmul" &&
        Ext != "zaamo" && Ext != "zalrsc")
      return false;
  }

  return true;
}

void validate(const Triple &TT, const FeatureBitset &FeatureBits) {
  if (TT.isArch64Bit() && !FeatureBits[YSX::Feature64Bit])
    reportFatalUsageError("RV64 target requires an RV64 CPU");
  if (!TT.isArch64Bit() && !FeatureBits[YSX::Feature32Bit])
    reportFatalUsageError("RV32 target requires an RV32 CPU");
  if (FeatureBits[YSX::Feature32Bit] &&
      FeatureBits[YSX::Feature64Bit])
    reportFatalUsageError("RV32 and RV64 can't be combined");

  if (TT.isYSX64()) {
    auto ISAInfo = parseFeatureBits(/*IsRV64=*/true, FeatureBits);
    if (!ISAInfo)
      reportFatalUsageError(ISAInfo.takeError());
    if (!isValidYSXISAInfo(**ISAInfo))
      reportFatalUsageError("YSX only supports the rv64ima ISA");
  }
}

llvm::Expected<std::unique_ptr<YSXISAInfo>>
parseFeatureBits(bool IsRV64, const FeatureBitset &FeatureBits) {
  unsigned XLen = IsRV64 ? 64 : 32;
  std::vector<std::string> FeatureVector;
  // Convert FeatureBitset to FeatureVector.
  for (auto Feature : YSXFeatureKV) {
    if (FeatureBits[Feature.Value] &&
        llvm::YSXISAInfo::isSupportedExtensionFeature(Feature.Key))
      FeatureVector.push_back(std::string("+") + Feature.Key);
  }
  return llvm::YSXISAInfo::parseFeatures(XLen, FeatureVector);
}

} // namespace YSXFeatures

bool YSXRVC::compress(MCInst &OutInst, const MCInst &MI,
                        const MCSubtargetInfo &STI) {
  return false;
}

bool YSXRVC::uncompress(MCInst &OutInst, const MCInst &MI,
                          const MCSubtargetInfo &STI) {
  return false;
}

void YSXZC::printRegList(unsigned RlistEncode, raw_ostream &OS) {
  assert(RlistEncode >= RLISTENCODE::RA &&
         RlistEncode <= RLISTENCODE::RA_S0_S11 && "Invalid Rlist");
  OS << "{ra";
  if (RlistEncode > YSXZC::RA) {
    OS << ", s0";
    if (RlistEncode == YSXZC::RA_S0_S11)
      OS << "-s11";
    else if (RlistEncode > YSXZC::RA_S0 && RlistEncode <= YSXZC::RA_S0_S11)
      OS << "-s" << (RlistEncode - YSXZC::RA_S0);
  }
  OS << "}";
}

} // namespace llvm
