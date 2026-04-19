//===-- YSXMCCodeEmitter.cpp - Convert RISC-V code to machine code ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the YSXMCCodeEmitter class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/YSXBaseInfo.h"
#include "MCTargetDesc/YSXFixupKinds.h"
#include "MCTargetDesc/YSXMCAsmInfo.h"
#include "MCTargetDesc/YSXMCTargetDesc.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/EndianStream.h"

using namespace llvm;

#define DEBUG_TYPE "mccodeemitter"

STATISTIC(MCNumEmitted, "Number of MC instructions emitted");
STATISTIC(MCNumFixups, "Number of MC fixups created");

namespace {
class YSXMCCodeEmitter : public MCCodeEmitter {
  YSXMCCodeEmitter(const YSXMCCodeEmitter &) = delete;
  void operator=(const YSXMCCodeEmitter &) = delete;
  MCContext &Ctx;
  MCInstrInfo const &MCII;

public:
  YSXMCCodeEmitter(MCContext &ctx, MCInstrInfo const &MCII)
      : Ctx(ctx), MCII(MCII) {}

  ~YSXMCCodeEmitter() override = default;

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

  void expandFunctionCall(const MCInst &MI, SmallVectorImpl<char> &CB,
                          SmallVectorImpl<MCFixup> &Fixups,
                          const MCSubtargetInfo &STI) const;

