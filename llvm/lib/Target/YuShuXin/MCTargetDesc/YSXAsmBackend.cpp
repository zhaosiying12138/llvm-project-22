//===-- YSXAsmBackend.cpp - RISC-V Assembler Backend --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "YSXAsmBackend.h"
#include "YSXFixupKinds.h"
#include "llvm/ADT/APInt.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCMachObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// Temporary workaround for old linkers that do not support ULEB128 relocations,
// which are abused by DWARF v5 DW_LLE_offset_pair/DW_RLE_offset_pair
// implemented in Clang/LLVM.
static cl::opt<bool> ULEB128Reloc(
    "ysx-uleb128-reloc", cl::init(true), cl::Hidden,
    cl::desc("Emit R_RISCV_SET_ULEB128/E_RISCV_SUB_ULEB128 if appropriate"));

YSXAsmBackend::YSXAsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI,
                                 bool Is64Bit, bool IsLittleEndian,
                                 const MCTargetOptions &Options)
    : MCAsmBackend(IsLittleEndian ? llvm::endianness::little
                                  : llvm::endianness::big),
      STI(STI), OSABI(OSABI), Is64Bit(Is64Bit), TargetOptions(Options) {
  YSXFeatures::validate(STI.getTargetTriple(), STI.getFeatureBits());
}

std::optional<MCFixupKind> YSXAsmBackend::getFixupKind(StringRef Name) const {
  if (STI.getTargetTriple().isOSBinFormatELF()) {
    unsigned Type;
    Type = llvm::StringSwitch<unsigned>(Name)
#define ELF_RELOC(NAME, ID) .Case(#NAME, ID)
#include "llvm/BinaryFormat/ELFRelocs/RISCV.def"
#undef ELF_RELOC
#define ELF_RISCV_NONSTANDARD_RELOC(_VENDOR, NAME, ID) .Case(#NAME, ID)
#include "llvm/BinaryFormat/ELFRelocs/RISCV_nonstandard.def"
#undef ELF_RISCV_NONSTANDARD_RELOC
               .Case("BFD_RELOC_NONE", ELF::R_RISCV_NONE)
               .Case("BFD_RELOC_32", ELF::R_RISCV_32)
               .Case("BFD_RELOC_64", ELF::R_RISCV_64)
               .Default(-1u);
    if (Type != -1u)
      return static_cast<MCFixupKind>(FirstLiteralRelocationKind + Type);
  }
  return std::nullopt;
}

MCFixupKindInfo YSXAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  const static MCFixupKindInfo Infos[] = {
      // This table *must* be in the order that the fixup_* kinds are defined in
      // YSXFixupKinds.h.
      //
      // name                      offset bits  flags
      {"fixup_ysx_hi20", 12, 20, 0},
      {"fixup_ysx_lo12_i", 20, 12, 0},
      {"fixup_ysx_12_i", 20, 12, 0},
      {"fixup_ysx_lo12_s", 0, 32, 0},
      {"fixup_ysx_pcrel_hi20", 12, 20, 0},
      {"fixup_ysx_pcrel_lo12_i", 20, 12, 0},
      {"fixup_ysx_pcrel_lo12_s", 0, 32, 0},
      {"fixup_ysx_jal", 12, 20, 0},
      {"fixup_ysx_branch", 0, 32, 0},
      {"fixup_ysx_call", 0, 64, 0},
      {"fixup_ysx_call_plt", 0, 64, 0},
  };
  static_assert((std::size(Infos)) == YSX::NumTargetFixupKinds,
                "Not all fixup kinds added to Infos array");

  // Fixup kinds from raw relocation types and .reloc directives force
  // relocations and do not use these fields.
  if (mc::isRelocation(Kind))
    return {};

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);

  assert(unsigned(Kind - FirstTargetFixupKind) < YSX::NumTargetFixupKinds &&
         "Invalid kind!");
  return Infos[Kind - FirstTargetFixupKind];
}

