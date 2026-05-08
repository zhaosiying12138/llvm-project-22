//===-- YSXRegisterInfo.td - RISC-V Register defs --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//  Declarations that describe the RISC-V register files
//===----------------------------------------------------------------------===//

let Namespace = "YSX" in {
class YSXReg<bits<5> Enc, string n, list<string> alt = []> : Register<n> {
  let HWEncoding{4-0} = Enc;
  let AltNames = alt;
}

class YSXRegWithSubRegs<bits<5> Enc, string n, list<Register> subregs,
                          list<string> alt = []>
      : RegisterWithSubRegs<n, subregs> {
  let HWEncoding{4-0} = Enc;
  let AltNames = alt;
}

def sub_16 : SubRegIndex<16>;
def sub_32 : SubRegIndex<32>;

let FallbackRegAltNameIndex = NoRegAltName in
def ABIRegAltName : RegAltNameIndex;

def sub_gpr_even : SubRegIndex<32> {
  let SubRegRanges =
      SubRegRangeByHwMode<[DefaultMode], [SubRegRange<64>]>;
}
def sub_gpr_odd  : SubRegIndex<32, 32> {
  let SubRegRanges =
      SubRegRangeByHwMode<[DefaultMode], [SubRegRange<64, 64>]>;
}
} // Namespace = "YSX"

//===----------------------------------------------------------------------===//
// General Purpose Registers (aka Integer Registers)
//===----------------------------------------------------------------------===//

