//===-- YSXInstrFormats.td - RISC-V Instruction Formats --*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
//  These instruction format definitions are structured to match the
//  description in the RISC-V User-Level ISA specification as closely as
//  possible. For instance, the specification describes instructions with the
//  MSB (31st bit) on the left and the LSB (0th bit) on the right. This is
//  reflected in the order of parameters to each instruction class.
//
//  One area of divergence is in the description of immediates. The
//  specification describes immediate encoding in terms of bit-slicing
//  operations on the logical value represented. The immediate argument to
//  these instruction formats instead represents the bit sequence that will be
//  inserted into the instruction. e.g. although JAL's immediate is logically
//  a 21-bit value (where the LSB is always zero), we describe it as an imm20
//  to match how it is encoded.
//
//===----------------------------------------------------------------------===//

// Format specifies the encoding used by the instruction. This is used by
// YSXMCCodeEmitter to determine which form of fixup to use. These
// definitions must be kept in-sync with YSXBaseInfo.h.
class InstFormat<bits<5> val> {
  bits<5> Value = val;
}
def InstFormatPseudo        : InstFormat<0>;
def InstFormatR             : InstFormat<1>;
def InstFormatR4            : InstFormat<2>;
def InstFormatI             : InstFormat<3>;
def InstFormatS             : InstFormat<4>;
def InstFormatB             : InstFormat<5>;
def InstFormatU             : InstFormat<6>;
def InstFormatJ             : InstFormat<7>;
def InstFormatCR            : InstFormat<8>;
def InstFormatCI            : InstFormat<9>;
def InstFormatCSS           : InstFormat<10>;
def InstFormatCIW           : InstFormat<11>;
def InstFormatCL            : InstFormat<12>;
def InstFormatCS            : InstFormat<13>;
def InstFormatCA            : InstFormat<14>;
def InstFormatCB            : InstFormat<15>;
def InstFormatCJ            : InstFormat<16>;
def InstFormatCU            : InstFormat<17>;
def InstFormatCLB           : InstFormat<18>;
def InstFormatCLH           : InstFormat<19>;
def InstFormatCSB           : InstFormat<20>;
def InstFormatCSH           : InstFormat<21>;
def InstFormatQC_EAI        : InstFormat<22>;
def InstFormatQC_EI         : InstFormat<23>;
def InstFormatQC_EB         : InstFormat<24>;
def InstFormatQC_EJ         : InstFormat<25>;
def InstFormatQC_ES         : InstFormat<26>;
def InstFormatNDS_BRANCH_10 : InstFormat<27>;
def InstFormatOther         : InstFormat<31>;


// The following opcode names match those given in Table 19.1 in the
// RISC-V User-level ISA specification ("RISC-V base opcode map").
class YSXOpcode<string name, bits<7> val> {
  string Name = name;
  bits<7> Value = val;
}
def YSXOpcodesList : GenericTable {
  let FilterClass = "YSXOpcode";
  let Fields = [
    "Name", "Value"
  ];
  let PrimaryKey = [ "Value" ];
  let PrimaryKeyName = "lookupYSXOpcodeByValue";
}
def lookupYSXOpcodeByName : SearchIndex {
  let Table = YSXOpcodesList;
  let Key = [ "Name" ];
}
def OPC_LOAD      : YSXOpcode<"LOAD",      0b0000011>;
def OPC_LOAD_FP   : YSXOpcode<"LOAD_FP",   0b0000111>;
def OPC_CUSTOM_0  : YSXOpcode<"CUSTOM_0",  0b0001011>;
def OPC_MISC_MEM  : YSXOpcode<"MISC_MEM",  0b0001111>;
def OPC_OP_IMM    : YSXOpcode<"OP_IMM",    0b0010011>;
def OPC_AUIPC     : YSXOpcode<"AUIPC",     0b0010111>;
def OPC_OP_IMM_32 : YSXOpcode<"OP_IMM_32", 0b0011011>;
def OPC_STORE     : YSXOpcode<"STORE",     0b0100011>;
def OPC_STORE_FP  : YSXOpcode<"STORE_FP",  0b0100111>;
def OPC_CUSTOM_1  : YSXOpcode<"CUSTOM_1",  0b0101011>;
def OPC_AMO       : YSXOpcode<"AMO",       0b0101111>;
def OPC_OP        : YSXOpcode<"OP",        0b0110011>;
def OPC_LUI       : YSXOpcode<"LUI",       0b0110111>;
def OPC_OP_32     : YSXOpcode<"OP_32",     0b0111011>;
def OPC_MADD      : YSXOpcode<"MADD",      0b1000011>;
def OPC_MSUB      : YSXOpcode<"MSUB",      0b1000111>;
def OPC_NMSUB     : YSXOpcode<"NMSUB",     0b1001011>;
def OPC_NMADD     : YSXOpcode<"NMADD",     0b1001111>;
def OPC_OP_FP     : YSXOpcode<"OP_FP",     0b1010011>;
def OPC_OP_V      : YSXOpcode<"OP_V",      0b1010111>;
def OPC_CUSTOM_2  : YSXOpcode<"CUSTOM_2",  0b1011011>;
def OPC_BRANCH    : YSXOpcode<"BRANCH",    0b1100011>;
def OPC_JALR      : YSXOpcode<"JALR",      0b1100111>;
def OPC_JAL       : YSXOpcode<"JAL",       0b1101111>;
def OPC_SYSTEM    : YSXOpcode<"SYSTEM",    0b1110011>;
def OPC_OP_VE     : YSXOpcode<"OP_VE",     0b1110111>;
def OPC_CUSTOM_3  : YSXOpcode<"CUSTOM_3",  0b1111011>;