bool YSXAsmBackend::fixupNeedsRelaxationAdvanced(const MCFragment &,
                                                   const MCFixup &Fixup,
                                                   const MCValue &,
                                                   uint64_t Value,
                                                   bool Resolved) const {
  int64_t Offset = int64_t(Value);
  auto Kind = Fixup.getKind();

  // Return true if the symbol is unresolved.
  if (!Resolved)
    return true;

  switch (Kind) {
  default:
    return false;
  case YSX::fixup_ysx_branch:
    // For conditional branch instructions the immediate must be
    // in the range [-4096, 4094].
    return Offset > 4094 || Offset < -4096;
  case YSX::fixup_ysx_jal:
    // For jump instructions the immediate must be in the range
    // [-1048576, 1048574]
    return Offset > 1048574 || Offset < -1048576;
  }
}

// Return the expanded long-branch opcode, or the original opcode if no
// expansion is available.
static unsigned getRelaxedOpcode(unsigned Opcode, ArrayRef<MCOperand> Operands,
                                 const MCSubtargetInfo &STI) {
  switch (Opcode) {
  case YSX::BEQ:
    return YSX::PseudoLongBEQ;
  case YSX::BNE:
    return YSX::PseudoLongBNE;
  case YSX::BLT:
    return YSX::PseudoLongBLT;
  case YSX::BGE:
    return YSX::PseudoLongBGE;
  case YSX::BLTU:
    return YSX::PseudoLongBLTU;
  case YSX::BGEU:
    return YSX::PseudoLongBGEU;
  }

  // Returning the original opcode means we cannot relax the instruction.
  return Opcode;
}

void YSXAsmBackend::relaxInstruction(MCInst &Inst,
                                       const MCSubtargetInfo &STI) const {
  if (STI.hasFeature(YSX::FeatureExactAssembly))
    return;

  MCInst Res;
  switch (Inst.getOpcode()) {
  default:
    llvm_unreachable("Opcode not expected!");
  case YSX::BEQ:
  case YSX::BNE:
  case YSX::BLT:
  case YSX::BGE:
  case YSX::BLTU:
  case YSX::BGEU:
    Res.setOpcode(getRelaxedOpcode(Inst.getOpcode(), Inst.getOperands(), STI));
    Res.addOperand(Inst.getOperand(0));
    Res.addOperand(Inst.getOperand(1));
    Res.addOperand(Inst.getOperand(2));
    break;
  }
  Inst = std::move(Res);
}

// Check if an R_RISCV_ALIGN relocation is needed for an alignment directive.
// If conditions are met, compute the padding size and create a fixup encoding
// the padding size in the addend.
bool YSXAsmBackend::relaxAlign(MCFragment &F, unsigned &Size) {
  // Alignments before the first linker-relaxable instruction have fixed sizes
  // and do not require relocations. Alignments after a linker-relaxable
  // instruction require a relocation, even if the STI specifies norelax.
  //
  // firstLinkerRelaxable is the layout order within the subsection, which may
  // be smaller than the section's order. Therefore, alignments in a
  // lower-numbered subsection may be unnecessarily treated as linker-relaxable.
  auto *Sec = F.getParent();
  if (F.getLayoutOrder() <= Sec->firstLinkerRelaxable())
    return false;

  // Use default handling unless the alignment is larger than the nop size.
  unsigned MinNopLen = 4;
  if (F.getAlignment() <= MinNopLen)
    return false;

  Size = F.getAlignment().value() - MinNopLen;
  auto *Expr = MCConstantExpr::create(Size, getContext());
  MCFixup Fixup =
      MCFixup::create(0, Expr, FirstLiteralRelocationKind + ELF::R_RISCV_ALIGN);
  F.setVarFixups({Fixup});
  F.setLinkerRelaxable();
  return true;
}