let RegAltNameIndices = [ABIRegAltName] in {
  // 16-bit sub-registers for use by Zhinx. Having a 16-bit sub-register reduces
  // the spill size for these operations.
  let isConstant = true in
  def X0_H  : YSXReg<0, "x0", ["zero"]>;
  let CostPerUse = [0, 1] in {
  def X1_H  : YSXReg<1, "x1", ["ra"]>;
  def X2_H  : YSXReg<2, "x2", ["sp"]>;
  def X3_H  : YSXReg<3, "x3", ["gp"]>;
  def X4_H  : YSXReg<4, "x4", ["tp"]>;
  def X5_H  : YSXReg<5, "x5", ["t0"]>;
  def X6_H  : YSXReg<6, "x6", ["t1"]>;
  def X7_H  : YSXReg<7, "x7", ["t2"]>;
  }
  def X8_H  : YSXReg<8, "x8", ["s0", "fp"]>;
  def X9_H  : YSXReg<9, "x9", ["s1"]>;
  def X10_H : YSXReg<10,"x10", ["a0"]>;
  def X11_H : YSXReg<11,"x11", ["a1"]>;
  def X12_H : YSXReg<12,"x12", ["a2"]>;
  def X13_H : YSXReg<13,"x13", ["a3"]>;
  def X14_H : YSXReg<14,"x14", ["a4"]>;
  def X15_H : YSXReg<15,"x15", ["a5"]>;
  let CostPerUse = [0, 1] in {
  def X16_H : YSXReg<16,"x16", ["a6"]>;
  def X17_H : YSXReg<17,"x17", ["a7"]>;
  def X18_H : YSXReg<18,"x18", ["s2"]>;
  def X19_H : YSXReg<19,"x19", ["s3"]>;
  def X20_H : YSXReg<20,"x20", ["s4"]>;
  def X21_H : YSXReg<21,"x21", ["s5"]>;
  def X22_H : YSXReg<22,"x22", ["s6"]>;
  def X23_H : YSXReg<23,"x23", ["s7"]>;
  def X24_H : YSXReg<24,"x24", ["s8"]>;
  def X25_H : YSXReg<25,"x25", ["s9"]>;
  def X26_H : YSXReg<26,"x26", ["s10"]>;
  def X27_H : YSXReg<27,"x27", ["s11"]>;
  def X28_H : YSXReg<28,"x28", ["t3"]>;
  def X29_H : YSXReg<29,"x29", ["t4"]>;
  def X30_H : YSXReg<30,"x30", ["t5"]>;
  def X31_H : YSXReg<31,"x31", ["t6"]>;
  }

  let SubRegIndices = [sub_16] in {
  let isConstant = true in
  def X0_W  : YSXRegWithSubRegs<0, "x0", [X0_H], ["zero"]>;
  let CostPerUse = [0, 1] in {
  def X1_W  : YSXRegWithSubRegs<1, "x1", [X1_H], ["ra"]>;
  def X2_W  : YSXRegWithSubRegs<2, "x2", [X2_H], ["sp"]>;
  def X3_W  : YSXRegWithSubRegs<3, "x3", [X3_H], ["gp"]>;
  def X4_W  : YSXRegWithSubRegs<4, "x4", [X4_H], ["tp"]>;
  def X5_W  : YSXRegWithSubRegs<5, "x5", [X5_H], ["t0"]>;
  def X6_W  : YSXRegWithSubRegs<6, "x6", [X6_H], ["t1"]>;
  def X7_W  : YSXRegWithSubRegs<7, "x7", [X7_H], ["t2"]>;
  }
  def X8_W  : YSXRegWithSubRegs<8, "x8", [X8_H], ["s0", "fp"]>;
  def X9_W  : YSXRegWithSubRegs<9, "x9", [X9_H], ["s1"]>;
  def X10_W : YSXRegWithSubRegs<10,"x10", [X10_H], ["a0"]>;
  def X11_W : YSXRegWithSubRegs<11,"x11", [X11_H], ["a1"]>;
  def X12_W : YSXRegWithSubRegs<12,"x12", [X12_H], ["a2"]>;
  def X13_W : YSXRegWithSubRegs<13,"x13", [X13_H], ["a3"]>;
  def X14_W : YSXRegWithSubRegs<14,"x14", [X14_H], ["a4"]>;
  def X15_W : YSXRegWithSubRegs<15,"x15", [X15_H], ["a5"]>;
  let CostPerUse = [0, 1] in {
  def X16_W : YSXRegWithSubRegs<16,"x16", [X16_H], ["a6"]>;
  def X17_W : YSXRegWithSubRegs<17,"x17", [X17_H], ["a7"]>;
  def X18_W : YSXRegWithSubRegs<18,"x18", [X18_H], ["s2"]>;
  def X19_W : YSXRegWithSubRegs<19,"x19", [X19_H], ["s3"]>;
  def X20_W : YSXRegWithSubRegs<20,"x20", [X20_H], ["s4"]>;
  def X21_W : YSXRegWithSubRegs<21,"x21", [X21_H], ["s5"]>;
  def X22_W : YSXRegWithSubRegs<22,"x22", [X22_H], ["s6"]>;
  def X23_W : YSXRegWithSubRegs<23,"x23", [X23_H], ["s7"]>;
  def X24_W : YSXRegWithSubRegs<24,"x24", [X24_H], ["s8"]>;
  def X25_W : YSXRegWithSubRegs<25,"x25", [X25_H], ["s9"]>;
  def X26_W : YSXRegWithSubRegs<26,"x26", [X26_H], ["s10"]>;
  def X27_W : YSXRegWithSubRegs<27,"x27", [X27_H], ["s11"]>;
  def X28_W : YSXRegWithSubRegs<28,"x28", [X28_H], ["t3"]>;
  def X29_W : YSXRegWithSubRegs<29,"x29", [X29_H], ["t4"]>;
  def X30_W : YSXRegWithSubRegs<30,"x30", [X30_H], ["t5"]>;
  def X31_W : YSXRegWithSubRegs<31,"x31", [X31_H], ["t6"]>;
  }
  }

  let SubRegIndices = [sub_32] in {
  let isConstant = true in
  def X0  : YSXRegWithSubRegs<0, "x0", [X0_W], ["zero"]>, DwarfRegNum<[0]>;
  let CostPerUse = [0, 1] in {
  def X1  : YSXRegWithSubRegs<1, "x1", [X1_W], ["ra"]>, DwarfRegNum<[1]>;
  def X2  : YSXRegWithSubRegs<2, "x2", [X2_W], ["sp"]>, DwarfRegNum<[2]>;
  def X3  : YSXRegWithSubRegs<3, "x3", [X3_W], ["gp"]>, DwarfRegNum<[3]>;
  def X4  : YSXRegWithSubRegs<4, "x4", [X4_W], ["tp"]>, DwarfRegNum<[4]>;
  def X5  : YSXRegWithSubRegs<5, "x5", [X5_W], ["t0"]>, DwarfRegNum<[5]>;
  def X6  : YSXRegWithSubRegs<6, "x6", [X6_W], ["t1"]>, DwarfRegNum<[6]>;
  def X7  : YSXRegWithSubRegs<7, "x7", [X7_W], ["t2"]>, DwarfRegNum<[7]>;
  }
  def X8  : YSXRegWithSubRegs<8, "x8", [X8_W], ["s0", "fp"]>, DwarfRegNum<[8]>;
  def X9  : YSXRegWithSubRegs<9, "x9", [X9_W], ["s1"]>, DwarfRegNum<[9]>;
  def X10 : YSXRegWithSubRegs<10,"x10", [X10_W], ["a0"]>, DwarfRegNum<[10]>;
  def X11 : YSXRegWithSubRegs<11,"x11", [X11_W], ["a1"]>, DwarfRegNum<[11]>;
  def X12 : YSXRegWithSubRegs<12,"x12", [X12_W], ["a2"]>, DwarfRegNum<[12]>;
  def X13 : YSXRegWithSubRegs<13,"x13", [X13_W], ["a3"]>, DwarfRegNum<[13]>;
  def X14 : YSXRegWithSubRegs<14,"x14", [X14_W], ["a4"]>, DwarfRegNum<[14]>;
  def X15 : YSXRegWithSubRegs<15,"x15", [X15_W], ["a5"]>, DwarfRegNum<[15]>;
  let CostPerUse = [0, 1] in {
  def X16 : YSXRegWithSubRegs<16,"x16", [X16_W], ["a6"]>, DwarfRegNum<[16]>;
  def X17 : YSXRegWithSubRegs<17,"x17", [X17_W], ["a7"]>, DwarfRegNum<[17]>;
  def X18 : YSXRegWithSubRegs<18,"x18", [X18_W], ["s2"]>, DwarfRegNum<[18]>;
  def X19 : YSXRegWithSubRegs<19,"x19", [X19_W], ["s3"]>, DwarfRegNum<[19]>;
  def X20 : YSXRegWithSubRegs<20,"x20", [X20_W], ["s4"]>, DwarfRegNum<[20]>;
  def X21 : YSXRegWithSubRegs<21,"x21", [X21_W], ["s5"]>, DwarfRegNum<[21]>;
  def X22 : YSXRegWithSubRegs<22,"x22", [X22_W], ["s6"]>, DwarfRegNum<[22]>;
  def X23 : YSXRegWithSubRegs<23,"x23", [X23_W], ["s7"]>, DwarfRegNum<[23]>;
  def X24 : YSXRegWithSubRegs<24,"x24", [X24_W], ["s8"]>, DwarfRegNum<[24]>;
  def X25 : YSXRegWithSubRegs<25,"x25", [X25_W], ["s9"]>, DwarfRegNum<[25]>;
  def X26 : YSXRegWithSubRegs<26,"x26", [X26_W], ["s10"]>, DwarfRegNum<[26]>;
  def X27 : YSXRegWithSubRegs<27,"x27", [X27_W], ["s11"]>, DwarfRegNum<[27]>;
  def X28 : YSXRegWithSubRegs<28,"x28", [X28_W], ["t3"]>, DwarfRegNum<[28]>;
  def X29 : YSXRegWithSubRegs<29,"x29", [X29_W], ["t4"]>, DwarfRegNum<[29]>;
  def X30 : YSXRegWithSubRegs<30,"x30", [X30_W], ["t5"]>, DwarfRegNum<[30]>;
  def X31 : YSXRegWithSubRegs<31,"x31", [X31_W], ["t6"]>, DwarfRegNum<[31]>;
  }
  }
}

