//===-- YSXInstPrinter.cpp - Convert RISC-V MCInst to asm syntax --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This class prints an RISC-V MCInst to a .s file.
//
//===----------------------------------------------------------------------===//

#include "YSXInstPrinter.h"
#include "YSXBaseInfo.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the auto-generated portion of the assembly writer.
#define PRINT_ALIAS_INSTR
#include "YSXGenAsmWriter.inc"

static cl::opt<bool>
    NoAliases("ysx-no-aliases",
              cl::desc("Disable the emission of assembler pseudo instructions"),
              cl::init(false), cl::Hidden);

static cl::opt<bool> EmitX8AsFP("ysx-emit-x8-as-fp",
                                cl::desc("Emit x8 as fp instead of s0"),
                                cl::init(false), cl::Hidden);

// Print architectural register names rather than the ABI names (such as x2
// instead of sp).
// TODO: Make YSXInstPrinter::getRegisterName non-static so that this can a
// member.
static bool ArchRegNames;

// The command-line flags above are used by llvm-mc and llc. They can be used by
// `llvm-objdump`, but we override their values here to handle options passed to
// `llvm-objdump` with `-M` (which matches GNU objdump). There did not seem to
// be an easier way to allow these options in all these tools, without doing it
// this way.
bool YSXInstPrinter::applyTargetSpecificCLOption(StringRef Opt) {
  if (Opt == "no-aliases") {
    PrintAliases = false;
    return true;
  }
  if (Opt == "numeric") {
    ArchRegNames = true;
    return true;
  }
  if (Opt == "emit-x8-as-fp") {
    if (!ArchRegNames)
      EmitX8AsFP = true;
    return true;
  }

  return false;
}

void YSXInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                 StringRef Annot, const MCSubtargetInfo &STI,
                                 raw_ostream &O) {
  bool Res = false;
  const MCInst *NewMI = MI;
  MCInst UncompressedMI;
  if (PrintAliases && !NoAliases)
    Res = YSXRVC::uncompress(UncompressedMI, *MI, STI);
  if (Res)
    NewMI = &UncompressedMI;
  if (!PrintAliases || NoAliases || !printAliasInstr(NewMI, Address, STI, O))
    printInstruction(NewMI, Address, STI, O);
  printAnnotation(O, Annot);
}

void YSXInstPrinter::printRegName(raw_ostream &O, MCRegister Reg) {
  markup(O, Markup::Register) << getRegisterName(Reg);
}

void YSXInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                    const MCSubtargetInfo &STI,
                                    raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);

  if (MO.isReg()) {
    printRegName(O, MO.getReg());
    return;
  }

  if (MO.isImm()) {
    printImm(MI, OpNo, STI, O);
    return;
  }

  assert(MO.isExpr() && "Unknown operand kind in printOperand");
  MAI.printExpr(O, *MO.getExpr());
}

void YSXInstPrinter::printBranchOperand(const MCInst *MI, uint64_t Address,
                                          unsigned OpNo,
                                          const MCSubtargetInfo &STI,
                                          raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);
  if (!MO.isImm())
    return printOperand(MI, OpNo, STI, O);

  if (PrintBranchImmAsAddress) {
    uint64_t Target = Address + MO.getImm();
    if (!STI.hasFeature(YSX::Feature64Bit))
      Target &= 0xffffffff;
    markup(O, Markup::Target) << formatHex(Target);
  } else {
    markup(O, Markup::Target) << formatImm(MO.getImm());
  }
}

void YSXInstPrinter::printCSRSystemRegister(const MCInst *MI, unsigned OpNo,
                                              const MCSubtargetInfo &STI,
                                              raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  auto Range = YSXSysReg::lookupSysRegByEncoding(Imm);
  for (auto &Reg : Range) {
    if (Reg.IsAltName || Reg.IsDeprecatedName)
      continue;
    if (Reg.haveRequiredFeatures(STI.getFeatureBits())) {
      markup(O, Markup::Register) << Reg.Name;
      return;
    }
  }
  markup(O, Markup::Register) << formatImm(Imm);
}

void YSXInstPrinter::printFenceArg(const MCInst *MI, unsigned OpNo,
                                     const MCSubtargetInfo &STI,
                                     raw_ostream &O) {
  unsigned FenceArg = MI->getOperand(OpNo).getImm();
  assert (((FenceArg >> 4) == 0) && "Invalid immediate in printFenceArg");

  if ((FenceArg & YSXFenceField::I) != 0)
    O << 'i';
  if ((FenceArg & YSXFenceField::O) != 0)
    O << 'o';
  if ((FenceArg & YSXFenceField::R) != 0)
    O << 'r';
  if ((FenceArg & YSXFenceField::W) != 0)
    O << 'w';
  if (FenceArg == 0)
    O << "0";
}