bool YSXAsmBackend::relaxDwarfLineAddr(MCFragment &F) const {
  int64_t LineDelta = F.getDwarfLineDelta();
  const MCExpr &AddrDelta = F.getDwarfAddrDelta();
  int64_t Value;
  // If the label difference can be resolved, use the default handling, which
  // utilizes a shorter special opcode.
  if (AddrDelta.evaluateAsAbsolute(Value, *Asm))
    return false;
  [[maybe_unused]] bool IsAbsolute =
      AddrDelta.evaluateKnownAbsolute(Value, *Asm);
  assert(IsAbsolute && "CFA with invalid expression");

  SmallVector<char> Data;
  raw_svector_ostream OS(Data);

  // INT64_MAX is a signal that this is actually a DW_LNE_end_sequence.
  if (LineDelta != INT64_MAX) {
    OS << uint8_t(dwarf::DW_LNS_advance_line);
    encodeSLEB128(LineDelta, OS);
  }

  // According to the DWARF specification, the `DW_LNS_fixed_advance_pc` opcode
  // takes a single unsigned half (unencoded) operand. The maximum encodable
  // value is therefore 65535.  Set a conservative upper bound for relaxation.
  unsigned PCBytes;
  if (Value > 60000) {
    PCBytes = getContext().getAsmInfo()->getCodePointerSize();
    OS << uint8_t(dwarf::DW_LNS_extended_op) << uint8_t(PCBytes + 1)
       << uint8_t(dwarf::DW_LNE_set_address);
    OS.write_zeros(PCBytes);
  } else {
    PCBytes = 2;
    OS << uint8_t(dwarf::DW_LNS_fixed_advance_pc);
    support::endian::write<uint16_t>(OS, 0, Endian);
  }
  auto Offset = OS.tell() - PCBytes;

  if (LineDelta == INT64_MAX) {
    OS << uint8_t(dwarf::DW_LNS_extended_op);
    OS << uint8_t(1);
    OS << uint8_t(dwarf::DW_LNE_end_sequence);
  } else {
    OS << uint8_t(dwarf::DW_LNS_copy);
  }

  F.setVarContents(Data);
  F.setVarFixups({MCFixup::create(Offset, &AddrDelta,
                                  MCFixup::getDataKindForSize(PCBytes))});
  return true;
}

bool YSXAsmBackend::relaxDwarfCFA(MCFragment &F) const {
  const MCExpr &AddrDelta = F.getDwarfAddrDelta();
  SmallVector<MCFixup, 2> Fixups;
  int64_t Value;
  if (AddrDelta.evaluateAsAbsolute(Value, *Asm))
    return false;
  [[maybe_unused]] bool IsAbsolute =
      AddrDelta.evaluateKnownAbsolute(Value, *Asm);
  assert(IsAbsolute && "CFA with invalid expression");

  assert(getContext().getAsmInfo()->getMinInstAlignment() == 1 &&
         "expected 1-byte alignment");
  if (Value == 0) {
    F.clearVarContents();
    F.clearVarFixups();
    return true;
  }

  auto AddFixups = [&Fixups, &AddrDelta](unsigned Offset,
                                         std::pair<unsigned, unsigned> Fixup) {
    const MCBinaryExpr &MBE = cast<MCBinaryExpr>(AddrDelta);
    Fixups.push_back(MCFixup::create(Offset, MBE.getLHS(), std::get<0>(Fixup)));
    Fixups.push_back(MCFixup::create(Offset, MBE.getRHS(), std::get<1>(Fixup)));
  };

  SmallVector<char, 8> Data;
  raw_svector_ostream OS(Data);
  if (isUIntN(6, Value)) {
    OS << uint8_t(dwarf::DW_CFA_advance_loc);
    AddFixups(0, {ELF::R_RISCV_SET6, ELF::R_RISCV_SUB6});
  } else if (isUInt<8>(Value)) {
    OS << uint8_t(dwarf::DW_CFA_advance_loc1);
    support::endian::write<uint8_t>(OS, 0, Endian);
    AddFixups(1, {ELF::R_RISCV_SET8, ELF::R_RISCV_SUB8});
  } else if (isUInt<16>(Value)) {
    OS << uint8_t(dwarf::DW_CFA_advance_loc2);
    support::endian::write<uint16_t>(OS, 0, Endian);
    AddFixups(1, {ELF::R_RISCV_SET16, ELF::R_RISCV_SUB16});
  } else if (isUInt<32>(Value)) {
    OS << uint8_t(dwarf::DW_CFA_advance_loc4);
    support::endian::write<uint32_t>(OS, 0, Endian);
    AddFixups(1, {ELF::R_RISCV_SET32, ELF::R_RISCV_SUB32});
  } else {
    llvm_unreachable("unsupported CFA encoding");
  }
  F.setVarContents(Data);
  F.setVarFixups(Fixups);
  return true;
}

