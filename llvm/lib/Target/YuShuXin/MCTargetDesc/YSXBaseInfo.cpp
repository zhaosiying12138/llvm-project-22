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
  if (!ABIName.empty() && ABIName != "lp64")
    reportFatalUsageError("YSX only supports the lp64 ABI");
  return ABI_LP64;
}

ABI getTargetABI(StringRef ABIName) {
  return ABIName == "lp64" ? ABI_LP64 : ABI_Unknown;
}

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
  if (!TT.isYSX64() || !FeatureBits[YSX::Feature64Bit])
    reportFatalUsageError("YSX requires a 64-bit target");

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

} // namespace llvm