defvar XLenVT = i64;
defvar XLenPairVT = untyped;

def XLenRI : RegInfoByHwMode<[DefaultMode], [RegInfo<64,64,64>]>;

class YSXRegisterClass<list<ValueType> regTypes, int align, dag regList>
    : RegisterClass<"YSX", regTypes, align, regList> {
  bit IsVRegClass = 0;
  int VLMul = 1;
  int NF = 1;

  let TSFlags{0} = IsVRegClass;
  let TSFlags{2-1} = !logtwo(VLMul);
  let TSFlags{5-3} = !sub(NF, 1);
}

class GPRRegisterClass<dag regList>
    : YSXRegisterClass<[XLenVT, i32], 32, regList> {
  let RegInfos = XLenRI;
}

// The order of registers represents the preferred allocation sequence.
// Registers are listed in the order caller-save, callee-save, specials.
def GPR : GPRRegisterClass<(add (sequence "X%u", 10, 17),
                                (sequence "X%u", 5, 7),
                                (sequence "X%u", 28, 31),
                                (sequence "X%u", 8, 9),
                                (sequence "X%u", 18, 27),
                                (sequence "X%u", 0, 4))>;

def GPRX0 : GPRRegisterClass<(add X0)>;

def GPRX1 : GPRRegisterClass<(add X1)> {
  let DiagnosticType = "InvalidRegClassGPRX1";
  let DiagnosticString = "register must be ra (x1)";
}

