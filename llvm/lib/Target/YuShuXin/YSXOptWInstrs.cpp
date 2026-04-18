//===- YSXOptWInstrs.cpp - MI W instruction optimizations ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//
//
// This pass does some optimizations for *W instructions at the MI level.
//
// First it removes unneeded sext.w instructions. Either because the sign
// extended bits aren't consumed or because the input was already sign extended
// by an earlier instruction.
//
// Then:
// 1. Unless explicit disabled or the target prefers instructions with W suffix,
//    it removes the -w suffix from opw instructions whenever all users are
//    dependent only on the lower word of the result of the instruction.
//    The cases handled are:
//    * addw because c.add has a larger register encoding than c.addw.
//    * addiw because it helps reduce test differences between RV32 and RV64
//      w/o being a pessimization.
//    * mulw because c.mulw doesn't exist but c.mul does (w/ zcb)
//    * slliw because c.slliw doesn't exist and c.slli does
//
// 2. Or if explicit enabled or the target prefers instructions with W suffix,
//    it adds the W suffix to the instruction whenever all users are dependent
//    only on the lower word of the result of the instruction.
//    The cases handled are:
//    * add/addi/sub/mul.
//    * slli with imm < 32.
//    * ld/lwu.
//===---------------------------------------------------------------------===//

#include "YSX.h"
#include "YSXMachineFunctionInfo.h"
#include "YSXSubtarget.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

using namespace llvm;

#define DEBUG_TYPE "ysx-opt-w-instrs"
#define YSX_OPT_W_INSTRS_NAME "RISC-V Optimize W Instructions"

STATISTIC(NumRemovedSExtW, "Number of removed sign-extensions");
STATISTIC(NumTransformedToWInstrs,
          "Number of instructions transformed to W-ops");
STATISTIC(NumTransformedToNonWInstrs,
          "Number of instructions transformed to non-W-ops");

static cl::opt<bool> DisableSExtWRemoval("ysx-disable-sextw-removal",
                                         cl::desc("Disable removal of sext.w"),
                                         cl::init(false), cl::Hidden);
static cl::opt<bool> DisableStripWSuffix("ysx-disable-strip-w-suffix",
                                         cl::desc("Disable strip W suffix"),
                                         cl::init(false), cl::Hidden);

namespace {

class YSXOptWInstrs : public MachineFunctionPass {
public:
  static char ID;

  YSXOptWInstrs() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;
  bool removeSExtWInstrs(MachineFunction &MF, const YSXInstrInfo &TII,
                         const YSXSubtarget &ST, MachineRegisterInfo &MRI);
  bool canonicalizeWSuffixes(MachineFunction &MF, const YSXInstrInfo &TII,
                             const YSXSubtarget &ST,
                             MachineRegisterInfo &MRI);

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override { return YSX_OPT_W_INSTRS_NAME; }
};

} // end anonymous namespace

char YSXOptWInstrs::ID = 0;
INITIALIZE_PASS(YSXOptWInstrs, DEBUG_TYPE, YSX_OPT_W_INSTRS_NAME, false,
                false)

FunctionPass *llvm::createYSXOptWInstrsPass() {
  return new YSXOptWInstrs();
}

static bool vectorPseudoHasAllNBitUsers(const MachineOperand &UserOp,
                                        unsigned Bits) {
  const MachineInstr &MI = *UserOp.getParent();
  unsigned MCOpcode = YSX::getRVVMCOpcode(MI.getOpcode());

  if (!MCOpcode)
    return false;

  const MCInstrDesc &MCID = MI.getDesc();
  const uint64_t TSFlags = MCID.TSFlags;
  if (!YSXII::hasSEWOp(TSFlags))
    return false;
  assert(YSXII::hasVLOp(TSFlags));
  const unsigned Log2SEW = MI.getOperand(YSXII::getSEWOpNum(MCID)).getImm();

  if (UserOp.getOperandNo() == YSXII::getVLOpNum(MCID))
    return false;

  auto NumDemandedBits =
      YSX::getVectorLowDemandedScalarBits(MCOpcode, Log2SEW);
  return NumDemandedBits && Bits >= *NumDemandedBits;
}

