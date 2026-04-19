//===- YSXMatInt.cpp - Immediate materialisation -------------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "YSXMatInt.h"
#include "MCTargetDesc/YSXMCTargetDesc.h"
#include "llvm/ADT/APInt.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/Support/MathExtras.h"
using namespace llvm;

static int getInstSeqCost(YSXMatInt::InstSeq &Res, bool HasRVC) {
  if (!HasRVC)
    return Res.size();

  int Cost = 0;
  for (auto Instr : Res) {
    // Assume instructions that aren't listed aren't compressible.
    bool Compressed = false;
    switch (Instr.getOpcode()) {
    case YSX::SLLI:
    case YSX::SRLI:
      Compressed = true;
      break;
    case YSX::ADDI:
    case YSX::ADDIW:
    case YSX::LUI:
      Compressed = isInt<6>(Instr.getImm());
      break;
    }
    // Two RVC instructions take the same space as one RVI instruction, but
    // can take longer to execute than the single RVI instruction. Thus, we
    // consider that two RVC instruction are slightly more costly than one
    // RVI instruction. For longer sequences of RVC instructions the space
    // savings can be worth it, though. The costs below try to model that.
    if (!Compressed)
      Cost += 100; // Baseline cost of one RVI instruction: 100%.
    else
      Cost += 70; // 70% cost of baseline.
  }
  return Cost;
}

// Recursively generate a sequence for materializing an integer.
static void generateInstSeqImpl(int64_t Val, const MCSubtargetInfo &STI,
                                YSXMatInt::InstSeq &Res) {
  bool IsRV64 = STI.hasFeature(YSX::Feature64Bit);

  if (isInt<32>(Val)) {
    // Depending on the active bits in the immediate Value v, the following
    // instruction sequences are emitted:
    //
    // v == 0                        : ADDI
    // v[0,12) != 0 && v[12,32) == 0 : ADDI
    // v[0,12) == 0 && v[12,32) != 0 : LUI
    // v[0,32) != 0                  : LUI+ADDI(W)
    int64_t Hi20 = ((Val + 0x800) >> 12) & 0xFFFFF;
    int64_t Lo12 = SignExtend64<12>(Val);

    if (Hi20)
      Res.emplace_back(YSX::LUI, Hi20);

    if (Lo12 || Hi20 == 0) {
      unsigned AddiOpc = YSX::ADDI;
      if (IsRV64 && Hi20) {
        // Use ADDIW rather than ADDI only when necessary for correctness. As
        // noted in YSXOptWInstrs, this helps reduce test differences vs
        // RV32 without being a pessimization.
        int64_t LuiRes = SignExtend64<32>(Hi20 << 12);
        if (!isInt<32>(LuiRes + Lo12))
          AddiOpc = YSX::ADDIW;
      }
      Res.emplace_back(AddiOpc, Lo12);
    }
    return;
  }

  assert(IsRV64 && "Can't emit >32-bit imm for non-RV64 target");

  // In the worst case, for a full 64-bit constant, a sequence of 8 instructions
  // (i.e., LUI+ADDI+SLLI+ADDI+SLLI+ADDI+SLLI+ADDI) has to be emitted. Note
  // that the first two instructions (LUI+ADDI) can contribute up to 32 bits
  // while the following ADDI instructions contribute up to 12 bits each.
  //
  // On the first glance, implementing this seems to be possible by simply
  // emitting the most significant 32 bits (LUI+ADDI(W)) followed by as many
  // left shift (SLLI) and immediate additions (ADDI) as needed. However, due to
  // the fact that ADDI performs a sign extended addition, doing it like that
  // would only be possible when at most 11 bits of the ADDI instructions are
  // used. Using all 12 bits of the ADDI instructions, like done by GAS,
  // actually requires that the constant is processed starting with the least
  // significant bit.
  //
  // In the following, constants are processed from LSB to MSB but instruction
  // emission is performed from MSB to LSB by recursively calling
  // generateInstSeq. In each recursion, first the lowest 12 bits are removed
  // from the constant and the optimal shift amount, which can be greater than
  // 12 bits if the constant is sparse, is determined. Then, the shifted
  // remaining constant is processed recursively and gets emitted as soon as it
  // fits into 32 bits. The emission of the shifts and additions is subsequently
  // performed when the recursion returns.

  int64_t Lo12 = SignExtend64<12>(Val);
  Val = (uint64_t)Val - (uint64_t)Lo12;

  int ShiftAmount = 0;

  // Val might now be valid for LUI without needing a shift.
  if (!isInt<32>(Val)) {
    ShiftAmount = llvm::countr_zero((uint64_t)Val);
    Val >>= ShiftAmount;

    // If the remaining bits don't fit in 12 bits, we might be able to reduce
    // the shift amount in order to use LUI which will zero the lower 12
    // bits.
    if (ShiftAmount > 12 && !isInt<12>(Val)) {
      if (isInt<32>((uint64_t)Val << 12)) {
        // Reduce the shift amount and add zeros to the LSBs so it will match
        // LUI.
        ShiftAmount -= 12;
        Val = (uint64_t)Val << 12;
      }
    }
  }

  generateInstSeqImpl(Val, STI, Res);

  // Skip shift if we were able to use LUI directly.
  if (ShiftAmount) {
    Res.emplace_back(YSX::SLLI, ShiftAmount);
  }

  if (Lo12)
    Res.emplace_back(YSX::ADDI, Lo12);
}

