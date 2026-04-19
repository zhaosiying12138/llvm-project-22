//===-- YSXDisassembler.cpp - Disassembler for RISC-V -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the YSXDisassembler class.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/YSXBaseInfo.h"
#include "MCTargetDesc/YSXMCTargetDesc.h"
#include "TargetInfo/YSXTargetInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDecoder.h"
#include "llvm/MC/MCDecoderOps.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Endian.h"

using namespace llvm;
using namespace llvm::MCD;

#define DEBUG_TYPE "ysx-disassembler"

typedef MCDisassembler::DecodeStatus DecodeStatus;

namespace {
class YSXDisassembler : public MCDisassembler {
  std::unique_ptr<MCInstrInfo const> const MCII;

public:
  YSXDisassembler(const MCSubtargetInfo &STI, MCContext &Ctx,
                    MCInstrInfo const *MCII)
      : MCDisassembler(STI, Ctx), MCII(MCII) {}

  DecodeStatus getInstruction(MCInst &Instr, uint64_t &Size,
                              ArrayRef<uint8_t> Bytes, uint64_t Address,
                              raw_ostream &CStream) const override;

private:
  DecodeStatus getInstruction48(MCInst &Instr, uint64_t &Size,
                                ArrayRef<uint8_t> Bytes, uint64_t Address,
                                raw_ostream &CStream) const;

  DecodeStatus getInstruction32(MCInst &Instr, uint64_t &Size,
                                ArrayRef<uint8_t> Bytes, uint64_t Address,
                                raw_ostream &CStream) const;
  DecodeStatus getInstruction16(MCInst &Instr, uint64_t &Size,
                                ArrayRef<uint8_t> Bytes, uint64_t Address,
                                raw_ostream &CStream) const;
};
} // end anonymous namespace