// Checks if all users only demand the lower \p OrigBits of the original
// instruction's result.
// TODO: handle multiple interdependent transformations
static bool hasAllNBitUsers(const MachineInstr &OrigMI,
                            const YSXSubtarget &ST,
                            const MachineRegisterInfo &MRI, unsigned OrigBits) {

  SmallSet<std::pair<const MachineInstr *, unsigned>, 4> Visited;
  SmallVector<std::pair<const MachineInstr *, unsigned>, 4> Worklist;

  Worklist.emplace_back(&OrigMI, OrigBits);

  while (!Worklist.empty()) {
    auto P = Worklist.pop_back_val();
    const MachineInstr *MI = P.first;
    unsigned Bits = P.second;

    if (!Visited.insert(P).second)
      continue;

    // Only handle instructions with one def.
    if (MI->getNumExplicitDefs() != 1)
      return false;

    Register DestReg = MI->getOperand(0).getReg();
    if (!DestReg.isVirtual())
      return false;

    for (auto &UserOp : MRI.use_nodbg_operands(DestReg)) {
      const MachineInstr *UserMI = UserOp.getParent();
      unsigned OpIdx = UserOp.getOperandNo();

      switch (UserMI->getOpcode()) {
      default:
        if (vectorPseudoHasAllNBitUsers(UserOp, Bits))
          break;
        return false;

      case YSX::ADDIW:
      case YSX::ADDW:
      case YSX::DIVUW:
      case YSX::DIVW:
      case YSX::MULW:
      case YSX::REMUW:
      case YSX::REMW:
      case YSX::SLLW:
      case YSX::SRAIW:
      case YSX::SRAW:
      case YSX::SRLIW:
      case YSX::SRLW:
      case YSX::SUBW:
      case YSX::ROLW:
      case YSX::RORW:
      case YSX::RORIW:
      case YSX::CLSW:
      case YSX::CLZW:
      case YSX::CTZW:
      case YSX::CPOPW:
      case YSX::SLLI_UW:
      case YSX::ABSW:
      case YSX::FMV_W_X:
      case YSX::FCVT_H_W:
      case YSX::FCVT_H_W_INX:
      case YSX::FCVT_H_WU:
      case YSX::FCVT_H_WU_INX:
      case YSX::FCVT_S_W:
      case YSX::FCVT_S_W_INX:
      case YSX::FCVT_S_WU:
      case YSX::FCVT_S_WU_INX:
      case YSX::FCVT_D_W:
      case YSX::FCVT_D_W_INX:
      case YSX::FCVT_D_WU:
      case YSX::FCVT_D_WU_INX:
        if (Bits >= 32)
          break;
        return false;

      case YSX::SEXT_B:
      case YSX::PACKH:
        if (Bits >= 8)
          break;
        return false;
      case YSX::SEXT_H:
      case YSX::FMV_H_X:
      case YSX::ZEXT_H_RV32:
      case YSX::ZEXT_H_RV64:
      case YSX::PACKW:
        if (Bits >= 16)
          break;
        return false;

      case YSX::PACK:
        if (Bits >= (ST.getXLen() / 2))
          break;
        return false;

      case YSX::SRLI: {
        // If we are shifting right by less than Bits, and users don't demand
        // any bits that were shifted into [Bits-1:0], then we can consider this
        // as an N-Bit user.
        unsigned ShAmt = UserMI->getOperand(2).getImm();
        if (Bits > ShAmt) {
          Worklist.emplace_back(UserMI, Bits - ShAmt);
          break;
        }
        return false;
      }

      // these overwrite higher input bits, otherwise the lower word of output
      // depends only on the lower word of input. So check their uses read W.
      case YSX::SLLI: {
        unsigned ShAmt = UserMI->getOperand(2).getImm();
        if (Bits >= (ST.getXLen() - ShAmt))
          break;
        Worklist.emplace_back(UserMI, Bits + ShAmt);
        break;
      }
      case YSX::SLLIW: {
        unsigned ShAmt = UserMI->getOperand(2).getImm();
        if (Bits >= 32 - ShAmt)
          break;
        Worklist.emplace_back(UserMI, Bits + ShAmt);
        break;
      }

      case YSX::ANDI: {
        uint64_t Imm = UserMI->getOperand(2).getImm();
        if (Bits >= (unsigned)llvm::bit_width(Imm))
          break;
        Worklist.emplace_back(UserMI, Bits);
        break;
      }
      case YSX::ORI: {
        uint64_t Imm = UserMI->getOperand(2).getImm();
        if (Bits >= (unsigned)llvm::bit_width<uint64_t>(~Imm))
          break;
        Worklist.emplace_back(UserMI, Bits);
        break;
      }

      case YSX::SLL:
      case YSX::BSET:
      case YSX::BCLR:
      case YSX::BINV:
        // Operand 2 is the shift amount which uses log2(xlen) bits.
        if (OpIdx == 2) {
          if (Bits >= Log2_32(ST.getXLen()))
            break;
          return false;
        }
        Worklist.emplace_back(UserMI, Bits);
        break;

      case YSX::SRA:
      case YSX::SRL:
      case YSX::ROL:
      case YSX::ROR:
        // Operand 2 is the shift amount which uses 6 bits.
        if (OpIdx == 2 && Bits >= Log2_32(ST.getXLen()))
          break;
        return false;

      case YSX::ADD_UW:
      case YSX::SH1ADD_UW:
      case YSX::SH2ADD_UW:
      case YSX::SH3ADD_UW:
        // Operand 1 is implicitly zero extended.
        if (OpIdx == 1 && Bits >= 32)
          break;
        Worklist.emplace_back(UserMI, Bits);
        break;

      case YSX::BEXTI:
        if (UserMI->getOperand(2).getImm() >= Bits)
          return false;
        break;

      case YSX::SB:
        // The first argument is the value to store.
        if (OpIdx == 0 && Bits >= 8)
          break;
        return false;
      case YSX::SH:
        // The first argument is the value to store.
        if (OpIdx == 0 && Bits >= 16)
          break;
        return false;
      case YSX::SW:
        // The first argument is the value to store.
        if (OpIdx == 0 && Bits >= 32)
          break;
        return false;

      // For these, lower word of output in these operations, depends only on
      // the lower word of input. So, we check all uses only read lower word.
      case YSX::COPY:
      case YSX::PHI:

      case YSX::ADD:
      case YSX::ADDI:
      case YSX::AND:
      case YSX::MUL:
      case YSX::OR:
      case YSX::SUB:
      case YSX::XOR:
      case YSX::XORI:

      case YSX::ANDN:
      case YSX::CLMUL:
      case YSX::ORN:
      case YSX::SH1ADD:
      case YSX::SH2ADD:
      case YSX::SH3ADD:
      case YSX::XNOR:
      case YSX::BSETI:
      case YSX::BCLRI:
      case YSX::BINVI:
        Worklist.emplace_back(UserMI, Bits);
        break;

      case YSX::BREV8:
      case YSX::ORC_B:
        // BREV8 and ORC_B work on bytes. Round Bits down to the nearest byte.
        Worklist.emplace_back(UserMI, alignDown(Bits, 8));
        break;

      case YSX::PseudoCCMOVGPR:
      case YSX::PseudoCCMOVGPRNoX0:
        // Either operand 4 or operand 5 is returned by this instruction. If
        // only the lower word of the result is used, then only the lower word
        // of operand 4 and 5 is used.
        if (OpIdx != 4 && OpIdx != 5)
          return false;
        Worklist.emplace_back(UserMI, Bits);
        break;

      case YSX::CZERO_EQZ:
      case YSX::CZERO_NEZ:
      case YSX::VT_MASKC:
      case YSX::VT_MASKCN:
        if (OpIdx != 1)
          return false;
        Worklist.emplace_back(UserMI, Bits);
        break;
      case YSX::TH_EXT:
      case YSX::TH_EXTU:
        unsigned Msb = UserMI->getOperand(2).getImm();
        unsigned Lsb = UserMI->getOperand(3).getImm();
        // Behavior of Msb < Lsb is not well documented.
        if (Msb >= Lsb && Bits > Msb)
          break;
        return false;
      }
    }
  }

  return true;
}