static void generateInstSeqLeadingZeros(int64_t Val, const MCSubtargetInfo &STI,
                                        YSXMatInt::InstSeq &Res) {
  assert(Val > 0 && "Expected positive val");

  unsigned LeadingZeros = llvm::countl_zero((uint64_t)Val);
  uint64_t ShiftedVal = (uint64_t)Val << LeadingZeros;
  // Fill in the bits that will be shifted out with 1s. An example where this
  // helps is trailing one masks with 32 or more ones. This will generate
  // ADDI -1 and an SRLI.
  ShiftedVal |= maskTrailingOnes<uint64_t>(LeadingZeros);

  YSXMatInt::InstSeq TmpSeq;
  generateInstSeqImpl(ShiftedVal, STI, TmpSeq);

  // Keep the new sequence if it is an improvement or the original is empty.
  if ((TmpSeq.size() + 1) < Res.size() ||
      (Res.empty() && TmpSeq.size() < 8)) {
    TmpSeq.emplace_back(YSX::SRLI, LeadingZeros);
    Res = TmpSeq;
  }

  // Some cases can benefit from filling the lower bits with zeros instead.
  ShiftedVal &= maskTrailingZeros<uint64_t>(LeadingZeros);
  TmpSeq.clear();
  generateInstSeqImpl(ShiftedVal, STI, TmpSeq);

  // Keep the new sequence if it is an improvement or the original is empty.
  if ((TmpSeq.size() + 1) < Res.size() ||
      (Res.empty() && TmpSeq.size() < 8)) {
    TmpSeq.emplace_back(YSX::SRLI, LeadingZeros);
    Res = TmpSeq;
  }
}

