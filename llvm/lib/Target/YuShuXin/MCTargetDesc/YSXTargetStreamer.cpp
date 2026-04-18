//===-- YSXTargetStreamer.cpp - RISC-V Target Streamer Methods ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides RISC-V specific target streamer methods.
//
//===----------------------------------------------------------------------===//

#include "YSXTargetStreamer.h"
#include "YSXBaseInfo.h"
#include "YSXMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/RISCVAttributes.h"
#include "llvm/TargetParser/YSXISAInfo.h"

using namespace llvm;

// This option controls whether or not we emit ELF attributes for ABI features,
// like RISC-V atomics or X3 usage.
static cl::opt<bool> RiscvAbiAttr(
    "ysx-abi-attributes",
    cl::desc("Enable emitting RISC-V ELF attributes for ABI features"),
    cl::Hidden);

YSXTargetStreamer::YSXTargetStreamer(MCStreamer &S) : MCTargetStreamer(S) {}

void YSXTargetStreamer::finish() { finishAttributeSection(); }
void YSXTargetStreamer::reset() {}

void YSXTargetStreamer::emitDirectiveOptionArch(
    ArrayRef<YSXOptionArchArg> Args) {}
void YSXTargetStreamer::emitDirectiveOptionExact() {}
void YSXTargetStreamer::emitDirectiveOptionNoExact() {}
void YSXTargetStreamer::emitDirectiveOptionPIC() {}
void YSXTargetStreamer::emitDirectiveOptionNoPIC() {}
void YSXTargetStreamer::emitDirectiveOptionPop() {}
void YSXTargetStreamer::emitDirectiveOptionPush() {}
void YSXTargetStreamer::emitDirectiveOptionRelax() {}
void YSXTargetStreamer::emitDirectiveOptionNoRelax() {}
void YSXTargetStreamer::emitDirectiveOptionRVC() {}
void YSXTargetStreamer::emitDirectiveOptionNoRVC() {}
void YSXTargetStreamer::emitDirectiveVariantCC(MCSymbol &Symbol) {}
void YSXTargetStreamer::emitAttribute(unsigned Attribute, unsigned Value) {}
void YSXTargetStreamer::finishAttributeSection() {}
void YSXTargetStreamer::emitTextAttribute(unsigned Attribute,
                                            StringRef String) {}
void YSXTargetStreamer::emitIntTextAttribute(unsigned Attribute,
                                               unsigned IntValue,
                                               StringRef StringValue) {}

void YSXTargetStreamer::setTargetABI(YSXABI::ABI ABI) {
  assert(ABI != YSXABI::ABI_Unknown && "Improperly initialized target ABI");
  TargetABI = ABI;
}

void YSXTargetStreamer::setFlagsFromFeatures(const MCSubtargetInfo &STI) {
  HasRVC = STI.hasFeature(YSX::FeatureStdExtZca);
  HasTSO = STI.hasFeature(YSX::FeatureStdExtZtso);
}

void YSXTargetStreamer::emitTargetAttributes(const MCSubtargetInfo &STI,
                                               bool EmitStackAlign) {
  if (EmitStackAlign) {
    unsigned StackAlign;
    if (TargetABI == YSXABI::ABI_ILP32E)
      StackAlign = 4;
    else if (TargetABI == YSXABI::ABI_LP64E)
      StackAlign = 8;
    else
      StackAlign = 16;
    emitAttribute(RISCVAttrs::STACK_ALIGN, StackAlign);
  }

  auto ParseResult = YSXFeatures::parseFeatureBits(
      STI.hasFeature(YSX::Feature64Bit), STI.getFeatureBits());
  if (!ParseResult) {
    report_fatal_error(ParseResult.takeError());
  } else {
    auto &ISAInfo = *ParseResult;
    emitTextAttribute(RISCVAttrs::ARCH, ISAInfo->toString());
  }

  if (RiscvAbiAttr && STI.hasFeature(YSX::FeatureStdExtA)) {
    unsigned AtomicABITag;
    if (STI.hasFeature(YSX::FeatureStdExtZalasr))
      AtomicABITag = static_cast<unsigned>(RISCVAttrs::RISCVAtomicAbiTag::A7);
    else if (STI.hasFeature(YSX::FeatureNoTrailingSeqCstFence))
      AtomicABITag =
          static_cast<unsigned>(RISCVAttrs::RISCVAtomicAbiTag::A6C);
    else
      AtomicABITag =
          static_cast<unsigned>(RISCVAttrs::RISCVAtomicAbiTag::A6S);
    emitAttribute(RISCVAttrs::ATOMIC_ABI, AtomicABITag);
  }
}

// This part is for ascii assembly output
YSXTargetAsmStreamer::YSXTargetAsmStreamer(MCStreamer &S,
                                               formatted_raw_ostream &OS)
    : YSXTargetStreamer(S), OS(OS) {}

void YSXTargetAsmStreamer::emitDirectiveOptionPush() {
  OS << "\t.option\tpush\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionPop() {
  OS << "\t.option\tpop\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionPIC() {
  OS << "\t.option\tpic\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionNoPIC() {
  OS << "\t.option\tnopic\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionRVC() {
  OS << "\t.option\trvc\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionNoRVC() {
  OS << "\t.option\tnorvc\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionExact() {
  OS << "\t.option\texact\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionNoExact() {
  OS << "\t.option\tnoexact\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionRelax() {
  OS << "\t.option\trelax\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionNoRelax() {
  OS << "\t.option\tnorelax\n";
}

void YSXTargetAsmStreamer::emitDirectiveOptionArch(
    ArrayRef<YSXOptionArchArg> Args) {
  OS << "\t.option\tarch";
  for (const auto &Arg : Args) {
    OS << ", ";
    switch (Arg.Type) {
    case YSXOptionArchArgType::Full:
      break;
    case YSXOptionArchArgType::Plus:
      OS << "+";
      break;
    case YSXOptionArchArgType::Minus:
      OS << "-";
      break;
    }
    OS << Arg.Value;
  }
  OS << "\n";
}

void YSXTargetAsmStreamer::emitDirectiveVariantCC(MCSymbol &Symbol) {
  OS << "\t.variant_cc\t" << Symbol.getName() << "\n";
}

void YSXTargetAsmStreamer::emitAttribute(unsigned Attribute, unsigned Value) {
  OS << "\t.attribute\t" << Attribute << ", " << Twine(Value) << "\n";
}

void YSXTargetAsmStreamer::emitTextAttribute(unsigned Attribute,
                                               StringRef String) {
  OS << "\t.attribute\t" << Attribute << ", \"" << String << "\"\n";
}

void YSXTargetAsmStreamer::emitIntTextAttribute(unsigned Attribute,
                                                  unsigned IntValue,
                                                  StringRef StringValue) {}

void YSXTargetAsmStreamer::finishAttributeSection() {}