static bool hasAllWUsers(const MachineInstr &OrigMI, const YSXSubtarget &ST,
                         const MachineRegisterInfo &MRI) {
  return hasAllNBitUsers(OrigMI, ST, MRI, 32);
}

// This function returns true if the machine instruction always outputs a value
// where bits 63:32 match bit 31.
static bool isSignExtendingOpW(const MachineInstr &MI, unsigned OpNo) {
  uint64_t TSFlags = MI.getDesc().TSFlags;

  // Instructions that can be determined from opcode are marked in tablegen.
  if (TSFlags & YSXII::IsSignExtendingOpWMask)
    return true;

  // Special cases that require checking operands.
  switch (MI.getOpcode()) {
  // shifting right sufficiently makes the value 32-bit sign-extended
  case YSX::SRAI:
    return MI.getOperand(2).getImm() >= 32;
  case YSX::SRLI:
    return MI.getOperand(2).getImm() > 32;
  // The LI pattern ADDI rd, X0, imm is sign extended.
  case YSX::ADDI:
    return MI.getOperand(1).isReg() && MI.getOperand(1).getReg() == YSX::X0;
  // An ANDI with an 11 bit immediate will zero bits 63:11.
  case YSX::ANDI:
    return isUInt<11>(MI.getOperand(2).getImm());
  // An ORI with an >11 bit immediate (negative 12-bit) will set bits 63:11.
  case YSX::ORI:
    return !isUInt<11>(MI.getOperand(2).getImm());
  // A bseti with X0 is sign extended if the immediate is less than 31.
  case YSX::BSETI:
    return MI.getOperand(2).getImm() < 31 &&
           MI.getOperand(1).getReg() == YSX::X0;
  // Copying from X0 produces zero.
  case YSX::COPY:
    return MI.getOperand(1).getReg() == YSX::X0;
  // Ignore the scratch register destination.
  case YSX::PseudoAtomicLoadNand32:
    return OpNo == 0;
  case YSX::PseudoVMV_X_S: {
    // vmv.x.s has at least 33 sign bits if log2(sew) <= 5.
    int64_t Log2SEW = MI.getOperand(2).getImm();
    assert(Log2SEW >= 3 && Log2SEW <= 6 && "Unexpected Log2SEW");
    return Log2SEW <= 5;
  }
  case YSX::TH_EXT: {
    unsigned Msb = MI.getOperand(2).getImm();
    unsigned Lsb = MI.getOperand(3).getImm();
    return Msb >= Lsb && (Msb - Lsb + 1) <= 32;
  }
  case YSX::TH_EXTU: {
    unsigned Msb = MI.getOperand(2).getImm();
    unsigned Lsb = MI.getOperand(3).getImm();
    return Msb >= Lsb && (Msb - Lsb + 1) < 32;
  }
  }

  return false;
}