namespace llvm::YSXMatInt {
InstSeq generateInstSeq(int64_t Val, const MCSubtargetInfo &STI) {
  YSXMatInt::InstSeq Res;
  generateInstSeqImpl(Val, STI, Res);

  // If the low 12 bits are non-zero, the first expansion may end with an ADDI
  // or ADDIW. If there are trailing zeros, try generating a sign extended
  // constant with no trailing zeros and use a final SLLI to restore them.
  if ((Val & 0xfff) != 0 && (Val & 1) == 0 && Res.size() >= 2) {
    unsigned TrailingZeros = llvm::countr_zero((uint64_t)Val);
    int64_t ShiftedVal = Val >> TrailingZeros;
    // If we can use C.LI+C.SLLI instead of LUI+ADDI(W) prefer that since
    // its more compressible. But only if LUI+ADDI(W) isn't fusable.
    // NOTE: We don't check for C extension to minimize differences in generated
    // code.
    bool IsShiftedCompressible =
        isInt<6>(ShiftedVal) && !STI.hasFeature(YSX::YSXTuneLUIADDIFusion);
    YSXMatInt::InstSeq TmpSeq;
    generateInstSeqImpl(ShiftedVal, STI, TmpSeq);

    // Keep the new sequence if it is an improvement.
    if ((TmpSeq.size() + 1) < Res.size() || IsShiftedCompressible) {
      TmpSeq.emplace_back(YSX::SLLI, TrailingZeros);
      Res = TmpSeq;
    }
  }

  // If we have a 1 or 2 instruction sequence this is the best we can do. This
  // will always be true for RV32 and will often be true for RV64.
  if (Res.size() <= 2)
    return Res;

  assert(STI.hasFeature(YSX::Feature64Bit) &&
         "Expected RV32 to only need 2 instructions");

  // If the lower 13 bits are something like 0x17ff, try to add 1 to change the
  // lower 13 bits to 0x1800. We can restore this with an ADDI of -1 at the end
  // of the sequence. Call generateInstSeqImpl on the new constant which may
  // subtract 0xfffffffffffff800 to create another ADDI. This will leave a
  // constant with more than 12 trailing zeros for the next recursive step.
  if ((Val & 0xfff) != 0 && (Val & 0x1800) == 0x1000) {
    int64_t Imm12 = -(0x800 - (Val & 0xfff));
    int64_t AdjustedVal = Val - Imm12;
    YSXMatInt::InstSeq TmpSeq;
    generateInstSeqImpl(AdjustedVal, STI, TmpSeq);

    // Keep the new sequence if it is an improvement.
    if ((TmpSeq.size() + 1) < Res.size()) {
      TmpSeq.emplace_back(YSX::ADDI, Imm12);
      Res = TmpSeq;
    }
  }

  // If the constant is positive we might be able to generate a shifted constant
  // with no leading zeros and use a final SRLI to restore them.
  if (Val > 0 && Res.size() > 2) {
    generateInstSeqLeadingZeros(Val, STI, Res);
  }

  // If the constant is negative, trying inverting and using our trailing zero
  // optimizations. Use an xori to invert the final value.
  if (Val < 0 && Res.size() > 3) {
    uint64_t InvertedVal = ~(uint64_t)Val;
    YSXMatInt::InstSeq TmpSeq;
    generateInstSeqLeadingZeros(InvertedVal, STI, TmpSeq);

    // Keep it if we found a sequence that is smaller after inverting.
    if (!TmpSeq.empty() && (TmpSeq.size() + 1) < Res.size()) {
      TmpSeq.emplace_back(YSX::XORI, -1);
      Res = TmpSeq;
    }
  }

  return Res;
}

void generateMCInstSeq(int64_t Val, const MCSubtargetInfo &STI,
                       MCRegister DestReg, SmallVectorImpl<MCInst> &Insts) {
  YSXMatInt::InstSeq Seq = YSXMatInt::generateInstSeq(Val, STI);

  MCRegister SrcReg = YSX::X0;
  for (YSXMatInt::Inst &Inst : Seq) {
    switch (Inst.getOpndKind()) {
    case YSXMatInt::Imm:
      Insts.push_back(MCInstBuilder(Inst.getOpcode())
                          .addReg(DestReg)
                          .addImm(Inst.getImm()));
      break;
    case YSXMatInt::RegX0:
      Insts.push_back(MCInstBuilder(Inst.getOpcode())
                          .addReg(DestReg)
                          .addReg(SrcReg)
                          .addReg(YSX::X0));
      break;
    case YSXMatInt::RegReg:
      Insts.push_back(MCInstBuilder(Inst.getOpcode())
                          .addReg(DestReg)
                          .addReg(SrcReg)
                          .addReg(SrcReg));
      break;
    case YSXMatInt::RegImm:
      Insts.push_back(MCInstBuilder(Inst.getOpcode())
                          .addReg(DestReg)
                          .addReg(SrcReg)
                          .addImm(Inst.getImm()));
      break;
    }

    // Only the first instruction has X0 as its source.
    SrcReg = DestReg;
  }
}

InstSeq generateTwoRegInstSeq(int64_t Val, const MCSubtargetInfo &STI,
                              unsigned &ShiftAmt, unsigned &AddOpc) {
  int64_t LoVal = SignExtend64<32>(Val);
  if (LoVal == 0)
    return YSXMatInt::InstSeq();

  // Subtract the LoVal to emulate the effect of the final ADD.
  uint64_t Tmp = (uint64_t)Val - (uint64_t)LoVal;
  assert(Tmp != 0);

  // Use trailing zero counts to figure how far we need to shift LoVal to line
  // up with the remaining constant.
  // TODO: This algorithm assumes all non-zero bits in the low 32 bits of the
  // final constant come from LoVal.
  unsigned TzLo = llvm::countr_zero((uint64_t)LoVal);
  unsigned TzHi = llvm::countr_zero(Tmp);
  assert(TzLo < 32 && TzHi >= 32);
  ShiftAmt = TzHi - TzLo;
  AddOpc = YSX::ADD;

  if (Tmp == ((uint64_t)LoVal << ShiftAmt))
    return YSXMatInt::generateInstSeq(LoVal, STI);

  return YSXMatInt::InstSeq();
}

int getIntMatCost(const APInt &Val, unsigned Size, const MCSubtargetInfo &STI,
                  bool CompressionCost, bool FreeZeroes) {
  bool IsRV64 = STI.hasFeature(YSX::Feature64Bit);
  bool HasRVC = false;
  int PlatRegSize = IsRV64 ? 64 : 32;

  // Split the constant into platform register sized chunks, and calculate cost
  // of each chunk.
  int Cost = 0;
  for (unsigned ShiftVal = 0; ShiftVal < Size; ShiftVal += PlatRegSize) {
    APInt Chunk = Val.ashr(ShiftVal).sextOrTrunc(PlatRegSize);
    if (FreeZeroes && Chunk.getSExtValue() == 0)
      continue;
    InstSeq MatSeq = generateInstSeq(Chunk.getSExtValue(), STI);
    Cost += getInstSeqCost(MatSeq, HasRVC);
  }
  return std::max(FreeZeroes ? 0 : 1, Cost);
}

OpndKind Inst::getOpndKind() const {
  switch (Opc) {
  default:
    llvm_unreachable("Unexpected opcode!");
  case YSX::LUI:
    return YSXMatInt::Imm;
  case YSX::ADDI:
  case YSX::ADDIW:
  case YSX::XORI:
  case YSX::SLLI:
  case YSX::SRLI:
    return YSXMatInt::RegImm;
  }
}

} // namespace llvm::YSXMatInt