static MCDisassembler *createYSXDisassembler(const Target &T,
                                               const MCSubtargetInfo &STI,
                                               MCContext &Ctx) {
  return new YSXDisassembler(STI, Ctx, T.createMCInstrInfo());
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYSXDisassembler() {
  // Register the disassembler for the supported target.
  TargetRegistry::RegisterMCDisassembler(getTheYSX64Target(),
                                         createYSXDisassembler);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYuShuXinDisassembler() {
  LLVMInitializeYSXDisassembler();
}

static DecodeStatus DecodeGPRRegisterClass(MCInst &Inst, uint32_t RegNo,
                                           uint64_t Address,
                                           const MCDisassembler *Decoder) {
  bool IsRVE = false;

  if (RegNo >= 32 || (IsRVE && RegNo >= 16))
    return MCDisassembler::Fail;

  MCRegister Reg = YSX::X0 + RegNo;
  Inst.addOperand(MCOperand::createReg(Reg));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeUImmOperand(MCInst &Inst, uint32_t Imm,
                                      int64_t Address,
                                      const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

template <unsigned Width, unsigned LowerBound>
static DecodeStatus decodeUImmOperandGE(MCInst &Inst, uint32_t Imm,
                                        int64_t Address,
                                        const MCDisassembler *Decoder) {
  assert(isUInt<Width>(Imm) && "Invalid immediate");

  if (Imm < LowerBound)
    return MCDisassembler::Fail;

  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

template <unsigned Width, unsigned LowerBound>
static DecodeStatus decodeUImmPlus1OperandGE(MCInst &Inst, uint32_t Imm,
                                             int64_t Address,
                                             const MCDisassembler *Decoder) {
  assert(isUInt<Width>(Imm) && "Invalid immediate");

  if ((Imm + 1) < LowerBound)
    return MCDisassembler::Fail;

  Inst.addOperand(MCOperand::createImm(Imm + 1));
  return MCDisassembler::Success;
}

static DecodeStatus decodeUImmLog2XLenOperand(MCInst &Inst, uint32_t Imm,
                                              int64_t Address,
                                              const MCDisassembler *Decoder) {
  assert(isUInt<6>(Imm) && "Invalid immediate");

  if (!Decoder->getSubtargetInfo().hasFeature(YSX::Feature64Bit) &&
      !isUInt<5>(Imm))
    return MCDisassembler::Fail;

  Inst.addOperand(MCOperand::createImm(Imm));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeUImmNonZeroOperand(MCInst &Inst, uint32_t Imm,
                                             int64_t Address,
                                             const MCDisassembler *Decoder) {
  if (Imm == 0)
    return MCDisassembler::Fail;
  return decodeUImmOperand<N>(Inst, Imm, Address, Decoder);
}

template <unsigned N>
static DecodeStatus decodeUImmPlus1Operand(MCInst &Inst, uint32_t Imm,
                                           int64_t Address,
                                           const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  Inst.addOperand(MCOperand::createImm(Imm + 1));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeSImmOperand(MCInst &Inst, uint32_t Imm,
                                      int64_t Address,
                                      const MCDisassembler *Decoder) {
  assert(isUInt<N>(Imm) && "Invalid immediate");
  // Sign-extend the number in the bottom N bits of Imm
  Inst.addOperand(MCOperand::createImm(SignExtend64<N>(Imm)));
  return MCDisassembler::Success;
}

template <unsigned N>
static DecodeStatus decodeSImmNonZeroOperand(MCInst &Inst, uint32_t Imm,
                                             int64_t Address,
                                             const MCDisassembler *Decoder) {
  if (Imm == 0)
    return MCDisassembler::Fail;
  return decodeSImmOperand<N>(Inst, Imm, Address, Decoder);
}

template <unsigned T, unsigned N>
static DecodeStatus decodeSImmOperandAndLslN(MCInst &Inst, uint32_t Imm,
                                             int64_t Address,
                                             const MCDisassembler *Decoder) {
  assert(isUInt<T - N + 1>(Imm) && "Invalid immediate");
  // Sign-extend the number in the bottom T bits of Imm after accounting for
  // the fact that the T bit immediate is stored in T-N bits (the LSB is
  // always zero)
  Inst.addOperand(MCOperand::createImm(SignExtend64<T>(Imm << N)));
  return MCDisassembler::Success;
}

#include "YSXGenDisassemblerTables.inc"

namespace {

struct DecoderListEntry {
  const uint8_t *Table;
  const char *Desc;
};

} // end anonymous namespace

static constexpr DecoderListEntry DecoderList32[]{
    {DecoderTable32, "standard 32-bit instructions"},
};

namespace {
// Define bitwidths for various types used to instantiate the decoder.
template <> constexpr uint32_t InsnBitWidth<uint32_t> = 32;
} // namespace

DecodeStatus YSXDisassembler::getInstruction32(MCInst &MI, uint64_t &Size,
                                                 ArrayRef<uint8_t> Bytes,
                                                 uint64_t Address,
                                                 raw_ostream &CS) const {
  if (Bytes.size() < 4) {
    Size = 0;
    return MCDisassembler::Fail;
  }
  Size = 4;

  uint32_t Insn = support::endian::read32le(Bytes.data());

  for (const DecoderListEntry &Entry : DecoderList32) {
    LLVM_DEBUG(dbgs() << "Trying " << Entry.Desc << " table:\n");
    DecodeStatus Result =
        decodeInstruction(Entry.Table, MI, Insn, Address, this, STI);
    if (Result == MCDisassembler::Fail)
      continue;

    return Result;
  }

  return MCDisassembler::Fail;
}

DecodeStatus YSXDisassembler::getInstruction16(MCInst &MI, uint64_t &Size,
                                                 ArrayRef<uint8_t> Bytes,
                                                 uint64_t Address,
                                                 raw_ostream &CS) const {
  Size = Bytes.size() >= 2 ? 2 : 0;
  return MCDisassembler::Fail;
}

DecodeStatus YSXDisassembler::getInstruction48(MCInst &MI, uint64_t &Size,
                                                 ArrayRef<uint8_t> Bytes,
                                                 uint64_t Address,
                                                 raw_ostream &CS) const {
  Size = Bytes.size() >= 6 ? 6 : 0;
  return MCDisassembler::Fail;
}

DecodeStatus YSXDisassembler::getInstruction(MCInst &MI, uint64_t &Size,
                                               ArrayRef<uint8_t> Bytes,
                                               uint64_t Address,
                                               raw_ostream &CS) const {
  CommentStream = &CS;
  // It's a 16 bit instruction if bit 0 and 1 are not 0b11.
  if ((Bytes[0] & 0b11) != 0b11)
    return getInstruction16(MI, Size, Bytes, Address, CS);

  // Try to decode as a 32-bit instruction first.
  DecodeStatus Result = getInstruction32(MI, Size, Bytes, Address, CS);
  if (Result != MCDisassembler::Fail)
    return Result;

  // If bits [4:2] are 0b111 this might be a 48-bit or larger instruction,
  // otherwise assume it's an unknown 32-bit instruction.
  if ((Bytes[0] & 0b1'1100) != 0b1'1100) {
    Size = Bytes.size() >= 4 ? 4 : 0;
    return MCDisassembler::Fail;
  }

  // 48-bit instructions are encoded as 0bxx011111.
  if ((Bytes[0] & 0b11'1111) == 0b01'1111)
    return getInstruction48(MI, Size, Bytes, Address, CS);

  // 64-bit instructions are encoded as 0x0111111.
  if ((Bytes[0] & 0b111'1111) == 0b011'1111) {
    Size = Bytes.size() >= 8 ? 8 : 0;
    return MCDisassembler::Fail;
  }

  // Remaining cases need to check a second byte.
  if (Bytes.size() < 2) {
    Size = 0;
    return MCDisassembler::Fail;
  }

  // 80-bit through 176-bit instructions are encoded as 0bxnnnxxxx_x1111111.
  // Where the number of bits is (80 + (nnn * 16)) for nnn != 0b111.
  unsigned nnn = (Bytes[1] >> 4) & 0b111;
  if (nnn != 0b111) {
    Size = 10 + (nnn * 2);
    if (Bytes.size() < Size)
      Size = 0;
    return MCDisassembler::Fail;
  }

  // Remaining encodings are reserved for > 176-bit instructions.
  Size = 0;
  return MCDisassembler::Fail;
}