  void expandTLSDESCCall(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

  void expandAddTPRel(const MCInst &MI, SmallVectorImpl<char> &CB,
                      SmallVectorImpl<MCFixup> &Fixups,
                      const MCSubtargetInfo &STI) const;

  void expandLongCondBr(const MCInst &MI, SmallVectorImpl<char> &CB,
                        SmallVectorImpl<MCFixup> &Fixups,
                        const MCSubtargetInfo &STI) const;

  /// TableGen'erated function for getting the binary encoding for an
  /// instruction.
  uint64_t getBinaryCodeForInstr(const MCInst &MI,
                                 SmallVectorImpl<MCFixup> &Fixups,
                                 const MCSubtargetInfo &STI) const;

  /// Return binary encoding of operand. If the machine operand requires
  /// relocation, record the relocation and return zero.
  uint64_t getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  uint64_t getImmOpValueMinus1(const MCInst &MI, unsigned OpNo,
                               SmallVectorImpl<MCFixup> &Fixups,
                               const MCSubtargetInfo &STI) const;

  template <unsigned N>
  unsigned getImmOpValueAsrN(const MCInst &MI, unsigned OpNo,
                             SmallVectorImpl<MCFixup> &Fixups,
                             const MCSubtargetInfo &STI) const;

  uint64_t getImmOpValue(const MCInst &MI, unsigned OpNo,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;

};
} // end anonymous namespace

MCCodeEmitter *llvm::createYSXMCCodeEmitter(const MCInstrInfo &MCII,
                                              MCContext &Ctx) {
  return new YSXMCCodeEmitter(Ctx, MCII);
}

static void addFixup(SmallVectorImpl<MCFixup> &Fixups, uint32_t Offset,
                     const MCExpr *Value, uint16_t Kind) {
  bool PCRel = false;
  switch (Kind) {
  case ELF::R_RISCV_CALL_PLT:
  case YSX::fixup_ysx_pcrel_hi20:
  case YSX::fixup_ysx_pcrel_lo12_i:
  case YSX::fixup_ysx_pcrel_lo12_s:
  case YSX::fixup_ysx_jal:
  case YSX::fixup_ysx_branch:
  case YSX::fixup_ysx_call:
  case YSX::fixup_ysx_call_plt:
    PCRel = true;
  }
  Fixups.push_back(MCFixup::create(Offset, Value, Kind, PCRel));
}

// Expand PseudoCALL(Reg), PseudoTAIL and PseudoJump to AUIPC and JALR with
// relocation types. Linker relaxation can still relax the AUIPC/JALR pair to
// JAL when relaxation is enabled.
void YSXMCCodeEmitter::expandFunctionCall(const MCInst &MI,
                                            SmallVectorImpl<char> &CB,
                                            SmallVectorImpl<MCFixup> &Fixups,
                                            const MCSubtargetInfo &STI) const {
  MCInst TmpInst;
  MCOperand Func;
  MCRegister Ra;
  if (MI.getOpcode() == YSX::PseudoTAIL) {
    Func = MI.getOperand(0);
    Ra = YSXII::getTailExpandUseRegNo(STI.getFeatureBits());
  } else if (MI.getOpcode() == YSX::PseudoCALLReg) {
    Func = MI.getOperand(1);
    Ra = MI.getOperand(0).getReg();
  } else if (MI.getOpcode() == YSX::PseudoCALL) {
    Func = MI.getOperand(0);
    Ra = YSX::X1;
  } else if (MI.getOpcode() == YSX::PseudoJump) {
    Func = MI.getOperand(1);
    Ra = MI.getOperand(0).getReg();
  }
  uint32_t Binary;

  assert(Func.isExpr() && "Expected expression");

  const MCExpr *CallExpr = Func.getExpr();

  // Emit AUIPC Ra, Func with R_RISCV_CALL relocation type.
  TmpInst = MCInstBuilder(YSX::AUIPC).addReg(Ra).addExpr(CallExpr);
  Binary = getBinaryCodeForInstr(TmpInst, Fixups, STI);
  support::endian::write(CB, Binary, llvm::endianness::little);

  if (MI.getOpcode() == YSX::PseudoTAIL ||
      MI.getOpcode() == YSX::PseudoJump)
    // Emit JALR X0, Ra, 0
    TmpInst = MCInstBuilder(YSX::JALR).addReg(YSX::X0).addReg(Ra).addImm(0);
  else
    // Emit JALR Ra, Ra, 0
    TmpInst = MCInstBuilder(YSX::JALR).addReg(Ra).addReg(Ra).addImm(0);
  Binary = getBinaryCodeForInstr(TmpInst, Fixups, STI);
  support::endian::write(CB, Binary, llvm::endianness::little);
}

void YSXMCCodeEmitter::expandTLSDESCCall(const MCInst &MI,
                                           SmallVectorImpl<char> &CB,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  MCOperand SrcSymbol = MI.getOperand(3);
  assert(SrcSymbol.isExpr() &&
         "Expected expression as first input to TLSDESCCALL");
  const auto *Expr = dyn_cast<MCSpecifierExpr>(SrcSymbol.getExpr());
  MCRegister Link = MI.getOperand(0).getReg();
  MCRegister Dest = MI.getOperand(1).getReg();
  int64_t Imm = MI.getOperand(2).getImm();
  addFixup(Fixups, 0, Expr, ELF::R_RISCV_TLSDESC_CALL);
  MCInst Call =
      MCInstBuilder(YSX::JALR).addReg(Link).addReg(Dest).addImm(Imm);

  uint32_t Binary = getBinaryCodeForInstr(Call, Fixups, STI);
  support::endian::write(CB, Binary, llvm::endianness::little);
}

// Expand PseudoAddTPRel to a simple ADD with the correct relocation.
void YSXMCCodeEmitter::expandAddTPRel(const MCInst &MI,
                                        SmallVectorImpl<char> &CB,
                                        SmallVectorImpl<MCFixup> &Fixups,
                                        const MCSubtargetInfo &STI) const {
  MCOperand DestReg = MI.getOperand(0);
  MCOperand SrcReg = MI.getOperand(1);
  MCOperand TPReg = MI.getOperand(2);
  assert(TPReg.isReg() && TPReg.getReg() == YSX::X4 &&
         "Expected thread pointer as second input to TP-relative add");

  MCOperand SrcSymbol = MI.getOperand(3);
  assert(SrcSymbol.isExpr() &&
         "Expected expression as third input to TP-relative add");

  const auto *Expr = dyn_cast<MCSpecifierExpr>(SrcSymbol.getExpr());
  assert(Expr && Expr->getSpecifier() == ELF::R_RISCV_TPREL_ADD &&
         "Expected tprel_add relocation on TP-relative symbol");

  addFixup(Fixups, 0, Expr, ELF::R_RISCV_TPREL_ADD);
  if (STI.hasFeature(YSX::FeatureRelax))
    Fixups.back().setLinkerRelaxable();

  // Emit a normal ADD instruction with the given operands.
  MCInst TmpInst = MCInstBuilder(YSX::ADD)
                       .addOperand(DestReg)
                       .addOperand(SrcReg)
                       .addOperand(TPReg);
  uint32_t Binary = getBinaryCodeForInstr(TmpInst, Fixups, STI);
  support::endian::write(CB, Binary, llvm::endianness::little);
}

static unsigned getInvertedBranchOp(unsigned BrOp) {
  switch (BrOp) {
  default:
    llvm_unreachable("Unexpected branch opcode!");
  case YSX::PseudoLongBEQ:
    return YSX::BNE;
  case YSX::PseudoLongBNE:
    return YSX::BEQ;
  case YSX::PseudoLongBLT:
    return YSX::BGE;
  case YSX::PseudoLongBGE:
    return YSX::BLT;
  case YSX::PseudoLongBLTU:
    return YSX::BGEU;
  case YSX::PseudoLongBGEU:
    return YSX::BLTU;
  }
}

// Expand PseudoLongBxx to an inverted conditional branch and an unconditional
// jump.
void YSXMCCodeEmitter::expandLongCondBr(const MCInst &MI,
                                          SmallVectorImpl<char> &CB,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  MCRegister SrcReg1 = MI.getOperand(0).getReg();
  const MCOperand &Src2 = MI.getOperand(1);
  const MCOperand &SrcSymbol = MI.getOperand(2);
  unsigned Opcode = MI.getOpcode();
  unsigned InvOpc = getInvertedBranchOp(Opcode);
  MCInst TmpBr =
      MCInstBuilder(InvOpc).addReg(SrcReg1).addOperand(Src2).addImm(8);
  uint32_t BrBinary = getBinaryCodeForInstr(TmpBr, Fixups, STI);
  support::endian::write(CB, BrBinary, llvm::endianness::little);
  uint32_t Offset = 4;

  // Save the number fixups.
  size_t FixupStartIndex = Fixups.size();

  // Emit an unconditional jump to the destination.
  MCInst TmpInst =
      MCInstBuilder(YSX::JAL).addReg(YSX::X0).addOperand(SrcSymbol);
  uint32_t Binary = getBinaryCodeForInstr(TmpInst, Fixups, STI);
  support::endian::write(CB, Binary, llvm::endianness::little);

  // Drop any fixup added so we can add the correct one.
  Fixups.resize(FixupStartIndex);

  if (SrcSymbol.isExpr()) {
    addFixup(Fixups, Offset, SrcSymbol.getExpr(), YSX::fixup_ysx_jal);
    if (STI.hasFeature(YSX::FeatureRelax))
      Fixups.back().setLinkerRelaxable();
  }
}

void YSXMCCodeEmitter::encodeInstruction(const MCInst &MI,
                                           SmallVectorImpl<char> &CB,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  const MCInstrDesc &Desc = MCII.get(MI.getOpcode());
  // Get byte count of instruction.
  unsigned Size = Desc.getSize();

  // YSXInstrInfo::getInstSizeInBytes expects that the total size of the
  // expanded instructions for each pseudo is correct in the Size field of the
  // tablegen definition for the pseudo.
  switch (MI.getOpcode()) {
  default:
    break;
  case YSX::PseudoCALLReg:
  case YSX::PseudoCALL:
  case YSX::PseudoTAIL:
  case YSX::PseudoJump:
    expandFunctionCall(MI, CB, Fixups, STI);
    MCNumEmitted += 2;
    return;
  case YSX::PseudoAddTPRel:
    expandAddTPRel(MI, CB, Fixups, STI);
    MCNumEmitted += 1;
    return;
  case YSX::PseudoLongBEQ:
  case YSX::PseudoLongBNE:
  case YSX::PseudoLongBLT:
  case YSX::PseudoLongBGE:
  case YSX::PseudoLongBLTU:
  case YSX::PseudoLongBGEU:
    expandLongCondBr(MI, CB, Fixups, STI);
    MCNumEmitted += 2;
    return;
  case YSX::PseudoTLSDESCCall:
    expandTLSDESCCall(MI, CB, Fixups, STI);
    MCNumEmitted += 1;
    return;
  }

  switch (Size) {
  default:
    llvm_unreachable("Unhandled encodeInstruction length!");
  case 2: {
    uint16_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
    support::endian::write<uint16_t>(CB, Bits, llvm::endianness::little);
    break;
  }
  case 4: {
    uint32_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
    support::endian::write(CB, Bits, llvm::endianness::little);
    break;
  }
  case 6: {
    uint64_t Bits = getBinaryCodeForInstr(MI, Fixups, STI) & 0xffff'ffff'ffffu;
    SmallVector<char, 8> Encoding;
    support::endian::write(Encoding, Bits, llvm::endianness::little);
    assert(Encoding[6] == 0 && Encoding[7] == 0 &&
           "Unexpected encoding for 48-bit instruction");
    Encoding.truncate(6);
    CB.append(Encoding);
    break;
  }
  case 8: {
    uint64_t Bits = getBinaryCodeForInstr(MI, Fixups, STI);
    support::endian::write(CB, Bits, llvm::endianness::little);
    break;
  }
  }

  ++MCNumEmitted; // Keep track of the # of mi's emitted.
}

uint64_t
YSXMCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                                      SmallVectorImpl<MCFixup> &Fixups,
                                      const MCSubtargetInfo &STI) const {

  if (MO.isReg())
    return Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());