std::pair<bool, bool> YSXAsmBackend::relaxLEB128(MCFragment &LF,
                                                   int64_t &Value) const {
  if (LF.isLEBSigned())
    return std::make_pair(false, false);
  const MCExpr &Expr = LF.getLEBValue();
  if (ULEB128Reloc) {
    LF.setVarFixups({MCFixup::create(0, &Expr, FK_Data_leb128)});
  }
  return std::make_pair(Expr.evaluateKnownAbsolute(Value, *Asm), false);
}

bool YSXAsmBackend::mayNeedRelaxation(unsigned Opcode,
                                        ArrayRef<MCOperand> Operands,
                                        const MCSubtargetInfo &STI) const {
  // This function has access to two STIs, the member of the AsmBackend, and the
  // one passed as an argument. The latter is more specific, so we query it for
  // specific features.
  if (STI.hasFeature(YSX::FeatureExactAssembly))
    return false;

  return getRelaxedOpcode(Opcode, Operands, STI) != Opcode;
}

bool YSXAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                   const MCSubtargetInfo *STI) const {
  // Align to an even boundary with zero padding and use 4-byte scalar nops for
  // the remaining padding.

  // Instructions always are at even addresses.  We must be in a data area or
  // be unaligned due to some other reason.
  if (Count % 2) {
    OS.write("\0", 1);
    Count -= 1;
  }

  // TODO: emit a mapping symbol right here

  if (Count % 4 == 2) {
    OS.write("\0\0", 2);
    Count -= 2;
  }

  // The canonical nop on RISC-V is addi x0, x0, 0.
  for (; Count >= 4; Count -= 4)
    OS.write("\x13\0\0\0", 4);

  return true;
}

static uint64_t adjustFixupValue(const MCFixup &Fixup, uint64_t Value,
                                 MCContext &Ctx) {
  switch (Fixup.getKind()) {
  default:
    llvm_unreachable("Unknown fixup kind!");
  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
  case FK_Data_leb128:
    return Value;
  case YSX::fixup_ysx_lo12_i:
  case YSX::fixup_ysx_pcrel_lo12_i:
    return Value & 0xfff;
  case YSX::fixup_ysx_12_i:
    if (!isInt<12>(Value)) {
      Ctx.reportError(Fixup.getLoc(),
                      "operand must be a constant 12-bit integer");
    }
    return Value & 0xfff;
  case YSX::fixup_ysx_lo12_s:
  case YSX::fixup_ysx_pcrel_lo12_s:
    return (((Value >> 5) & 0x7f) << 25) | ((Value & 0x1f) << 7);
  case YSX::fixup_ysx_hi20:
  case YSX::fixup_ysx_pcrel_hi20:
    // Add 1 if bit 11 is 1, to compensate for low 12 bits being negative.
    return ((Value + 0x800) >> 12) & 0xfffff;
  case YSX::fixup_ysx_jal: {
    if (!isInt<21>(Value))
      Ctx.reportError(Fixup.getLoc(), "fixup value out of range");
    if (Value & 0x1)
      Ctx.reportError(Fixup.getLoc(), "fixup value must be 2-byte aligned");
    // Need to produce imm[19|10:1|11|19:12] from the 21-bit Value.
    unsigned Sbit = (Value >> 20) & 0x1;
    unsigned Hi8 = (Value >> 12) & 0xff;
    unsigned Mid1 = (Value >> 11) & 0x1;
    unsigned Lo10 = (Value >> 1) & 0x3ff;
    // Inst{31} = Sbit;
    // Inst{30-21} = Lo10;
    // Inst{20} = Mid1;
    // Inst{19-12} = Hi8;
    Value = (Sbit << 19) | (Lo10 << 9) | (Mid1 << 8) | Hi8;
    return Value;
  }
  case YSX::fixup_ysx_branch: {
    if (!isInt<13>(Value))
      Ctx.reportError(Fixup.getLoc(), "fixup value out of range");
    if (Value & 0x1)
      Ctx.reportError(Fixup.getLoc(), "fixup value must be 2-byte aligned");
    // Need to extract imm[12], imm[10:5], imm[4:1], imm[11] from the 13-bit
    // Value.
    unsigned Sbit = (Value >> 12) & 0x1;
    unsigned Hi1 = (Value >> 11) & 0x1;
    unsigned Mid6 = (Value >> 5) & 0x3f;
    unsigned Lo4 = (Value >> 1) & 0xf;
    // Inst{31} = Sbit;
    // Inst{30-25} = Mid6;
    // Inst{11-8} = Lo4;
    // Inst{7} = Hi1;
    Value = (Sbit << 31) | (Mid6 << 25) | (Lo4 << 8) | (Hi1 << 7);
    return Value;
  }
  case YSX::fixup_ysx_call:
  case YSX::fixup_ysx_call_plt: {
    // Jalr will add UpperImm with the sign-extended 12-bit LowerImm,
    // we need to add 0x800ULL before extract upper bits to reflect the
    // effect of the sign extension.
    uint64_t UpperImm = (Value + 0x800ULL) & 0xfffff000ULL;
    uint64_t LowerImm = Value & 0xfffULL;
    return UpperImm | ((LowerImm << 20) << 32);
  }
  }
}

