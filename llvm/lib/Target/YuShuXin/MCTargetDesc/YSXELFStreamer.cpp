//===-- YSXELFStreamer.cpp - RISC-V ELF Target Streamer Methods ---------===//
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

#include "YSXELFStreamer.h"
#include "YSXAsmBackend.h"
#include "YSXBaseInfo.h"
#include "YSXMCTargetDesc.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

// This part is for ELF object output.
YSXTargetELFStreamer::YSXTargetELFStreamer(MCStreamer &S,
                                               const MCSubtargetInfo &STI)
    : YSXTargetStreamer(S), AttributeNamespace("riscv") {
  MCAssembler &MCA = getStreamer().getAssembler();
  const FeatureBitset &Features = STI.getFeatureBits();
  auto &MAB = static_cast<YSXAsmBackend &>(MCA.getBackend());
  setTargetABI(YSXABI::computeTargetABI(STI.getTargetTriple(), Features,
                                          MAB.getTargetOptions().getABIName()));
  setFlagsFromFeatures(STI);
}

YSXELFStreamer::YSXELFStreamer(MCContext &C,
                                   std::unique_ptr<MCAsmBackend> MAB,
                                   std::unique_ptr<MCObjectWriter> MOW,
                                   std::unique_ptr<MCCodeEmitter> MCE)
    : MCELFStreamer(C, std::move(MAB), std::move(MOW), std::move(MCE)) {}

YSXELFStreamer &YSXTargetELFStreamer::getStreamer() {
  return static_cast<YSXELFStreamer &>(Streamer);
}

void YSXTargetELFStreamer::emitDirectiveOptionExact() {}
void YSXTargetELFStreamer::emitDirectiveOptionNoExact() {}
void YSXTargetELFStreamer::emitDirectiveOptionPIC() {}
void YSXTargetELFStreamer::emitDirectiveOptionNoPIC() {}
void YSXTargetELFStreamer::emitDirectiveOptionPop() {}
void YSXTargetELFStreamer::emitDirectiveOptionPush() {}
void YSXTargetELFStreamer::emitDirectiveOptionRelax() {}
void YSXTargetELFStreamer::emitDirectiveOptionNoRelax() {}

void YSXTargetELFStreamer::emitAttribute(unsigned Attribute, unsigned Value) {
  getStreamer().setAttributeItem(Attribute, Value, /*OverwriteExisting=*/true);
}

void YSXTargetELFStreamer::emitTextAttribute(unsigned Attribute,
                                               StringRef String) {
  getStreamer().setAttributeItem(Attribute, String, /*OverwriteExisting=*/true);
}

void YSXTargetELFStreamer::emitIntTextAttribute(unsigned Attribute,
                                                  unsigned IntValue,
                                                  StringRef StringValue) {
  getStreamer().setAttributeItems(Attribute, IntValue, StringValue,
                                  /*OverwriteExisting=*/true);
}

void YSXTargetELFStreamer::finishAttributeSection() {
  YSXELFStreamer &S = getStreamer();
  if (S.Contents.empty())
    return;

  S.emitAttributesSection(AttributeNamespace, ".riscv.attributes",
                          ELF::SHT_RISCV_ATTRIBUTES, AttributeSection);
}

void YSXTargetELFStreamer::finish() {
  YSXTargetStreamer::finish();
  ELFObjectWriter &W = getStreamer().getWriter();
  YSXABI::ABI ABI = getTargetABI();

  unsigned EFlags = W.getELFHeaderEFlags();

  if (hasTSO())
    EFlags |= ELF::EF_RISCV_TSO;

  switch (ABI) {
  case YSXABI::ABI_LP64:
    break;
  case YSXABI::ABI_Unknown:
    llvm_unreachable("Improperly initialised target ABI");
  }

  W.setELFHeaderEFlags(EFlags);
}

void YSXTargetELFStreamer::reset() {
  AttributeSection = nullptr;
}

void YSXELFStreamer::reset() {
  static_cast<YSXTargetStreamer *>(getTargetStreamer())->reset();
  MCELFStreamer::reset();
  LastMappingSymbols.clear();
  LastEMS = EMS_None;
}