class EltDeps<bit vl, bit mask> {
  bit VL = vl;
  bit Mask = mask;
}

def EltDepsNone      : EltDeps<vl=0, mask=0>;
def EltDepsVL        : EltDeps<vl=1, mask=0>;
def EltDepsMask      : EltDeps<vl=0, mask=1>;
def EltDepsVLMask    : EltDeps<vl=1, mask=1>;

class RVInstCommon<dag outs, dag ins, string opcodestr, string argstr,
                   list<dag> pattern, InstFormat format> : Instruction {
  let Namespace = "YSX";

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = opcodestr # !if(!empty(argstr), "", "\t" # argstr);
  let Pattern = pattern;

  InstFormat Format = format;

  let TSFlags{4-0} = Format.Value;

  // Indicates that the result can be considered sign extended from bit 31. Some
  // instructions with this flag aren't W instructions, but are either sign
  // extended from a smaller size, always outputs a small integer, or put zeros
  // in bits 63:31. Used by the SExtWRemoval pass.
  bit IsSignExtendingOpW = 0;
  let TSFlags{5} = IsSignExtendingOpW;
}

class RVInst<dag outs, dag ins, string opcodestr, string argstr,
             list<dag> pattern, InstFormat format>
    : RVInstCommon<outs, ins, opcodestr, argstr, pattern, format> {
  field bits<32> Inst;
  let Size = 4;
}

class RVInst48<dag outs, dag ins, string opcodestr, string argstr,
               list<dag> pattern, InstFormat format>
    : RVInstCommon<outs, ins, opcodestr, argstr, pattern, format> {
  field bits<48> Inst;
  let Size = 6;
}

class RVInst64<dag outs, dag ins, string opcodestr, string argstr,
               list<dag> pattern, InstFormat format>
    : RVInstCommon<outs, ins, opcodestr, argstr, pattern, format> {
  field bits<64> Inst;
  let Size = 8;
}

// Pseudo instructions
class Pseudo<dag outs, dag ins, list<dag> pattern, string opcodestr = "", string argstr = "">
    : RVInstCommon<outs, ins, opcodestr, argstr, pattern, InstFormatPseudo> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
  let Size = 4;
}

class PseudoQuietFCMP<DAGOperand Ty>
    : Pseudo<(outs GPR:$rd), (ins Ty:$rs1, Ty:$rs2), []> {
  let hasSideEffects = 1;
  let mayLoad = 0;
  let mayStore = 0;
}

// Pseudo load instructions.
class PseudoLoad<string opcodestr, DAGOperand rdty = GPR>
    : Pseudo<(outs rdty:$rd), (ins bare_symbol:$addr), [], opcodestr, "$rd, $addr"> {
  let hasSideEffects = 0;
  let mayLoad = 1;
  let mayStore = 0;
  let isCodeGenOnly = 0;
  let isAsmParserOnly = 1;
}

class PseudoFloatLoad<string opcodestr, RegisterClass rdty>
    : Pseudo<(outs GPR:$tmp, rdty:$rd), (ins bare_symbol:$addr), [], opcodestr, "$rd, $addr, $tmp"> {
  let hasSideEffects = 0;
  let mayLoad = 1;
  let mayStore = 0;
  let isCodeGenOnly = 0;
  let isAsmParserOnly = 1;
}