def GPRX5 : GPRRegisterClass<(add X5)> {
  let DiagnosticType = "InvalidRegClassGPRX5";
  let DiagnosticString = "register must be t0 (x5)";
}

def GPRNoX0 : GPRRegisterClass<(sub GPR, X0)> {
  let DiagnosticType = "InvalidRegClassGPRNoX0";
  let DiagnosticString = "register must be a GPR excluding zero (x0)";
}

def GPRNoX2 : GPRRegisterClass<(sub GPR, X2)> {
  let DiagnosticType = "InvalidRegClassGPRNoX2";
  let DiagnosticString = "register must be a GPR excluding sp (x2)";
}

def GPRNoX0X2 : GPRRegisterClass<(sub GPR, X0, X2)> {
  let DiagnosticType = "InvalidRegClassGPRNoX0X2";
  let DiagnosticString = "register must be a GPR excluding zero (x0) and sp (x2)";
}

def GPRX7 : GPRRegisterClass<(add X7)>;

// Don't use X1 or X5 for JALR since that is a hint to pop the return address
// stack on some microarchitectures. Also remove the reserved registers X0, X2,
// X3, and X4 as it reduces the number of register classes that get synthesized
// by tablegen.
def GPRJALR : GPRRegisterClass<(sub GPR, (sequence "X%u", 0, 5))>;

def GPRJALRNonX7 : GPRRegisterClass<(sub GPRJALR, X7)>;

def GPRC : GPRRegisterClass<(add (sequence "X%u", 10, 15),
                                 (sequence "X%u", 8, 9))>;

// For indirect tail calls, we can't use callee-saved registers, as they are
// restored to the saved value before the tail call, which would clobber a call
// address. We shouldn't use x5 since that is a hint for to pop the return
// address stack on some microarchitectures.
def GPRTC : GPRRegisterClass<(add (sequence "X%u", 6, 7),
                                  (sequence "X%u", 10, 17),
                                  (sequence "X%u", 28, 31))>;
def GPRTCNonX7 : GPRRegisterClass<(sub GPRTC, X7)>;

def SP : GPRRegisterClass<(add X2)> {
  let DiagnosticType = "InvalidRegClassSP";
  let DiagnosticString = "register must be sp (x2)";
}

def GPRX1X5 :  GPRRegisterClass<(add X1, X5)> {
  let DiagnosticType = "InvalidRegClassGPRX1X5";
  let DiagnosticString = "register must be ra or t0 (x1 or x5)";
}

def GPRNoX31 : GPRRegisterClass<(sub GPR, X31)> {
  let DiagnosticType = "InvalidRegClassGPRX31";
  let DiagnosticString = "register must be a GPR excluding t6 (x31)";
}