static bool isSignExtendedW(Register SrcReg, const YSXSubtarget &ST,
                            const MachineRegisterInfo &MRI,
                            SmallPtrSetImpl<MachineInstr *> &FixableDef) {
  SmallSet<Register, 4> Visited;
  SmallVector<Register, 4> Worklist;

  auto AddRegToWorkList = [&](Register SrcReg) {
    if (!SrcReg.isVirtual())
      return false;
    Worklist.push_back(SrcReg);
    return true;
  };

  if (!AddRegToWorkList(SrcReg))
    return false;

  while (!Worklist.empty()) {
    Register Reg = Worklist.pop_back_val();

    // If we already visited this register, we don't need to check it again.
    if (!Visited.insert(Reg).second)
      continue;

    MachineInstr *MI = MRI.getVRegDef(Reg);
    if (!MI)
      continue;

    int OpNo = MI->findRegisterDefOperandIdx(Reg, /*TRI=*/nullptr);
    assert(OpNo != -1 && "Couldn't find register");

    // If this is a sign extending operation we don't need to look any further.
    if (isSignExtendingOpW(*MI, OpNo))
      continue;

    // Is this an instruction that propagates sign extend?
    switch (MI->getOpcode()) {
    default:
      // Unknown opcode, give up.
      return false;
    case YSX::COPY: {
      const MachineFunction *MF = MI->getMF();
      const YSXMachineFunctionInfo *RVFI =
          MF->getInfo<YSXMachineFunctionInfo>();

      // If this is the entry block and the register is livein, see if we know
      // it is sign extended.
      if (MI->getParent() == &MF->front()) {
        Register VReg = MI->getOperand(0).getReg();
        if (MF->getRegInfo().isLiveIn(VReg) && RVFI->isSExt32Register(VReg))
          continue;
      }

      Register CopySrcReg = MI->getOperand(1).getReg();
      if (CopySrcReg == YSX::X10) {
        // For a method return value, we check the ZExt/SExt flags in attribute.
        // We assume the following code sequence for method call.
        // PseudoCALL @bar, ...
        // ADJCALLSTACKUP 0, 0, implicit-def dead $x2, implicit $x2
        // %0:gpr = COPY $x10
        //
        // We use the PseudoCall to look up the IR function being called to find
        // its return attributes.
        const MachineBasicBlock *MBB = MI->getParent();
        auto II = MI->getIterator();
        if (II == MBB->instr_begin() ||
            (--II)->getOpcode() != YSX::ADJCALLSTACKUP)
          return false;

        const MachineInstr &CallMI = *(--II);
        if (!CallMI.isCall() || !CallMI.getOperand(0).isGlobal())
          return false;

        auto *CalleeFn =
            dyn_cast_if_present<Function>(CallMI.getOperand(0).getGlobal());
        if (!CalleeFn)
          return false;

        auto *IntTy = dyn_cast<IntegerType>(CalleeFn->getReturnType());
        if (!IntTy)
          return false;

        const AttributeSet &Attrs = CalleeFn->getAttributes().getRetAttrs();
        unsigned BitWidth = IntTy->getBitWidth();
        if ((BitWidth <= 32 && Attrs.hasAttribute(Attribute::SExt)) ||
            (BitWidth < 32 && Attrs.hasAttribute(Attribute::ZExt)))
          continue;
      }

      if (!AddRegToWorkList(CopySrcReg))
        return false;

      break;
    }

    // For these, we just need to check if the 1st operand is sign extended.
    case YSX::BCLRI:
    case YSX::BINVI:
    case YSX::BSETI:
      if (MI->getOperand(2).getImm() >= 31)
        return false;
      [[fallthrough]];
    case YSX::REM:
    case YSX::ANDI:
    case YSX::ORI:
    case YSX::XORI:
    case YSX::SRAI:
      // |Remainder| is always <= |Dividend|. If D is 32-bit, then so is R.
      // DIV doesn't work because of the edge case 0xf..f 8000 0000 / (long)-1
      // Logical operations use a sign extended 12-bit immediate.
      // Arithmetic shift right can only increase the number of sign bits.
      if (!AddRegToWorkList(MI->getOperand(1).getReg()))
        return false;

      break;
    case YSX::PseudoCCADDW:
    case YSX::PseudoCCADDIW:
    case YSX::PseudoCCSUBW:
    case YSX::PseudoCCSLLW:
    case YSX::PseudoCCSRLW:
    case YSX::PseudoCCSRAW:
    case YSX::PseudoCCSLLIW:
    case YSX::PseudoCCSRLIW:
    case YSX::PseudoCCSRAIW:
      // Returns operand 4 or an ADDW/SUBW/etc. of operands 5 and 6. We only
      // need to check if operand 4 is sign extended.
      if (!AddRegToWorkList(MI->getOperand(4).getReg()))
        return false;
      break;
    case YSX::REMU:
    case YSX::AND:
    case YSX::OR:
    case YSX::XOR:
    case YSX::ANDN:
    case YSX::ORN:
    case YSX::XNOR:
    case YSX::MAX:
    case YSX::MAXU:
    case YSX::MIN:
    case YSX::MINU:
    case YSX::PseudoCCMOVGPR:
    case YSX::PseudoCCMOVGPRNoX0:
    case YSX::PseudoCCAND:
    case YSX::PseudoCCOR:
    case YSX::PseudoCCXOR:
    case YSX::PseudoCCANDN:
    case YSX::PseudoCCORN:
    case YSX::PseudoCCXNOR:
    case YSX::PHI: {
      // If all incoming values are sign-extended, the output of AND, OR, XOR,
      // MIN, MAX, or PHI is also sign-extended.

      // The input registers for PHI are operand 1, 3, ...
      // The input registers for PseudoCCMOVGPR(NoX0) are 4 and 5.
      // The input registers for PseudoCCAND/OR/XOR are 4, 5, and 6.
      // The input registers for others are operand 1 and 2.
      unsigned B = 1, E = 3, D = 1;
      switch (MI->getOpcode()) {
      case YSX::PHI:
        E = MI->getNumOperands();
        D = 2;
        break;
      case YSX::PseudoCCMOVGPR:
      case YSX::PseudoCCMOVGPRNoX0:
        B = 4;
        E = 6;
        break;
      case YSX::PseudoCCAND:
      case YSX::PseudoCCOR:
      case YSX::PseudoCCXOR:
      case YSX::PseudoCCANDN:
      case YSX::PseudoCCORN:
      case YSX::PseudoCCXNOR:
        B = 4;
        E = 7;
        break;
       }

      for (unsigned I = B; I != E; I += D) {
        if (!MI->getOperand(I).isReg())
          return false;

        if (!AddRegToWorkList(MI->getOperand(I).getReg()))
          return false;
      }

      break;
    }

    case YSX::CZERO_EQZ:
    case YSX::CZERO_NEZ:
    case YSX::VT_MASKC:
    case YSX::VT_MASKCN:
      // Instructions return zero or operand 1. Result is sign extended if
      // operand 1 is sign extended.
      if (!AddRegToWorkList(MI->getOperand(1).getReg()))
        return false;
      break;

    case YSX::ADDI: {
      if (MI->getOperand(1).isReg() && MI->getOperand(1).getReg().isVirtual()) {
        if (MachineInstr *SrcMI = MRI.getVRegDef(MI->getOperand(1).getReg())) {
          if (SrcMI->getOpcode() == YSX::LUI &&
              SrcMI->getOperand(1).isImm()) {
            uint64_t Imm = SrcMI->getOperand(1).getImm();
            Imm = SignExtend64<32>(Imm << 12);
            Imm += (uint64_t)MI->getOperand(2).getImm();
            if (isInt<32>(Imm))
              continue;
          }
        }
      }

      if (hasAllWUsers(*MI, ST, MRI)) {
        FixableDef.insert(MI);
        break;
      }
      return false;
    }

    // With these opcode, we can "fix" them with the W-version
    // if we know all users of the result only rely on bits 31:0
    case YSX::SLLI:
      // SLLIW reads the lowest 5 bits, while SLLI reads lowest 6 bits
      if (MI->getOperand(2).getImm() >= 32)
        return false;
      [[fallthrough]];
    case YSX::ADD:
    case YSX::LD:
    case YSX::LWU:
    case YSX::MUL:
    case YSX::SUB:
      if (hasAllWUsers(*MI, ST, MRI)) {
        FixableDef.insert(MI);
        break;
      }
      return false;
    }
  }

  // If we get here, then every node we visited produces a sign extended value
  // or propagated sign extended values. So the result must be sign extended.
  return true;
}