// Pseudo store instructions.
class PseudoStore<string opcodestr, DAGOperand rsty = GPR>
    : Pseudo<(outs GPR:$tmp), (ins rsty:$rs, bare_symbol:$addr), [], opcodestr, "$rs, $addr, $tmp"> {
  let hasSideEffects = 0;
  let mayLoad = 0;
  let mayStore = 1;
  let isCodeGenOnly = 0;
  let isAsmParserOnly = 1;
}

// Instruction formats are listed in the order they appear in the RISC-V
// instruction set manual (R, R4, I, S, B, U, J).

// Common base class for R format instructions. Bits {31-25} should be set by
// the subclasses.
class RVInstRBase<bits<3> funct3, YSXOpcode opcode, dag outs,
                  dag ins, string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode.Value;
}

class RVInstR<bits<7> funct7, bits<3> funct3, YSXOpcode opcode, dag outs,
              dag ins, string opcodestr, string argstr>
    : RVInstRBase<funct3, opcode, outs, ins, opcodestr, argstr> {
  let Inst{31-25} = funct7;
}

class RVInstRAtomic<bits<5> funct5, bit aq, bit rl, bits<3> funct3,
                    YSXOpcode opcode, dag outs, dag ins, string opcodestr,
                    string argstr>
    : RVInstRBase<funct3, opcode, outs, ins, opcodestr, argstr> {
  let Inst{31-27} = funct5;
  let Inst{26} = aq;
  let Inst{25} = rl;
}

class RVInstRFrm<bits<7> funct7, YSXOpcode opcode, dag outs, dag ins,
                 string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs2;
  bits<5> rs1;
  bits<3> frm;
  bits<5> rd;

  let Inst{31-25} = funct7;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = frm;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode.Value;
}

class RVInstR4<bits<2> funct2, bits<3> funct3, YSXOpcode opcode, dag outs,
               dag ins, string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatR4> {
  bits<5> rs3;
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-27} = rs3;
  let Inst{26-25} = funct2;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode.Value;
}

class RVInstR4Frm<bits<2> funct2, YSXOpcode opcode, dag outs, dag ins,
                  string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatR4> {
  bits<5> rs3;
  bits<5> rs2;
  bits<5> rs1;
  bits<3> frm;
  bits<5> rd;

  let Inst{31-27} = rs3;
  let Inst{26-25} = funct2;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = frm;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode.Value;
}

// Common base class for I format instructions. Bits {31-20} should be set by
// the subclasses.
class RVInstIBase<bits<3> funct3, YSXOpcode opcode, dag outs, dag ins,
                  string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatI> {
  bits<5> rs1;
  bits<5> rd;

  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode.Value;
}

class RVInstI<bits<3> funct3, YSXOpcode opcode, dag outs, dag ins,
              string opcodestr, string argstr>
    : RVInstIBase<funct3, opcode, outs, ins, opcodestr, argstr> {
  bits<12> imm12;

  let Inst{31-20} = imm12;
}

class RVInstIShift<bits<5> imm11_7, bits<3> funct3, YSXOpcode opcode,
                   dag outs, dag ins, string opcodestr, string argstr>
    : RVInstIBase<funct3, opcode, outs, ins, opcodestr, argstr> {
  bits<6> shamt;

  let Inst{31-27} = imm11_7;
  let Inst{26} = 0;
  let Inst{25-20} = shamt;
}

class RVInstIShiftW<bits<7> imm11_5, bits<3> funct3, YSXOpcode opcode,
                    dag outs, dag ins, string opcodestr, string argstr>
    : RVInstIBase<funct3, opcode, outs, ins, opcodestr, argstr> {
  bits<5> shamt;

  let Inst{31-25} = imm11_5;
  let Inst{24-20} = shamt;
}

class RVInstIUnary<bits<12> imm12, bits<3> funct3, YSXOpcode opcode,
                   dag outs, dag ins, string opcodestr, string argstr>
    : RVInstIBase<funct3, opcode, outs, ins, opcodestr, argstr> {
  let Inst{31-20} = imm12;
}

class RVInstS<bits<3> funct3, YSXOpcode opcode, dag outs, dag ins,
              string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatS> {
  bits<12> imm12;
  bits<5> rs2;
  bits<5> rs1;

  let Inst{31-25} = imm12{11-5};
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = imm12{4-0};
  let Inst{6-0} = opcode.Value;
}