bool YSXAsmBackend::isPCRelFixupResolved(const MCSymbol *SymA,
                                           const MCFragment &F) {
  // If the section does not contain linker-relaxable fragments, PC-relative
  // fixups can be resolved.
  if (!F.getParent()->isLinkerRelaxable())
    return true;

  // Otherwise, check if the offset between the symbol and fragment is fully
  // resolved, unaffected by linker-relaxable fragments (e.g. instructions or
  // offset-affected FT_Align fragments). Complements the generic
  // isSymbolRefDifferenceFullyResolvedImpl.
  if (!PCRelTemp)
    PCRelTemp = getContext().createTempSymbol();
  PCRelTemp->setFragment(const_cast<MCFragment *>(&F));
  MCValue Res;
  MCExpr::evaluateSymbolicAdd(Asm, false, MCValue::get(SymA),
                              MCValue::get(nullptr, PCRelTemp), Res);
  return !Res.getSubSym();
}

// Get the corresponding PC-relative HI fixup that a S_PCREL_LO points to, and
// optionally the fragment containing it.
//
// \returns nullptr if this isn't a S_PCREL_LO pointing to a known PC-relative
// HI fixup.
static const MCFixup *getPCRelHiFixup(const MCSpecifierExpr &Expr,
                                      const MCFragment **DFOut) {
  MCValue AUIPCLoc;
  if (!Expr.getSubExpr()->evaluateAsRelocatable(AUIPCLoc, nullptr))
    return nullptr;

  const MCSymbol *AUIPCSymbol = AUIPCLoc.getAddSym();
  if (!AUIPCSymbol)
    return nullptr;
  const auto *DF = AUIPCSymbol->getFragment();
  if (!DF)
    return nullptr;

  uint64_t Offset = AUIPCSymbol->getOffset();
  if (DF->getContents().size() == Offset) {
    DF = DF->getNext();
    if (!DF)
      return nullptr;
    Offset = 0;
  }

  for (const MCFixup &F : DF->getFixups()) {
    if (F.getOffset() != Offset)
      continue;
    auto Kind = F.getKind();
    if (!mc::isRelocation(F.getKind())) {
      if (Kind == YSX::fixup_ysx_pcrel_hi20) {
        *DFOut = DF;
        return &F;
      }
      break;
    }
    switch (Kind) {
    case ELF::R_RISCV_GOT_HI20:
    case ELF::R_RISCV_TLS_GOT_HI20:
    case ELF::R_RISCV_TLS_GD_HI20:
    case ELF::R_RISCV_TLSDESC_HI20:
      *DFOut = DF;
      return &F;
    }
  }

  return nullptr;
}

