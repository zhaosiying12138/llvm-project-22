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

  static StringRef getRISCVAttributeString() {
    return "rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0";
  }

  std::string toRISCVAttributeString() const {
    std::string Attr = getRISCVAttributeString().str();
    if (hasExtension("xtinyf"))
      Attr += "_xtinyf1p0";
    if (hasExtension("xtinyv"))
      Attr += "_xtinyv1p0";
    if (hasExtension("zvl128b"))
      Attr += "_zvl128b1p0";
    return Attr;
  }

  StringRef computeDefaultABI() const { return "lp64"; }

  static bool isSupportedExtensionFeature(StringRef Feature) {
    return Feature == "i" || Feature == "m" || Feature == "a" ||
           Feature == "zmmul" || Feature == "zaamo" ||
           Feature == "zalrsc" || Feature == "xtinyf" ||
           Feature == "xtinyv" || Feature == "zvl128b";
  }

  static std::string getTargetFeatureForExtension(StringRef Ext) {
    return Ext.lower();
  }

  static std::unique_ptr<YSXISAInfo> createRV64IMAInfo() {
    auto Info =
        std::unique_ptr<YSXISAInfo>(new YSXISAInfo(64, StringRef("rv64ima")));
    Info->addExtension("i", 2, 1);
    Info->addExtension("m", 2, 0);
    Info->addExtension("zmmul", 1, 0);
    Info->addExtension("a", 2, 1);
    Info->addExtension("zaamo", 1, 0);
    Info->addExtension("zalrsc", 1, 0);
    return Info;
  }

  static StringRef normalizeOptionalExtension(StringRef Ext) {
    if (Ext == "xtinyf1p0")
      return "xtinyf";
    if (Ext == "xtinyv1p0")
      return "xtinyv";
    if (Ext == "zvl128b1p0")
      return "zvl128b";
    return Ext;
  }

  static Error addOptionalExtension(YSXISAInfo &Info, StringRef Ext) {
    Ext = normalizeOptionalExtension(Ext);
    if (Ext == "xtinyf") {
      Info.addExtension("xtinyf", 1, 0);
      return Error::success();
    }
    if (Ext == "xtinyv") {
      Info.addExtension("xtinyv", 1, 0);
      Info.addExtension("zvl128b", 1, 0);
      return Error::success();
    }
    if (Ext == "zvl128b") {
      Info.addExtension("zvl128b", 1, 0);
      return Error::success();
    }
    return unsupportedArch(Ext);
  }

  void updateArchString() {
    ArchString = "rv64ima";
    if (hasExtension("xtinyf"))
      ArchString += "_xtinyf";
    if (hasExtension("xtinyv"))
      ArchString += "_xtinyv";
    if (hasExtension("zvl128b"))
      ArchString += "_zvl128b";
  }

  static Expected<std::unique_ptr<YSXISAInfo>>
  parseFeatures(unsigned XLen, const std::vector<std::string> &Features) {
    if (XLen != 64)
      return unsupportedArch("rv32ima");

    auto Info =
        std::unique_ptr<YSXISAInfo>(new YSXISAInfo(64, StringRef("rv64ima")));

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
      } else if (Feature == "xtinyf" || Feature == "xtinyv" ||
                 Feature == "zvl128b") {
        if (Error Err = addOptionalExtension(*Info, Feature))
          return std::move(Err);
      } else {
        return unsupportedArch(Feature);
      }
    }

    if (!Info->hasExtension("i") || !Info->hasExtension("m") ||
        !Info->hasExtension("a"))
      return unsupportedArch("rv64ima");

    Info->updateArchString();
    return Info;
  }

  static Expected<std::unique_ptr<YSXISAInfo>>
  parseArchString(StringRef Arch, bool EnableExperimentalExtension,
                  bool ExperimentalExtensionVersionCheck = true) {
    std::string Lower = Arch.lower();
    StringRef LowerArch(Lower);
    auto Info = createRV64IMAInfo();
    if (LowerArch == "rv64ima" || LowerArch == getRISCVAttributeString())
      return Info;

    if (!LowerArch.consume_front("rv64ima_")) {
      if (!LowerArch.consume_front(getRISCVAttributeString()))
        return unsupportedArch(Arch);
      if (!LowerArch.consume_front("_"))
        return unsupportedArch(Arch);
    }

    SmallVector<StringRef, 4> Exts;
    LowerArch.split(Exts, "_", /*MaxSplit=*/-1, /*KeepEmpty=*/false);
    for (StringRef Ext : Exts)
      if (Error Err = addOptionalExtension(*Info, Ext))
        return std::move(Err);

    Info->updateArchString();
    return Info;
  }
};

} // namespace llvm

#endif // LLVM_TARGETPARSER_YSXISAINFO_H