  if (MO.isImm())
    return MO.getImm();

  llvm_unreachable("Unhandled expression!");
  return 0;
}

uint64_t
YSXMCCodeEmitter::getImmOpValueMinus1(const MCInst &MI, unsigned OpNo,
                                        SmallVectorImpl<MCFixup> &Fixups,
                                        const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);

  if (MO.isImm()) {
    uint64_t Res = MO.getImm();
    return (Res - 1);
  }

  llvm_unreachable("Unhandled expression!");
  return 0;
}

template <unsigned N>
unsigned
YSXMCCodeEmitter::getImmOpValueAsrN(const MCInst &MI, unsigned OpNo,
                                      SmallVectorImpl<MCFixup> &Fixups,
                                      const MCSubtargetInfo &STI) const {
  const MCOperand &MO = MI.getOperand(OpNo);

  if (MO.isImm()) {
    uint64_t Res = MO.getImm();
    assert((Res & ((1 << N) - 1)) == 0 && "LSB is non-zero");
    return Res >> N;
  }

  return getImmOpValue(MI, OpNo, Fixups, STI);
}

uint64_t YSXMCCodeEmitter::getImmOpValue(const MCInst &MI, unsigned OpNo,
                                           SmallVectorImpl<MCFixup> &Fixups,
                                           const MCSubtargetInfo &STI) const {
  bool EnableRelax = STI.hasFeature(YSX::FeatureRelax);
  const MCOperand &MO = MI.getOperand(OpNo);

  MCInstrDesc const &Desc = MCII.get(MI.getOpcode());
  unsigned MIFrm = YSXII::getFormat(Desc.TSFlags);

  // If the destination is an immediate, there is nothing to do.
  if (MO.isImm())
    return MO.getImm();

  assert(MO.isExpr() &&
         "getImmOpValue expects only expressions or immediates");
  const MCExpr *Expr = MO.getExpr();
  MCExpr::ExprKind Kind = Expr->getKind();

  // `RelaxCandidate` must be set to `true` in two cases:
  // - The fixup's relocation gets a R_RISCV_RELAX relocation
  // - The underlying instruction may be relaxed to an instruction that gets a
  //   `R_RISCV_RELAX` relocation.
  //
  // The actual emission of `R_RISCV_RELAX` will be handled in
  // `YSXAsmBackend::applyFixup`.
  bool RelaxCandidate = false;
  auto AsmRelaxToLinkerRelaxable = [&]() -> void {
    if (!STI.hasFeature(YSX::FeatureExactAssembly))
      RelaxCandidate = true;
  };

  unsigned FixupKind = YSX::fixup_ysx_invalid;
  if (Kind == MCExpr::Specifier) {
    const auto *RVExpr = cast<MCSpecifierExpr>(Expr);
    FixupKind = RVExpr->getSpecifier();
    switch (RVExpr->getSpecifier()) {
    default:
      assert(FixupKind && FixupKind < FirstTargetFixupKind &&
             "invalid specifier");
      break;
    case ELF::R_RISCV_TPREL_ADD:
      // tprel_add is only used to indicate that a relocation should be emitted
      // for an add instruction used in TP-relative addressing. It should not be
      // expanded as if representing an actual instruction operand and so to
      // encounter it here is an error.
      llvm_unreachable(
          "ELF::R_RISCV_TPREL_ADD should not represent an instruction operand");
    case YSX::S_LO:
      if (MIFrm == YSXII::InstFormatI)
        FixupKind = YSX::fixup_ysx_lo12_i;
      else if (MIFrm == YSXII::InstFormatS)
        FixupKind = YSX::fixup_ysx_lo12_s;
      else
        llvm_unreachable("VK_LO used with unexpected instruction format");
      RelaxCandidate = true;
      break;
    case ELF::R_RISCV_HI20:
      FixupKind = YSX::fixup_ysx_hi20;
      RelaxCandidate = true;
      break;
    case YSX::S_PCREL_LO:
      if (MIFrm == YSXII::InstFormatI)
        FixupKind = YSX::fixup_ysx_pcrel_lo12_i;
      else if (MIFrm == YSXII::InstFormatS)
        FixupKind = YSX::fixup_ysx_pcrel_lo12_s;
      else
        llvm_unreachable("VK_PCREL_LO used with unexpected instruction format");
      RelaxCandidate = true;
      break;
    case ELF::R_RISCV_PCREL_HI20:
      FixupKind = YSX::fixup_ysx_pcrel_hi20;
      RelaxCandidate = true;
      break;
    case YSX::S_TPREL_LO:
      if (MIFrm == YSXII::InstFormatI)
        FixupKind = ELF::R_RISCV_TPREL_LO12_I;
      else if (MIFrm == YSXII::InstFormatS)
        FixupKind = ELF::R_RISCV_TPREL_LO12_S;
      else
        llvm_unreachable("VK_TPREL_LO used with unexpected instruction format");
      RelaxCandidate = true;
      break;
    case ELF::R_RISCV_CALL_PLT:
      FixupKind = YSX::fixup_ysx_call_plt;
      RelaxCandidate = true;
      break;
    case ELF::R_RISCV_GOT_HI20:
    case ELF::R_RISCV_TPREL_HI20:
    case ELF::R_RISCV_TLSDESC_HI20:
      RelaxCandidate = true;
      break;
    }
  } else if (Kind == MCExpr::SymbolRef || Kind == MCExpr::Binary) {
    // FIXME: Sub kind binary exprs have chance of underflow.
    if (MIFrm == YSXII::InstFormatJ) {
      FixupKind = YSX::fixup_ysx_jal;
      RelaxCandidate = true;
    } else if (MIFrm == YSXII::InstFormatB) {
      FixupKind = YSX::fixup_ysx_branch;
      // Relaxes to B<cc>; JAL, with fixup_ysx_jal
      AsmRelaxToLinkerRelaxable();
    } else if (MIFrm == YSXII::InstFormatI) {
      FixupKind = YSX::fixup_ysx_12_i;
    }
  }

  assert(FixupKind != YSX::fixup_ysx_invalid && "Unhandled expression!");

  addFixup(Fixups, 0, Expr, FixupKind);
  // If linker relaxation is enabled and supported by this relocation, set a bit
  // so that the assembler knows the size of the instruction is not fixed/known,
  // and the relocation will need a R_RISCV_RELAX relocation.
  if (EnableRelax && RelaxCandidate)
    Fixups.back().setLinkerRelaxable();
  ++MCNumFixups;

  return 0;
}

#include "YSXGenMCCodeEmitter.inc"