class RVInstB<bits<3> funct3, YSXOpcode opcode, dag outs, dag ins,
              string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatB> {
  bits<12> imm12;
  bits<5> rs2;
  bits<5> rs1;

  let Inst{31} = imm12{11};
  let Inst{30-25} = imm12{9-4};
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-8} = imm12{3-0};
  let Inst{7} = imm12{10};
  let Inst{6-0} = opcode.Value;
}

class RVInstBIMM<bits<3> funct3, YSXOpcode opcode, dag outs, dag ins,
                 string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatB> {
  bits<12> imm12;
  bits<5> cimm;
  bits<5> rs1;
  let Inst{31} = imm12{11};
  let Inst{30-25} = imm12{9-4};
  let Inst{24-20} = cimm;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-8} = imm12{3-0};
  let Inst{7} = imm12{10};
  let Inst{6-0} = opcode.Value;
}

class RVInstU<YSXOpcode opcode, dag outs, dag ins, string opcodestr,
              string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatU> {
  bits<20> imm20;
  bits<5> rd;

  let Inst{31-12} = imm20;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode.Value;
}

class RVInstJ<YSXOpcode opcode, dag outs, dag ins, string opcodestr,
              string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatJ> {
  bits<20> imm20;
  bits<5> rd;

  let Inst{31} = imm20{19};
  let Inst{30-21} = imm20{9-0};
  let Inst{20} = imm20{10};
  let Inst{19-12} = imm20{18-11};
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode.Value;
}

//===----------------------------------------------------------------------===//
// Instruction classes for .insn directives
//===----------------------------------------------------------------------===//

class DirectiveInsnR<dag outs, dag ins, string argstr>
  : RVInst<outs, ins, "", "", [], InstFormatR> {
  bits<7> opcode;
  bits<7> funct7;
  bits<3> funct3;

  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-25} = funct7;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode;

  let AsmString = ".insn r " # argstr;
}

class DirectiveInsnR4<dag outs, dag ins, string argstr>
  : RVInst<outs, ins, "", "", [], InstFormatR4> {
  bits<7> opcode;
  bits<2> funct2;
  bits<3> funct3;

  bits<5> rs3;
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-27} = rs3;
  let Inst{26-25} = funct2;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode;

  let AsmString = ".insn r4 " # argstr;
}

class DirectiveInsnI<dag outs, dag ins, string argstr>
  : RVInst<outs, ins, "", "", [], InstFormatI> {
  bits<7> opcode;
  bits<3> funct3;

  bits<12> imm12;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-20} = imm12;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode;

  let AsmString = ".insn i " # argstr;
}

class DirectiveInsnS<dag outs, dag ins, string argstr>
  : RVInst<outs, ins, "", "", [], InstFormatS> {
  bits<7> opcode;
  bits<3> funct3;

  bits<12> imm12;
  bits<5> rs2;
  bits<5> rs1;

  let Inst{31-25} = imm12{11-5};
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = imm12{4-0};
  let Inst{6-0} = opcode;

  let AsmString = ".insn s " # argstr;
}

class DirectiveInsnB<dag outs, dag ins, string argstr>
  : RVInst<outs, ins, "", "", [], InstFormatB> {
  bits<7> opcode;
  bits<3> funct3;

  bits<12> imm12;
  bits<5> rs2;
  bits<5> rs1;

  let Inst{31} = imm12{11};
  let Inst{30-25} = imm12{9-4};
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-8} = imm12{3-0};
  let Inst{7} = imm12{10};
  let Inst{6-0} = opcode;

  let AsmString = ".insn b " # argstr;
}

class DirectiveInsnU<dag outs, dag ins, string argstr>
  : RVInst<outs, ins, "", "", [], InstFormatU> {
  bits<7> opcode;

  bits<20> imm20;
  bits<5> rd;

  let Inst{31-12} = imm20;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode;

  let AsmString = ".insn u " # argstr;
}

class DirectiveInsnJ<dag outs, dag ins, string argstr>
  : RVInst<outs, ins, "", "", [], InstFormatJ> {
  bits<7> opcode;

  bits<20> imm20;
  bits<5> rd;

  let Inst{31-12} = imm20;
  let Inst{11-7} = rd;
  let Inst{6-0} = opcode;

  let AsmString = ".insn j " # argstr;
}