static unsigned getWOp(unsigned Opcode) {
  switch (Opcode) {
  case YSX::ADDI:
    return YSX::ADDIW;
  case YSX::ADD:
    return YSX::ADDW;
  case YSX::LD:
  case YSX::LWU:
    return YSX::LW;
  case YSX::MUL:
    return YSX::MULW;
  case YSX::SLLI:
    return YSX::SLLIW;
  case YSX::SUB:
    return YSX::SUBW;
  default:
    llvm_unreachable("Unexpected opcode for replacement with W variant");
  }
}

bool YSXOptWInstrs::removeSExtWInstrs(MachineFunction &MF,
                                        const YSXInstrInfo &TII,
                                        const YSXSubtarget &ST,
                                        MachineRegisterInfo &MRI) {
  if (DisableSExtWRemoval)
    return false;

  bool MadeChange = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      // We're looking for the sext.w pattern ADDIW rd, rs1, 0.
      if (!YSXInstrInfo::isSEXT_W(MI))
        continue;

      Register SrcReg = MI.getOperand(1).getReg();

      SmallPtrSet<MachineInstr *, 4> FixableDefs;

      // If all users only use the lower bits, this sext.w is redundant.
      // Or if all definitions reaching MI sign-extend their output,
      // then sext.w is redundant.
      if (!hasAllWUsers(MI, ST, MRI) &&
          !isSignExtendedW(SrcReg, ST, MRI, FixableDefs))
        continue;

      Register DstReg = MI.getOperand(0).getReg();
      if (!MRI.constrainRegClass(SrcReg, MRI.getRegClass(DstReg)))
        continue;

      // Convert Fixable instructions to their W versions.
      for (MachineInstr *Fixable : FixableDefs) {
        LLVM_DEBUG(dbgs() << "Replacing " << *Fixable);
        Fixable->setDesc(TII.get(getWOp(Fixable->getOpcode())));
        Fixable->clearFlag(MachineInstr::MIFlag::NoSWrap);
        Fixable->clearFlag(MachineInstr::MIFlag::NoUWrap);
        Fixable->clearFlag(MachineInstr::MIFlag::IsExact);
        LLVM_DEBUG(dbgs() << "     with " << *Fixable);
        ++NumTransformedToWInstrs;
      }

      LLVM_DEBUG(dbgs() << "Removing redundant sign-extension\n");
      MRI.replaceRegWith(DstReg, SrcReg);
      MRI.clearKillFlags(SrcReg);
      MI.eraseFromParent();
      ++NumRemovedSExtW;
      MadeChange = true;
    }
  }

  return MadeChange;
}