std::optional<bool> YSXAsmBackend::evaluateFixup(const MCFragment &,
                                                   MCFixup &Fixup,
                                                   MCValue &Target,
                                                   uint64_t &Value) {
  const MCFixup *AUIPCFixup;
  const MCFragment *AUIPCDF;
  MCValue AUIPCTarget;
  switch (Fixup.getKind()) {
  default:
    // Use default handling for `Value` and `IsResolved`.
    return {};
  case YSX::fixup_ysx_pcrel_lo12_i:
  case YSX::fixup_ysx_pcrel_lo12_s: {
    AUIPCFixup =
        getPCRelHiFixup(cast<MCSpecifierExpr>(*Fixup.getValue()), &AUIPCDF);
    if (!AUIPCFixup) {
      getContext().reportError(Fixup.getLoc(),
                               "could not find corresponding %pcrel_hi");
      return true;
    }

    // MCAssembler::evaluateFixup will emit an error for this case when it sees
    // the %pcrel_hi, so don't duplicate it when also seeing the %pcrel_lo.
    const MCExpr *AUIPCExpr = AUIPCFixup->getValue();
    if (!AUIPCExpr->evaluateAsRelocatable(AUIPCTarget, Asm))
      return true;
    break;
  }
  }

  if (!AUIPCTarget.getAddSym())
    return false;

  auto &SA = static_cast<const MCSymbolELF &>(*AUIPCTarget.getAddSym());
  if (SA.isUndefined())
    return false;

  bool IsResolved = &SA.getSection() == AUIPCDF->getParent() &&
                    SA.getBinding() == ELF::STB_LOCAL &&
                    SA.getType() != ELF::STT_GNU_IFUNC;
  if (!IsResolved)
    return false;

  Value = Asm->getSymbolOffset(SA) + AUIPCTarget.getConstant();
  Value -= Asm->getFragmentOffset(*AUIPCDF) + AUIPCFixup->getOffset();

  return AUIPCFixup->getKind() == YSX::fixup_ysx_pcrel_hi20 &&
         isPCRelFixupResolved(AUIPCTarget.getAddSym(), *AUIPCDF);
}

static bool relaxableFixupNeedsRelocation(const MCFixupKind Kind) {
  // Some Fixups are marked as LinkerRelaxable by
  // `YSXMCCodeEmitter::getImmOpValue` only because they may be
  // (assembly-)relaxed into a linker-relaxable instruction. This function
  // should return `false` for those fixups so they do not get a `R_RISCV_RELAX`
  // relocation emitted in addition to the relocation.
  switch (Kind) {
  default:
    break;
  case YSX::fixup_ysx_branch:
    return false;
  }
  return true;
}

bool YSXAsmBackend::addReloc(const MCFragment &F, const MCFixup &Fixup,
                               const MCValue &Target, uint64_t &FixedValue,
                               bool IsResolved) {
  uint64_t FixedValueA, FixedValueB;
  if (Target.getSubSym()) {
    assert(Target.getSpecifier() == 0 &&
           "relocatable SymA-SymB cannot have relocation specifier");
    unsigned TA = 0, TB = 0;
    switch (Fixup.getKind()) {
    case llvm::FK_Data_1:
      TA = ELF::R_RISCV_ADD8;
      TB = ELF::R_RISCV_SUB8;
      break;
    case llvm::FK_Data_2:
      TA = ELF::R_RISCV_ADD16;
      TB = ELF::R_RISCV_SUB16;
      break;
    case llvm::FK_Data_4:
      TA = ELF::R_RISCV_ADD32;
      TB = ELF::R_RISCV_SUB32;
      break;
    case llvm::FK_Data_8:
      TA = ELF::R_RISCV_ADD64;
      TB = ELF::R_RISCV_SUB64;
      break;
    case llvm::FK_Data_leb128:
      TA = ELF::R_RISCV_SET_ULEB128;
      TB = ELF::R_RISCV_SUB_ULEB128;
      break;
    default:
      llvm_unreachable("unsupported fixup size");
    }
    MCValue A = MCValue::get(Target.getAddSym(), nullptr, Target.getConstant());
    MCValue B = MCValue::get(Target.getSubSym());
    auto FA = MCFixup::create(Fixup.getOffset(), nullptr, TA);
    auto FB = MCFixup::create(Fixup.getOffset(), nullptr, TB);
    Asm->getWriter().recordRelocation(F, FA, A, FixedValueA);
    Asm->getWriter().recordRelocation(F, FB, B, FixedValueB);
    FixedValue = FixedValueA - FixedValueB;
    return false;
  }

  // If linker relaxation is enabled and supported by the current fixup, then we
  // always want to generate a relocation.
  bool NeedsRelax = Fixup.isLinkerRelaxable() &&
                    relaxableFixupNeedsRelocation(Fixup.getKind());
  if (NeedsRelax)
    IsResolved = false;

  if (IsResolved && Fixup.isPCRel())
    IsResolved = isPCRelFixupResolved(Target.getAddSym(), F);

  if (!IsResolved) {
    Asm->getWriter().recordRelocation(F, Fixup, Target, FixedValue);

    if (NeedsRelax) {
      // Some Fixups get a RELAX relocation, record it (directly) after we add
      // the relocation.
      MCFixup RelaxFixup =
          MCFixup::create(Fixup.getOffset(), nullptr, ELF::R_RISCV_RELAX);
      MCValue RelaxTarget = MCValue::get(nullptr);
      uint64_t RelaxValue;
      Asm->getWriter().recordRelocation(F, RelaxFixup, RelaxTarget, RelaxValue);
    }
  }

  return false;
}