//===----------------------------------------------------------------------===//
// Even-Odd GPR Pairs
//===----------------------------------------------------------------------===//

def XLenPairRI
    : RegInfoByHwMode<[DefaultMode], [RegInfo<128, 128, 64>]>;

// Dummy zero register for use in the register pair containing X0 (as X1 is
// not read to or written when the X0 register pair is used).
def DUMMY_REG_PAIR_WITH_X0 : YSXReg<0, "0">;

// Must add DUMMY_REG_PAIR_WITH_X0 to a separate register class to prevent the
// register's existence from changing codegen (due to the regPressureSetLimit
// for the GPR register class being altered).
def GPRAll : GPRRegisterClass<(add GPR, DUMMY_REG_PAIR_WITH_X0)>;

let RegAltNameIndices = [ABIRegAltName] in {
  def X0_Pair : YSXRegWithSubRegs<0, X0.AsmName,
                                    [X0, DUMMY_REG_PAIR_WITH_X0],
                                    X0.AltNames> {
    let SubRegIndices = [sub_gpr_even, sub_gpr_odd];
    let CoveredBySubRegs = 1;
  }
  foreach I = 1-15 in {
    defvar Index = !shl(I, 1);
    defvar IndexP1 = !add(Index, 1);
    defvar Reg = !cast<Register>("X"#Index);
    defvar RegP1 = !cast<Register>("X"#IndexP1);
    def "X" # Index #"_X" # IndexP1 : YSXRegWithSubRegs<Index,
                                                          Reg.AsmName,
                                                          [Reg, RegP1],
                                                          Reg.AltNames> {
      let SubRegIndices = [sub_gpr_even, sub_gpr_odd];
      let CoveredBySubRegs = 1;
    }
  }
}

let RegInfos = XLenPairRI, CopyCost = 2 in {
def GPRPair : YSXRegisterClass<[XLenPairVT], 64, (add
    X10_X11, X12_X13, X14_X15, X16_X17,
    X6_X7,
    X28_X29, X30_X31,
    X8_X9,
    X18_X19, X20_X21, X22_X23, X24_X25, X26_X27,
    X0_Pair, X2_X3, X4_X5
)>;

def GPRPairNoX0 : YSXRegisterClass<[XLenPairVT], 64, (sub GPRPair, X0_Pair)>;

def GPRPairC : YSXRegisterClass<[XLenPairVT], 64, (add
  X10_X11, X12_X13, X14_X15, X8_X9
)>;
} // let RegInfos = XLenPairRI, CopyCost = 2

//===----------------------------------------------------------------------===//
// Floating-Point Registers
//===----------------------------------------------------------------------===//

let RegAltNameIndices = [ABIRegAltName] in {
  def F0_F  : YSXReg<0,  "f0",  ["ft0"]>,  DwarfRegNum<[32]>;
  def F1_F  : YSXReg<1,  "f1",  ["ft1"]>,  DwarfRegNum<[33]>;
  def F2_F  : YSXReg<2,  "f2",  ["ft2"]>,  DwarfRegNum<[34]>;
  def F3_F  : YSXReg<3,  "f3",  ["ft3"]>,  DwarfRegNum<[35]>;
  def F4_F  : YSXReg<4,  "f4",  ["ft4"]>,  DwarfRegNum<[36]>;
  def F5_F  : YSXReg<5,  "f5",  ["ft5"]>,  DwarfRegNum<[37]>;
  def F6_F  : YSXReg<6,  "f6",  ["ft6"]>,  DwarfRegNum<[38]>;
  def F7_F  : YSXReg<7,  "f7",  ["ft7"]>,  DwarfRegNum<[39]>;
  def F8_F  : YSXReg<8,  "f8",  ["fs0"]>,  DwarfRegNum<[40]>;
  def F9_F  : YSXReg<9,  "f9",  ["fs1"]>,  DwarfRegNum<[41]>;
  def F10_F : YSXReg<10, "f10", ["fa0"]>,  DwarfRegNum<[42]>;
  def F11_F : YSXReg<11, "f11", ["fa1"]>,  DwarfRegNum<[43]>;
  def F12_F : YSXReg<12, "f12", ["fa2"]>,  DwarfRegNum<[44]>;
  def F13_F : YSXReg<13, "f13", ["fa3"]>,  DwarfRegNum<[45]>;
  def F14_F : YSXReg<14, "f14", ["fa4"]>,  DwarfRegNum<[46]>;
  def F15_F : YSXReg<15, "f15", ["fa5"]>,  DwarfRegNum<[47]>;
  def F16_F : YSXReg<16, "f16", ["fa6"]>,  DwarfRegNum<[48]>;
  def F17_F : YSXReg<17, "f17", ["fa7"]>,  DwarfRegNum<[49]>;
  def F18_F : YSXReg<18, "f18", ["fs2"]>,  DwarfRegNum<[50]>;
  def F19_F : YSXReg<19, "f19", ["fs3"]>,  DwarfRegNum<[51]>;
  def F20_F : YSXReg<20, "f20", ["fs4"]>,  DwarfRegNum<[52]>;
  def F21_F : YSXReg<21, "f21", ["fs5"]>,  DwarfRegNum<[53]>;
  def F22_F : YSXReg<22, "f22", ["fs6"]>,  DwarfRegNum<[54]>;
  def F23_F : YSXReg<23, "f23", ["fs7"]>,  DwarfRegNum<[55]>;
  def F24_F : YSXReg<24, "f24", ["fs8"]>,  DwarfRegNum<[56]>;
  def F25_F : YSXReg<25, "f25", ["fs9"]>,  DwarfRegNum<[57]>;
  def F26_F : YSXReg<26, "f26", ["fs10"]>, DwarfRegNum<[58]>;
  def F27_F : YSXReg<27, "f27", ["fs11"]>, DwarfRegNum<[59]>;
  def F28_F : YSXReg<28, "f28", ["ft8"]>,  DwarfRegNum<[60]>;
  def F29_F : YSXReg<29, "f29", ["ft9"]>,  DwarfRegNum<[61]>;
  def F30_F : YSXReg<30, "f30", ["ft10"]>, DwarfRegNum<[62]>;
  def F31_F : YSXReg<31, "f31", ["ft11"]>, DwarfRegNum<[63]>;
}

def FPR32 : YSXRegisterClass<[f32], 32, (add
    (sequence "F%u_F", 15, 10),
    (sequence "F%u_F", 0, 7),
    (sequence "F%u_F", 16, 17),
    (sequence "F%u_F", 28, 31),
    (sequence "F%u_F", 8, 9),
    (sequence "F%u_F", 18, 27)
)>;

//===----------------------------------------------------------------------===//
// Tiny Vector Registers
//===----------------------------------------------------------------------===//

foreach Index = !range(0, 32, 1) in
  def V#Index : YSXReg<Index, "v"#Index>, DwarfRegNum<[!add(Index, 96)]>;

def VTYPE : YSXReg<0, "vtype">;
def VL    : YSXReg<0, "vl">;

defvar YSXVecM1VTs = [nxv2i32, nxv2f32, v4i32, v4f32];
defvar YSXVecMaskVTs = [nxv2i1];

class VReg<list<ValueType> regTypes, dag regList>
    : YSXRegisterClass<regTypes, 128, regList> {
  let IsVRegClass = 1;
  let VLMul = 1;
  let NF = 1;
  let Size = 128;
  let CopyCost = 1;
}

def VR : VReg<!listconcat(YSXVecM1VTs, YSXVecMaskVTs),
              (add (sequence "V%u", 8, 31),
                   (sequence "V%u", 7, 0))>;

def VRNoV0 : VReg<!listconcat(YSXVecM1VTs, YSXVecMaskVTs), (sub VR, V0)>;

def VMV0 : VReg<YSXVecMaskVTs, (add V0)>;

def VCSR : YSXRegisterClass<[XLenVT], 32, (add VTYPE, VL)> {
  let RegInfos = XLenRI;
  let isAllocatable = 0;
}