void YSXELFStreamer::emitDataMappingSymbol() {
  if (LastEMS == EMS_Data)
    return;
  emitMappingSymbol("$d");
  LastEMS = EMS_Data;
}

void YSXELFStreamer::emitInstructionsMappingSymbol() {
  if (LastEMS == EMS_Instructions)
    return;
  emitMappingSymbol("$x");
  LastEMS = EMS_Instructions;
}

void YSXELFStreamer::emitMappingSymbol(StringRef Name) {
  auto *Symbol =
      static_cast<MCSymbolELF *>(getContext().createLocalSymbol(Name));
  emitLabel(Symbol);
  Symbol->setType(ELF::STT_NOTYPE);
  Symbol->setBinding(ELF::STB_LOCAL);
}

void YSXELFStreamer::changeSection(MCSection *Section, uint32_t Subsection) {
  // We have to keep track of the mapping symbol state of any sections we
  // use. Each one should start off as EMS_None, which is provided as the
  // default constructor by DenseMap::lookup.
  LastMappingSymbols[getPreviousSection().first] = LastEMS;
  LastEMS = LastMappingSymbols.lookup(Section);

  MCELFStreamer::changeSection(Section, Subsection);
}

void YSXELFStreamer::emitInstruction(const MCInst &Inst,
                                       const MCSubtargetInfo &STI) {
  emitInstructionsMappingSymbol();
  MCELFStreamer::emitInstruction(Inst, STI);
}

void YSXELFStreamer::emitBytes(StringRef Data) {
  emitDataMappingSymbol();
  MCELFStreamer::emitBytes(Data);
}

void YSXELFStreamer::emitFill(const MCExpr &NumBytes, uint64_t FillValue,
                                SMLoc Loc) {
  emitDataMappingSymbol();
  MCELFStreamer::emitFill(NumBytes, FillValue, Loc);
}

void YSXELFStreamer::emitValueImpl(const MCExpr *Value, unsigned Size,
                                     SMLoc Loc) {
  emitDataMappingSymbol();
  MCELFStreamer::emitValueImpl(Value, Size, Loc);
}

MCStreamer *llvm::createYSXELFStreamer(const Triple &, MCContext &C,
                                         std::unique_ptr<MCAsmBackend> &&MAB,
                                         std::unique_ptr<MCObjectWriter> &&MOW,
                                         std::unique_ptr<MCCodeEmitter> &&MCE) {
  return new YSXELFStreamer(C, std::move(MAB), std::move(MOW),
                              std::move(MCE));
}

void YSXTargetELFStreamer::emitNoteGnuPropertySection(
    const uint32_t Feature1And) {
  MCStreamer &OutStreamer = getStreamer();
  MCContext &Ctx = OutStreamer.getContext();

  const Triple &Triple = Ctx.getTargetTriple();
  Align NoteAlign;
  uint64_t DescSize;
  if (Triple.isArch64Bit()) {
    NoteAlign = Align(8);
    DescSize = 16;
  } else {
    assert(Triple.isArch32Bit());
    NoteAlign = Align(4);
    DescSize = 12;
  }

  assert(Ctx.getObjectFileType() == MCContext::Environment::IsELF);
  MCSection *const NoteSection =
      Ctx.getELFSection(".note.gnu.property", ELF::SHT_NOTE, ELF::SHF_ALLOC);
  OutStreamer.pushSection();
  OutStreamer.switchSection(NoteSection);

  // Emit the note header
  OutStreamer.emitValueToAlignment(NoteAlign);
  OutStreamer.emitIntValue(4, 4);                           // n_namsz
  OutStreamer.emitIntValue(DescSize, 4);                    // n_descsz
  OutStreamer.emitIntValue(ELF::NT_GNU_PROPERTY_TYPE_0, 4); // n_type
  OutStreamer.emitBytes(StringRef("GNU", 4));               // n_name

  // Emit n_desc field

  // Emit the feature_1_and property
  OutStreamer.emitIntValue(ELF::GNU_PROPERTY_RISCV_FEATURE_1_AND, 4); // pr_type
  OutStreamer.emitIntValue(4, 4);              // pr_datasz
  OutStreamer.emitIntValue(Feature1And, 4);    // pr_data
  OutStreamer.emitValueToAlignment(NoteAlign); // pr_padding

  OutStreamer.popSection();
}