// Data fixups should be swapped for big endian cores.
// Instruction fixups should not be swapped as RISC-V instructions
// are always little-endian.
static bool isDataFixup(unsigned Kind) {
  switch (Kind) {
  default:
    return false;

  case FK_Data_1:
  case FK_Data_2:
  case FK_Data_4:
  case FK_Data_8:
    return true;
  }
}

void YSXAsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                 const MCValue &Target, uint8_t *Data,
                                 uint64_t Value, bool IsResolved) {
  IsResolved = addReloc(F, Fixup, Target, Value, IsResolved);
  MCFixupKind Kind = Fixup.getKind();
  if (mc::isRelocation(Kind))
    return;
  MCContext &Ctx = getContext();
  MCFixupKindInfo Info = getFixupKindInfo(Kind);
  if (!Value)
    return; // Doesn't change encoding.
  // Apply any target-specific value adjustments.
  Value = adjustFixupValue(Fixup, Value, Ctx);

  // Shift the value into position.
  Value <<= Info.TargetOffset;

  unsigned NumBytes = alignTo(Info.TargetSize + Info.TargetOffset, 8) / 8;
  assert(Fixup.getOffset() + NumBytes <= F.getSize() &&
         "Invalid fixup offset!");

  // For each byte of the fragment that the fixup touches, mask in the
  // bits from the fixup value.
  // For big endian cores, data fixup should be swapped.
  bool SwapValue = Endian == llvm::endianness::big && isDataFixup(Kind);
  for (unsigned i = 0; i != NumBytes; ++i) {
    unsigned Idx = SwapValue ? (NumBytes - 1 - i) : i;
    Data[Idx] |= uint8_t((Value >> (i * 8)) & 0xff);
  }
}

std::unique_ptr<MCObjectTargetWriter>
YSXAsmBackend::createObjectTargetWriter() const {
  return createYSXELFObjectWriter(OSABI, Is64Bit);
}

class DarwinYSXAsmBackend : public YSXAsmBackend {
public:
  DarwinYSXAsmBackend(const MCSubtargetInfo &STI, uint8_t OSABI, bool Is64Bit,
                        bool IsLittleEndian, const MCTargetOptions &Options)
      : YSXAsmBackend(STI, OSABI, Is64Bit, IsLittleEndian, Options) {}

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override {
    const Triple &TT = STI.getTargetTriple();
    uint32_t CPUType = cantFail(MachO::getCPUType(TT));
    uint32_t CPUSubType = cantFail(MachO::getCPUSubType(TT));
    return createYSXMachObjectWriter(CPUType, CPUSubType);
  }
};

MCAsmBackend *llvm::createYSXAsmBackend(const Target &T,
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo &MRI,
                                          const MCTargetOptions &Options) {
  const Triple &TT = STI.getTargetTriple();
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  if (TT.isOSBinFormatMachO())
    return new DarwinYSXAsmBackend(STI, OSABI, TT.isArch64Bit(),
                                     TT.isLittleEndian(), Options);
  return new YSXAsmBackend(STI, OSABI, TT.isArch64Bit(), TT.isLittleEndian(),
                             Options);
}