void YSXInstPrinter::printFRMArg(const MCInst *MI, unsigned OpNo,
                                   const MCSubtargetInfo &STI, raw_ostream &O) {
  auto FRMArg =
      static_cast<YSXFPRndMode::RoundingMode>(MI->getOperand(OpNo).getImm());
  if (PrintAliases && !NoAliases && FRMArg == YSXFPRndMode::RoundingMode::DYN)
    return;
  O << ", " << YSXFPRndMode::roundingModeToString(FRMArg);
}

void YSXInstPrinter::printFRMArgLegacy(const MCInst *MI, unsigned OpNo,
                                         const MCSubtargetInfo &STI,
                                         raw_ostream &O) {
  auto FRMArg =
      static_cast<YSXFPRndMode::RoundingMode>(MI->getOperand(OpNo).getImm());
  // Never print rounding mode if it's the default 'rne'. This ensures the
  // output can still be parsed by older tools that erroneously failed to
  // accept a rounding mode.
  if (FRMArg == YSXFPRndMode::RoundingMode::RNE)
    return;
  O << ", " << YSXFPRndMode::roundingModeToString(FRMArg);
}

void YSXInstPrinter::printFPImmOperand(const MCInst *MI, unsigned OpNo,
                                         const MCSubtargetInfo &STI,
                                         raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  if (Imm == 1) {
    markup(O, Markup::Immediate) << "min";
  } else if (Imm == 30) {
    markup(O, Markup::Immediate) << "inf";
  } else if (Imm == 31) {
    markup(O, Markup::Immediate) << "nan";
  } else {
    float FPVal = YSXLoadFPImm::getFPImm(Imm);
    // If the value is an integer, print a .0 fraction. Otherwise, use %g to
    // which will not print trailing zeros and will use scientific notation
    // if it is shorter than printing as a decimal. The smallest value requires
    // 12 digits of precision including the decimal.
    if (FPVal == (int)(FPVal))
      markup(O, Markup::Immediate) << format("%.1f", FPVal);
    else
      markup(O, Markup::Immediate) << format("%.12g", FPVal);
  }
}

void YSXInstPrinter::printZeroOffsetMemOp(const MCInst *MI, unsigned OpNo,
                                            const MCSubtargetInfo &STI,
                                            raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);

  assert(MO.isReg() && "printZeroOffsetMemOp can only print register operands");
  O << "(";
  printRegName(O, MO.getReg());
  O << ")";
}

void YSXInstPrinter::printVTypeI(const MCInst *MI, unsigned OpNo,
                                   const MCSubtargetInfo &STI, raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  // Print the raw immediate for reserved values: vlmul[2:0]=4, vsew[2:0]=0b1xx,
  // altfmt=1 without zvfbfa or zvfofp8min extension, or non-zero in bits 9 and
  // above.
  if (YSXVType::getVLMUL(Imm) == YSXVType::VLMUL::LMUL_RESERVED ||
      YSXVType::getSEW(Imm) > 64 ||
      (YSXVType::isAltFmt(Imm) &&
       !(STI.hasFeature(YSX::FeatureStdExtZvfbfa) ||
         STI.hasFeature(YSX::FeatureStdExtZvfofp8min) ||
         STI.hasFeature(YSX::FeatureVendorXSfvfbfexp16e))) ||
      (Imm >> 9) != 0) {
    O << formatImm(Imm);
    return;
  }
  // Print the text form.
  YSXVType::printVType(Imm, O);
}

void YSXInstPrinter::printXSfmmVType(const MCInst *MI, unsigned OpNo,
                                       const MCSubtargetInfo &STI,
                                       raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();
  assert(YSXVType::isValidXSfmmVType(Imm));
  unsigned SEW = YSXVType::getSEW(Imm);
  O << "e" << SEW;
  bool AltFmt = YSXVType::isAltFmt(Imm);
  if (AltFmt)
    O << "alt";
  unsigned Widen = YSXVType::getXSfmmWiden(Imm);
  O << ", w" << Widen;
}

