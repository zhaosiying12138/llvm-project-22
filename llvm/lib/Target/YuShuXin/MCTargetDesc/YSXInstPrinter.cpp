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
  const MCInst *NewMI = MI;
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

void YSXInstPrinter::printZeroOffsetMemOp(const MCInst *MI, unsigned OpNo,
                                            const MCSubtargetInfo &STI,
                                            raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);

  assert(MO.isReg() && "printZeroOffsetMemOp can only print register operands");
  O << "(";
  printRegName(O, MO.getReg());
  O << ")";
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
  // as fp instead of s0. GPR-pair aliases such as X8_X9 are not replaced.
  if (!ArchRegNames && EmitX8AsFP && Reg == YSX::X8)
    return "fp";
  return getRegisterName(Reg, ArchRegNames ? YSX::NoRegAltName
                                           : YSX::ABIRegAltName);
}
