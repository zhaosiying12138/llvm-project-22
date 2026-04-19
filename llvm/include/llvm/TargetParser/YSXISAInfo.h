//===-- YSXISAInfo.h - YSX ISA Information ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TARGETPARSER_YSXISAINFO_H
#define LLVM_TARGETPARSER_YSXISAINFO_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/TargetParser/RISCVTargetParser.h"
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace llvm {

class YSXISAInfo {
public:
  struct ExtensionVersion {
    unsigned Major;
    unsigned Minor;
  };

private:
  unsigned XLen;
  std::string ArchString;
  std::vector<std::pair<std::string, ExtensionVersion>> Extensions;

  YSXISAInfo(unsigned XLen, StringRef Arch) : XLen(XLen), ArchString(Arch) {}

  void addExtension(StringRef Name, unsigned Major, unsigned Minor) {
    if (hasExtension(Name))
      return;
    Extensions.push_back({Name.str(), {Major, Minor}});
  }

  static Error unsupportedArch(StringRef Arch) {
    return createStringError(std::errc::invalid_argument,
                             "YSX only supports arch string rv64ima: %s",
                             Arch.str().c_str());
  }

public:
  unsigned getXLen() const { return XLen; }

  bool hasExtension(StringRef Name) const {
    for (const auto &Extension : Extensions)
      if (Extension.first == Name)
        return true;
    return false;
  }

  ArrayRef<std::pair<std::string, ExtensionVersion>> getExtensions() const {
    return Extensions;
  }

  std::vector<std::string> toFeatures(bool AddAllExtensions = false,
                                      bool IgnoreUnknown = false) const {
    std::vector<std::string> Features;
    for (const auto &Extension : Extensions)
      Features.push_back("+" + Extension.first);
    return Features;
  }

  std::string toString() const { return ArchString; }

  StringRef computeDefaultABI() const { return XLen == 64 ? "lp64" : "ilp32"; }

  static bool isSupportedExtensionFeature(StringRef Feature) {
    return Feature == "i" || Feature == "m" || Feature == "a" ||
           Feature == "zmmul" || Feature == "zaamo" ||
           Feature == "zalrsc";
  }

  static std::string getTargetFeatureForExtension(StringRef Ext) {
    return Ext.lower();
  }

  static Expected<std::unique_ptr<YSXISAInfo>>
  parseFeatures(unsigned XLen, const std::vector<std::string> &Features) {
    auto Info = std::unique_ptr<YSXISAInfo>(new YSXISAInfo(
        XLen, XLen == 64 ? StringRef("rv64ima") : StringRef("rv32ima")));

    for (StringRef Feature : Features) {
      bool Enabled = true;
      if (Feature.consume_front("+"))
        Enabled = true;
      else if (Feature.consume_front("-"))
        Enabled = false;
      std::string LowerFeature = Feature.lower();
      Feature = LowerFeature;

      if (!Enabled)
        continue;

      if (Feature == "m") {
        Info->addExtension("m", 2, 0);
        Info->addExtension("zmmul", 1, 0);
      } else if (Feature == "a") {
        Info->addExtension("a", 2, 1);
        Info->addExtension("zaamo", 1, 0);
        Info->addExtension("zalrsc", 1, 0);
      } else if (Feature == "i") {
        Info->addExtension("i", 2, 1);
      } else if (Feature == "zmmul") {
        Info->addExtension("zmmul", 1, 0);
      } else if (Feature == "zaamo") {
        Info->addExtension("zaamo", 1, 0);
      } else if (Feature == "zalrsc") {
        Info->addExtension("zalrsc", 1, 0);
      } else {
        Info->addExtension(Feature, 0, 0);
      }
    }

    return Info;
  }

  static Expected<std::unique_ptr<YSXISAInfo>>
  parseArchString(StringRef Arch, bool EnableExperimentalExtension,
                  bool ExperimentalExtensionVersionCheck = true) {
    std::string Lower = Arch.lower();
    StringRef LowerArch(Lower);
    if (!LowerArch.starts_with("rv32") && !LowerArch.starts_with("rv64"))
      return unsupportedArch(Arch);

    unsigned XLen = LowerArch.starts_with("rv64") ? 64 : 32;
    auto Info = std::unique_ptr<YSXISAInfo>(new YSXISAInfo(XLen, LowerArch));
    StringRef Exts = LowerArch.drop_front(4);

    SmallVector<StringRef, 8> Groups;
    Exts.split(Groups, "_", /*MaxSplit=*/-1, /*KeepEmpty=*/false);
    for (StringRef Group : Groups) {
      if (Group.empty())
        continue;

      if (Group.size() > 1 && (Group.front() == 'z' || Group.front() == 's' ||
                               Group.front() == 'x')) {
        if (Group == "zmmul")
          Info->addExtension("zmmul", 1, 0);
        else if (Group == "zaamo")
          Info->addExtension("zaamo", 1, 0);
        else if (Group == "zalrsc")
          Info->addExtension("zalrsc", 1, 0);
        else
          Info->addExtension(Group, 0, 0);
        continue;
      }

      for (char C : Group) {
        switch (C) {
        case 'i':
          Info->addExtension("i", 2, 1);
          break;
        case 'm':
          Info->addExtension("m", 2, 0);
          Info->addExtension("zmmul", 1, 0);
          break;
        case 'a':
          Info->addExtension("a", 2, 1);
          Info->addExtension("zaamo", 1, 0);
          Info->addExtension("zalrsc", 1, 0);
          break;
        case 'g':
          Info->addExtension("i", 2, 1);
          Info->addExtension("m", 2, 0);
          Info->addExtension("a", 2, 1);
          Info->addExtension("zmmul", 1, 0);
          Info->addExtension("zaamo", 1, 0);
          Info->addExtension("zalrsc", 1, 0);
          Info->addExtension("f", 2, 2);
          Info->addExtension("d", 2, 2);
          Info->addExtension("zicsr", 2, 0);
          Info->addExtension("zifencei", 2, 0);
          break;
        case 'c':
          Info->addExtension("c", 2, 0);
          break;
        case 'f':
          Info->addExtension("f", 2, 2);
          break;
        case 'd':
          Info->addExtension("d", 2, 2);
          break;
        case 'v':
          Info->addExtension("v", 1, 0);
          break;
        default:
          return unsupportedArch(Arch);
        }
      }
    }

    return Info;
  }
};

namespace YSXVType {
using RISCVVType::decodeTWiden;
using RISCVVType::decodeVLMUL;
using RISCVVType::decodeVSEW;
using RISCVVType::encodeLMUL;
using RISCVVType::encodeSEW;
using RISCVVType::encodeVTYPE;
using RISCVVType::encodeXSfmmVType;
using RISCVVType::getSameRatioLMUL;
using RISCVVType::getSEW;
using RISCVVType::getSEWLMULRatio;
using RISCVVType::getVLMUL;
using RISCVVType::getXSfmmWiden;
using RISCVVType::hasXSfmmWiden;
using RISCVVType::isAltFmt;
using RISCVVType::isMaskAgnostic;
using RISCVVType::isTailAgnostic;
using RISCVVType::isValidLMUL;
using RISCVVType::isValidSEW;
using RISCVVType::isValidXSfmmVType;
using RISCVVType::LMUL_1;
using RISCVVType::LMUL_2;
using RISCVVType::LMUL_4;
using RISCVVType::LMUL_8;
using RISCVVType::LMUL_F2;
using RISCVVType::LMUL_F4;
using RISCVVType::LMUL_F8;
using RISCVVType::LMUL_RESERVED;
using RISCVVType::MASK_AGNOSTIC;
using RISCVVType::printVType;
using RISCVVType::printXSfmmVType;
using RISCVVType::TAIL_AGNOSTIC;
using RISCVVType::TAIL_UNDISTURBED_MASK_UNDISTURBED;
using RISCVVType::VLMUL;
} // namespace YSXVType

} // namespace llvm

#endif // LLVM_TARGETPARSER_YSXISAINFO_H