// Print a Zcmp RList. If we are printing architectural register names rather
// than ABI register names, we need to print "{x1, x8-x9, x18-x27}" for all
// registers. Otherwise, we print "{ra, s0-s11}".
void YSXInstPrinter::printRegList(const MCInst *MI, unsigned OpNo,
                                    const MCSubtargetInfo &STI, raw_ostream &O) {
  unsigned Imm = MI->getOperand(OpNo).getImm();

  assert(Imm >= YSXZC::RLISTENCODE::RA &&
         Imm <= YSXZC::RLISTENCODE::RA_S0_S11 && "Invalid Rlist");

  O << "{";
  printRegName(O, YSX::X1);

  if (Imm >= YSXZC::RLISTENCODE::RA_S0) {
    O << ", ";
    printRegName(O, YSX::X8);
  }

  if (Imm >= YSXZC::RLISTENCODE::RA_S0_S1) {
    O << '-';
    if (Imm == YSXZC::RLISTENCODE::RA_S0_S1 || ArchRegNames)
      printRegName(O, YSX::X9);
  }

  if (Imm >= YSXZC::RLISTENCODE::RA_S0_S2) {
    if (ArchRegNames)
      O << ", ";
    if (Imm == YSXZC::RLISTENCODE::RA_S0_S2 || ArchRegNames)
      printRegName(O, YSX::X18);
  }

  if (Imm >= YSXZC::RLISTENCODE::RA_S0_S3) {
    if (ArchRegNames)
      O << '-';
    unsigned Offset = (Imm - YSXZC::RLISTENCODE::RA_S0_S3);
    // Encodings for S3-S9 are contiguous. There is no encoding for S10, so we
    // must skip to S11(X27).
    if (Imm == YSXZC::RLISTENCODE::RA_S0_S11)
      ++Offset;
    printRegName(O, YSX::X19 + Offset);
  }

  O << "}";
}

void YSXInstPrinter::printRegReg(const MCInst *MI, unsigned OpNo,
                                   const MCSubtargetInfo &STI, raw_ostream &O) {
  const MCOperand &OffsetMO = MI->getOperand(OpNo + 1);

  assert(OffsetMO.isReg() && "printRegReg can only print register operands");
  printRegName(O, OffsetMO.getReg());

  O << "(";
  const MCOperand &BaseMO = MI->getOperand(OpNo);
  assert(BaseMO.isReg() && "printRegReg can only print register operands");
  printRegName(O, BaseMO.getReg());
  O << ")";
}

void YSXInstPrinter::printStackAdj(const MCInst *MI, unsigned OpNo,
                                     const MCSubtargetInfo &STI, raw_ostream &O,
                                     bool Negate) {
  int64_t Imm = MI->getOperand(OpNo).getImm();
  bool IsRV64 = STI.hasFeature(YSX::Feature64Bit);
  int64_t StackAdj = 0;
  auto RlistVal = MI->getOperand(0).getImm();
  auto Base = YSXZC::getStackAdjBase(RlistVal, IsRV64);
  StackAdj = Imm + Base;
  assert((StackAdj >= Base && StackAdj <= Base + 48) &&
         "Incorrect stack adjust");
  if (Negate)
    StackAdj = -StackAdj;

  // RAII guard for ANSI color escape sequences
  WithMarkup ScopedMarkup = markup(O, Markup::Immediate);
  O << StackAdj;
}

void YSXInstPrinter::printVMaskReg(const MCInst *MI, unsigned OpNo,
                                     const MCSubtargetInfo &STI,
                                     raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);

  assert(MO.isReg() && "printVMaskReg can only print register operands");
  if (MO.getReg() == YSX::NoRegister)
    return;
  O << ", ";
  printRegName(O, MO.getReg());
  O << ".t";
}

void YSXInstPrinter::printImm(const MCInst *MI, unsigned OpNo,
                                const MCSubtargetInfo &STI, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  const unsigned Opcode = MI->getOpcode();
  uint64_t Imm = Op.getImm();
  if (STI.getTargetTriple().isOSBinFormatMachO() &&
      (Opcode == YSX::ANDI || Opcode == YSX::ORI || Opcode == YSX::XORI ||
       Opcode == YSX::AUIPC || Opcode == YSX::LUI)) {
    if (!STI.hasFeature(YSX::Feature64Bit))
      Imm &= 0xffffffff;
    markup(O, Markup::Immediate) << formatHex(Imm);
  } else
    markup(O, Markup::Immediate) << formatImm(Imm);
}

const char *YSXInstPrinter::getRegisterName(MCRegister Reg) {
  // When PrintAliases is enabled, and EmitX8AsFP is enabled, x8 will be printed
  // as fp instead of s0. Note that these similar registers are not replaced:
  // - X8_H: used for f16 register in zhinx
  // - X8_W: used for f32 register in zfinx
  // - X8_X9: used for GPR Pair
  if (!ArchRegNames && EmitX8AsFP && Reg == YSX::X8)
    return "fp";
  return getRegisterName(Reg, ArchRegNames ? YSX::NoRegAltName
                                           : YSX::ABIRegAltName);
}