// Strips or adds W suffixes to eligible instructions depending on the
// subtarget preferences.
bool YSXOptWInstrs::canonicalizeWSuffixes(MachineFunction &MF,
                                            const YSXInstrInfo &TII,
                                            const YSXSubtarget &ST,
                                            MachineRegisterInfo &MRI) {
  bool ShouldStripW = !(DisableStripWSuffix || ST.preferWInst());
  bool ShouldPreferW = ST.preferWInst();
  bool MadeChange = false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      std::optional<unsigned> WOpc;
      std::optional<unsigned> NonWOpc;
      unsigned OrigOpc = MI.getOpcode();
      switch (OrigOpc) {
      default:
        continue;
      case YSX::ADDW:
        NonWOpc = YSX::ADD;
        break;
      case YSX::ADDIW:
        NonWOpc = YSX::ADDI;
        break;
      case YSX::MULW:
        NonWOpc = YSX::MUL;
        break;
      case YSX::SLLIW:
        NonWOpc = YSX::SLLI;
        break;
      case YSX::SUBW:
        NonWOpc = YSX::SUB;
        break;
      case YSX::ADD:
        WOpc = YSX::ADDW;
        break;
      case YSX::ADDI:
        WOpc = YSX::ADDIW;
        break;
      case YSX::SUB:
        WOpc = YSX::SUBW;
        break;
      case YSX::MUL:
        WOpc = YSX::MULW;
        break;
      case YSX::SLLI:
        // SLLIW reads the lowest 5 bits, while SLLI reads lowest 6 bits.
        if (MI.getOperand(2).getImm() >= 32)
          continue;
        WOpc = YSX::SLLIW;
        break;
      case YSX::LD:
      case YSX::LWU:
        WOpc = YSX::LW;
        break;
      }

      if (ShouldStripW && NonWOpc.has_value() && hasAllWUsers(MI, ST, MRI)) {
        LLVM_DEBUG(dbgs() << "Replacing " << MI);
        MI.setDesc(TII.get(NonWOpc.value()));
        LLVM_DEBUG(dbgs() << "     with " << MI);
        ++NumTransformedToNonWInstrs;
        MadeChange = true;
        continue;
      }
      // LWU is always converted to LW when possible as 1) LW is compressible
      // and 2) it helps minimise differences vs RV32.
      if ((ShouldPreferW || OrigOpc == YSX::LWU) && WOpc.has_value() &&
          hasAllWUsers(MI, ST, MRI)) {
        LLVM_DEBUG(dbgs() << "Replacing " << MI);
        MI.setDesc(TII.get(WOpc.value()));
        MI.clearFlag(MachineInstr::MIFlag::NoSWrap);
        MI.clearFlag(MachineInstr::MIFlag::NoUWrap);
        MI.clearFlag(MachineInstr::MIFlag::IsExact);
        LLVM_DEBUG(dbgs() << "     with " << MI);
        ++NumTransformedToWInstrs;
        MadeChange = true;
        continue;
      }
    }
  }
  return MadeChange;
}

bool YSXOptWInstrs::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;

  MachineRegisterInfo &MRI = MF.getRegInfo();
  const YSXSubtarget &ST = MF.getSubtarget<YSXSubtarget>();
  const YSXInstrInfo &TII = *ST.getInstrInfo();

  if (!ST.is64Bit())
    return false;

  bool MadeChange = false;
  MadeChange |= removeSExtWInstrs(MF, TII, ST, MRI);
  MadeChange |= canonicalizeWSuffixes(MF, TII, ST, MRI);
  return MadeChange;
}
