//===-- YSXISelDAGToDAG.cpp - A dag to dag inst selector for RISC-V -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the RISC-V target.
//
//===----------------------------------------------------------------------===//

#include "YSXISelDAGToDAG.h"
#include "MCTargetDesc/YSXBaseInfo.h"
#include "MCTargetDesc/YSXMCTargetDesc.h"
#include "MCTargetDesc/YSXMatInt.h"
#include "YSXISelLowering.h"
#include "YSXInstrInfo.h"
#include "YSXSelectionDAGInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/SDPatternMatch.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "ysx-isel"
#define PASS_NAME "RISC-V DAG->DAG Pattern Instruction Selection"

static cl::opt<bool> UsePseudoMovImm(
    "ysx-use-rematerializable-movimm", cl::Hidden,
    cl::desc("Use a rematerializable pseudoinstruction for 2 instruction "
             "constant materialization"),
    cl::init(false));

#define GET_DAGISEL_BODY YSXDAGToDAGISel
#include "YSXGenDAGISel.inc"

void YSXDAGToDAGISel::PreprocessISelDAG() {}

void YSXDAGToDAGISel::PostprocessISelDAG() {
  HandleSDNode Dummy(CurDAG->getRoot());
  SelectionDAG::allnodes_iterator Position = CurDAG->allnodes_end();

  bool MadeChange = false;
  while (Position != CurDAG->allnodes_begin()) {
    SDNode *N = &*--Position;
    // Skip dead nodes and any non-machine opcodes.
    if (N->use_empty() || !N->isMachineOpcode())
      continue;

    MadeChange |= doPeepholeSExtW(N);

  }

  CurDAG->setRoot(Dummy.getValue());

  if (MadeChange)
    CurDAG->RemoveDeadNodes();
}

static SDValue selectImmSeq(SelectionDAG *CurDAG, const SDLoc &DL, const MVT VT,
                            YSXMatInt::InstSeq &Seq) {
  SDValue SrcReg = CurDAG->getRegister(YSX::X0, VT);
  for (const YSXMatInt::Inst &Inst : Seq) {
    SDValue SDImm = CurDAG->getSignedTargetConstant(Inst.getImm(), DL, VT);
    SDNode *Result = nullptr;
    switch (Inst.getOpndKind()) {
    case YSXMatInt::Imm:
      Result = CurDAG->getMachineNode(Inst.getOpcode(), DL, VT, SDImm);
      break;
    case YSXMatInt::RegX0:
      Result = CurDAG->getMachineNode(Inst.getOpcode(), DL, VT, SrcReg,
                                      CurDAG->getRegister(YSX::X0, VT));
      break;
    case YSXMatInt::RegReg:
      Result = CurDAG->getMachineNode(Inst.getOpcode(), DL, VT, SrcReg, SrcReg);
      break;
    case YSXMatInt::RegImm:
      Result = CurDAG->getMachineNode(Inst.getOpcode(), DL, VT, SrcReg, SDImm);
      break;
    }

    // Only the first instruction has X0 as its source.
    SrcReg = SDValue(Result, 0);
  }

  return SrcReg;
}

static SDValue selectImm(SelectionDAG *CurDAG, const SDLoc &DL, const MVT VT,
                         int64_t Imm, const YSXSubtarget &Subtarget) {
  YSXMatInt::InstSeq Seq = YSXMatInt::generateInstSeq(Imm, Subtarget);

  // Use a rematerializable pseudo instruction for short sequences if enabled.
  if (Seq.size() == 2 && UsePseudoMovImm)
    return SDValue(
        CurDAG->getMachineNode(YSX::PseudoMovImm, DL, VT,
                               CurDAG->getSignedTargetConstant(Imm, DL, VT)),
        0);

  // See if we can create this constant as (ADD (SLLI X, C), X) where X is at
  // worst an LUI+ADDIW. This will require an extra register, but avoids a
  // constant pool.
  // Use a plain ADD sequence for constants that can be formed from two related
  // register values.
  if (Seq.size() > 3) {
    unsigned ShiftAmt, AddOpc;
    YSXMatInt::InstSeq SeqLo =
        YSXMatInt::generateTwoRegInstSeq(Imm, Subtarget, ShiftAmt, AddOpc);
    if (!SeqLo.empty() && (SeqLo.size() + 2) < Seq.size()) {
      SDValue Lo = selectImmSeq(CurDAG, DL, VT, SeqLo);

      SDValue SLLI = SDValue(
          CurDAG->getMachineNode(YSX::SLLI, DL, VT, Lo,
                                 CurDAG->getTargetConstant(ShiftAmt, DL, VT)),
          0);
      return SDValue(CurDAG->getMachineNode(AddOpc, DL, VT, Lo, SLLI), 0);
    }
  }

  // Otherwise, use the original sequence.
  return selectImmSeq(CurDAG, DL, VT, Seq);
}

bool YSXDAGToDAGISel::tryShrinkShlLogicImm(SDNode *Node) {
  MVT VT = Node->getSimpleValueType(0);
  unsigned Opcode = Node->getOpcode();
  assert((Opcode == ISD::AND || Opcode == ISD::OR || Opcode == ISD::XOR) &&
         "Unexpected opcode");
  SDLoc DL(Node);

  // For operations of the form (x << C1) op C2, check if we can use
  // ANDI/ORI/XORI by transforming it into (x op (C2>>C1)) << C1.
  SDValue N0 = Node->getOperand(0);
  SDValue N1 = Node->getOperand(1);

  ConstantSDNode *Cst = dyn_cast<ConstantSDNode>(N1);
  if (!Cst)
    return false;

  int64_t Val = Cst->getSExtValue();

  // Check if immediate can already use ANDI/ORI/XORI.
  if (isInt<12>(Val))
    return false;

  SDValue Shift = N0;

  // If Val is simm32 and we have a sext_inreg from i32, then the binop
  // produces at least 33 sign bits. We can peek through the sext_inreg and use
  // a SLLIW at the end.
  bool SignExt = false;
  if (isInt<32>(Val) && N0.getOpcode() == ISD::SIGN_EXTEND_INREG &&
      N0.hasOneUse() && cast<VTSDNode>(N0.getOperand(1))->getVT() == MVT::i32) {
    SignExt = true;
    Shift = N0.getOperand(0);
  }

  if (Shift.getOpcode() != ISD::SHL || !Shift.hasOneUse())
    return false;

  ConstantSDNode *ShlCst = dyn_cast<ConstantSDNode>(Shift.getOperand(1));
  if (!ShlCst)
    return false;

  uint64_t ShAmt = ShlCst->getZExtValue();

  // Make sure that we don't change the operation by removing bits.
  // This only matters for OR and XOR, AND is unaffected.
  uint64_t RemovedBitsMask = maskTrailingOnes<uint64_t>(ShAmt);
  if (Opcode != ISD::AND && (Val & RemovedBitsMask) != 0)
    return false;

  int64_t ShiftedVal = Val >> ShAmt;
  if (!isInt<12>(ShiftedVal))
    return false;

  // If we peeked through a sext_inreg, make sure the shift is valid for SLLIW.
  if (SignExt && ShAmt >= 32)
    return false;

  // Ok, we can reorder to get a smaller immediate.
  unsigned BinOpc;
  switch (Opcode) {
  default: llvm_unreachable("Unexpected opcode");
  case ISD::AND: BinOpc = YSX::ANDI; break;
  case ISD::OR:  BinOpc = YSX::ORI;  break;
  case ISD::XOR: BinOpc = YSX::XORI; break;
  }

  unsigned ShOpc = SignExt ? YSX::SLLIW : YSX::SLLI;

  SDNode *BinOp = CurDAG->getMachineNode(
      BinOpc, DL, VT, Shift.getOperand(0),
      CurDAG->getSignedTargetConstant(ShiftedVal, DL, VT));
  SDNode *SLLI =
      CurDAG->getMachineNode(ShOpc, DL, VT, SDValue(BinOp, 0),
                             CurDAG->getTargetConstant(ShAmt, DL, VT));
  ReplaceNode(Node, SLLI);
  return true;
}

bool YSXDAGToDAGISel::trySignedBitfieldExtract(SDNode *Node) {
  return false;
}

bool YSXDAGToDAGISel::trySignedBitfieldInsertInSign(SDNode *Node) {
  return false;
}

bool YSXDAGToDAGISel::tryUnsignedBitfieldExtract(SDNode *Node,
                                                   const SDLoc &DL, MVT VT,
                                                   SDValue X, unsigned Msb,
                                                   unsigned Lsb) {
  return false;
}

bool YSXDAGToDAGISel::tryUnsignedBitfieldInsertInZero(SDNode *Node,
                                                        const SDLoc &DL, MVT VT,
                                                        SDValue X, unsigned Msb,
                                                        unsigned Lsb) {
  return false;
}

bool YSXDAGToDAGISel::tryIndexedLoad(SDNode *Node) {
  return false;
}

void YSXDAGToDAGISel::Select(SDNode *Node) {
  // If we have a custom node, we have already selected.
  if (Node->isMachineOpcode()) {
    LLVM_DEBUG(dbgs() << "== "; Node->dump(CurDAG); dbgs() << "\n");
    Node->setNodeId(-1);
    return;
  }

  // Instruction Selection not handled by the auto-generated tablegen selection
  // should be handled here.
  unsigned Opcode = Node->getOpcode();
  SDLoc DL(Node);
  MVT VT = Node->getSimpleValueType(0);

  switch (Opcode) {
  case ISD::Constant: {
    assert(VT == Subtarget->getXLenVT() && "Unexpected VT");
    auto *ConstNode = cast<ConstantSDNode>(Node);
    if (ConstNode->isZero()) {
      SDValue New =
          CurDAG->getCopyFromReg(CurDAG->getEntryNode(), DL, YSX::X0, VT);
      ReplaceNode(Node, New.getNode());
      return;
    }
    int64_t Imm = ConstNode->getSExtValue();
    // If only the lower 8 bits are used, try to convert this to a simm6 by
    // sign-extending bit 7. This is neutral without the C extension, and
    // allows C.LI to be used if C is present.
    if (!isInt<8>(Imm) && isUInt<8>(Imm) && isInt<6>(SignExtend64<8>(Imm)) &&
        hasAllBUsers(Node))
      Imm = SignExtend64<8>(Imm);
    // If the upper XLen-16 bits are not used, try to convert this to a simm12
    // by sign extending bit 15.
    else if (!isInt<16>(Imm) && isUInt<16>(Imm) &&
             isInt<12>(SignExtend64<16>(Imm)) && hasAllHUsers(Node))
      Imm = SignExtend64<16>(Imm);
    // If the upper 32-bits are not used try to convert this into a simm32 by
    // sign extending bit 32.
    else if (!isInt<32>(Imm) && isUInt<32>(Imm) && hasAllWUsers(Node))
      Imm = SignExtend64<32>(Imm);

    ReplaceNode(Node, selectImm(CurDAG, DL, VT, Imm, *Subtarget).getNode());
    return;
  }
  case YSXISD::BuildGPRPair: {
    assert(Subtarget->is64Bit() && "YSX GPR pairs are only modeled for ysx64");

    SDValue Ops[] = {
        CurDAG->getTargetConstant(YSX::GPRPairRegClassID, DL, MVT::i32),
        Node->getOperand(0),
        CurDAG->getTargetConstant(YSX::sub_gpr_even, DL, MVT::i32),
        Node->getOperand(1),
        CurDAG->getTargetConstant(YSX::sub_gpr_odd, DL, MVT::i32)};

    SDNode *N = CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL, VT, Ops);
    ReplaceNode(Node, N);
    return;
  }
  case YSXISD::SplitGPRPair: {
    assert(Subtarget->is64Bit() && "YSX GPR pairs are only modeled for ysx64");
    if (!SDValue(Node, 0).use_empty()) {
      SDValue Lo = CurDAG->getTargetExtractSubreg(YSX::sub_gpr_even, DL,
                                                  Node->getValueType(0),
                                                  Node->getOperand(0));
      ReplaceUses(SDValue(Node, 0), Lo);
    }

    if (!SDValue(Node, 1).use_empty()) {
      SDValue Hi = CurDAG->getTargetExtractSubreg(YSX::sub_gpr_odd, DL,
                                                  Node->getValueType(1),
                                                  Node->getOperand(0));
      ReplaceUses(SDValue(Node, 1), Hi);
    }

    CurDAG->RemoveDeadNode(Node);
    return;
  }
  case ISD::SHL: {
    auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
    if (!N1C)
      break;
    SDValue N0 = Node->getOperand(0);
    if (N0.getOpcode() != ISD::AND || !N0.hasOneUse() ||
        !isa<ConstantSDNode>(N0.getOperand(1)))
      break;
    unsigned ShAmt = N1C->getZExtValue();
    uint64_t Mask = N0.getConstantOperandVal(1);

    if (isShiftedMask_64(Mask)) {
      unsigned XLen = Subtarget->getXLen();
      unsigned LeadingZeros = XLen - llvm::bit_width(Mask);
      unsigned TrailingZeros = llvm::countr_zero(Mask);
      if (ShAmt <= 32 && TrailingZeros > 0 && LeadingZeros == 32) {
        // Optimize (shl (and X, C2), C) -> (slli (srliw X, C3), C3+C)
        // where C2 has 32 leading zeros and C3 trailing zeros.
        SDNode *SRLIW = CurDAG->getMachineNode(
            YSX::SRLIW, DL, VT, N0.getOperand(0),
            CurDAG->getTargetConstant(TrailingZeros, DL, VT));
        SDNode *SLLI = CurDAG->getMachineNode(
            YSX::SLLI, DL, VT, SDValue(SRLIW, 0),
            CurDAG->getTargetConstant(TrailingZeros + ShAmt, DL, VT));
        ReplaceNode(Node, SLLI);
        return;
      }
      if (TrailingZeros == 0 && LeadingZeros > ShAmt &&
          XLen - LeadingZeros > 11 && LeadingZeros != 32) {
        // Optimize (shl (and X, C2), C) -> (srli (slli X, C4), C4-C)
        // where C2 has C4 leading zeros and no trailing zeros.
        // This is profitable if the "and" was to be lowered to
        // (srli (slli X, C4), C4) and not (andi X, C2).
        SDNode *SLLI = CurDAG->getMachineNode(
            YSX::SLLI, DL, VT, N0.getOperand(0),
            CurDAG->getTargetConstant(LeadingZeros, DL, VT));
        SDNode *SRLI = CurDAG->getMachineNode(
            YSX::SRLI, DL, VT, SDValue(SLLI, 0),
            CurDAG->getTargetConstant(LeadingZeros - ShAmt, DL, VT));
        ReplaceNode(Node, SRLI);
        return;
      }
    }
    break;
  }
  case ISD::SRL: {
    auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
    if (!N1C)
      break;
    SDValue N0 = Node->getOperand(0);
    if (N0.getOpcode() != ISD::AND || !isa<ConstantSDNode>(N0.getOperand(1)))
      break;
    unsigned ShAmt = N1C->getZExtValue();
    uint64_t Mask = N0.getConstantOperandVal(1);

    // Optimize (srl (and X, C2), C) -> (slli (srliw X, C3), C3-C) where C2 has
    // 32 leading zeros and C3 trailing zeros.
    if (isShiftedMask_64(Mask) && N0.hasOneUse()) {
      unsigned XLen = Subtarget->getXLen();
      unsigned LeadingZeros = XLen - llvm::bit_width(Mask);
      unsigned TrailingZeros = llvm::countr_zero(Mask);
      if (LeadingZeros == 32 && TrailingZeros > ShAmt) {
        SDNode *SRLIW = CurDAG->getMachineNode(
            YSX::SRLIW, DL, VT, N0.getOperand(0),
            CurDAG->getTargetConstant(TrailingZeros, DL, VT));
        SDNode *SLLI = CurDAG->getMachineNode(
            YSX::SLLI, DL, VT, SDValue(SRLIW, 0),
            CurDAG->getTargetConstant(TrailingZeros - ShAmt, DL, VT));
        ReplaceNode(Node, SLLI);
        return;
      }
    }

    // Optimize (srl (and X, C2), C) ->
    //          (srli (slli X, (XLen-C3), (XLen-C3) + C)
    // Where C2 is a mask with C3 trailing ones.
    // Taking into account that the C2 may have had lower bits unset by
    // SimplifyDemandedBits. This avoids materializing the C2 immediate.
    // This pattern occurs when type legalizing right shifts for types with
    // less than XLen bits.
    Mask |= maskTrailingOnes<uint64_t>(ShAmt);
    if (!isMask_64(Mask))
      break;
    unsigned TrailingOnes = llvm::countr_one(Mask);
    if (ShAmt >= TrailingOnes)
      break;
    if (TrailingOnes == 32) {
      SDNode *SRLI = CurDAG->getMachineNode(
          YSX::SRLIW, DL, VT, N0.getOperand(0),
          CurDAG->getTargetConstant(ShAmt, DL, VT));
      ReplaceNode(Node, SRLI);
      return;
    }

    // Only do the remaining transforms if the AND has one use.
    if (!N0.hasOneUse())
      break;

    const unsigned Msb = TrailingOnes - 1;
    const unsigned Lsb = ShAmt;
    if (tryUnsignedBitfieldExtract(Node, DL, VT, N0.getOperand(0), Msb, Lsb))
      return;

    unsigned LShAmt = Subtarget->getXLen() - TrailingOnes;
    SDNode *SLLI =
        CurDAG->getMachineNode(YSX::SLLI, DL, VT, N0.getOperand(0),
                               CurDAG->getTargetConstant(LShAmt, DL, VT));
    SDNode *SRLI = CurDAG->getMachineNode(
        YSX::SRLI, DL, VT, SDValue(SLLI, 0),
        CurDAG->getTargetConstant(LShAmt + ShAmt, DL, VT));
    ReplaceNode(Node, SRLI);
    return;
  }
  case ISD::SRA: {
    if (trySignedBitfieldExtract(Node))
      return;

    if (trySignedBitfieldInsertInSign(Node))
      return;

    // Optimize (sra (sext_inreg X, i16), C) ->
    //          (srai (slli X, (XLen-16), (XLen-16) + C)
    // And      (sra (sext_inreg X, i8), C) ->
    //          (srai (slli X, (XLen-8), (XLen-8) + C)
    // This can help expose CSE opportunities in the sdiv by constant
    // optimization.
    auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
    if (!N1C)
      break;
    SDValue N0 = Node->getOperand(0);
    if (N0.getOpcode() != ISD::SIGN_EXTEND_INREG || !N0.hasOneUse())
      break;
    unsigned ShAmt = N1C->getZExtValue();
    unsigned ExtSize =
        cast<VTSDNode>(N0.getOperand(1))->getVT().getSizeInBits();
    // ExtSize of 32 should use sraiw via tablegen pattern.
    if (ExtSize >= 32 || ShAmt >= ExtSize)
      break;
    unsigned LShAmt = Subtarget->getXLen() - ExtSize;
    SDNode *SLLI =
        CurDAG->getMachineNode(YSX::SLLI, DL, VT, N0.getOperand(0),
                               CurDAG->getTargetConstant(LShAmt, DL, VT));
    SDNode *SRAI = CurDAG->getMachineNode(
        YSX::SRAI, DL, VT, SDValue(SLLI, 0),
        CurDAG->getTargetConstant(LShAmt + ShAmt, DL, VT));
    ReplaceNode(Node, SRAI);
    return;
  }
  case ISD::OR: {
    if (tryShrinkShlLogicImm(Node))
      return;

    break;
  }
  case ISD::XOR:
    if (tryShrinkShlLogicImm(Node))
      return;

    break;
  case ISD::AND: {
    auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
    if (!N1C)
      break;

    SDValue N0 = Node->getOperand(0);

    bool LeftShift = N0.getOpcode() == ISD::SHL;
    if (LeftShift || N0.getOpcode() == ISD::SRL) {
      auto *C = dyn_cast<ConstantSDNode>(N0.getOperand(1));
      if (!C)
        break;
      unsigned C2 = C->getZExtValue();
      unsigned XLen = Subtarget->getXLen();
      assert((C2 > 0 && C2 < XLen) && "Unexpected shift amount!");

      // TODO: What if ANDI faster than shift?

      uint64_t C1 = N1C->getZExtValue();

      // Clear irrelevant bits in the mask.
      if (LeftShift)
        C1 &= maskTrailingZeros<uint64_t>(C2);
      else
        C1 &= maskTrailingOnes<uint64_t>(XLen - C2);

      // Some transforms should only be done if the shift has a single use or
      // the AND would become (srli (slli X, 32), 32)
      bool OneUseOrZExtW = N0.hasOneUse() || C1 == UINT64_C(0xFFFFFFFF);

      SDValue X = N0.getOperand(0);

      // Turn (and (srl x, c2) c1) -> (srli (slli x, c3-c2), c3) if c1 is a mask
      // with c3 leading zeros.
      if (!LeftShift && isMask_64(C1)) {
        unsigned Leading = XLen - llvm::bit_width(C1);
        if (C2 < Leading) {
          // If the number of leading zeros is C2+32 this can be SRLIW.
          if (C2 + 32 == Leading) {
            SDNode *SRLIW = CurDAG->getMachineNode(
                YSX::SRLIW, DL, VT, X, CurDAG->getTargetConstant(C2, DL, VT));
            ReplaceNode(Node, SRLIW);
            return;
          }

          // (and (srl (sexti32 Y), c2), c1) -> (srliw (sraiw Y, 31), c3 - 32)
          // if c1 is a mask with c3 leading zeros and c2 >= 32 and c3-c2==1.
          //
          // This pattern occurs when (i32 (srl (sra 31), c3 - 32)) is type
          // legalized and goes through DAG combine.
          if (C2 >= 32 && (Leading - C2) == 1 && N0.hasOneUse() &&
              X.getOpcode() == ISD::SIGN_EXTEND_INREG &&
              cast<VTSDNode>(X.getOperand(1))->getVT() == MVT::i32) {
            SDNode *SRAIW =
                CurDAG->getMachineNode(YSX::SRAIW, DL, VT, X.getOperand(0),
                                       CurDAG->getTargetConstant(31, DL, VT));
            SDNode *SRLIW = CurDAG->getMachineNode(
                YSX::SRLIW, DL, VT, SDValue(SRAIW, 0),
                CurDAG->getTargetConstant(Leading - 32, DL, VT));
            ReplaceNode(Node, SRLIW);
            return;
          }

          // Try to use an unsigned bitfield extract (e.g., th.extu) if
          // available.
          // Transform (and (srl x, C2), C1)
          //        -> (<bfextract> x, msb, lsb)
          //
          // Make sure to keep this below the SRLIW cases, as we always want to
          // prefer the more common instruction.
          const unsigned Msb = llvm::bit_width(C1) + C2 - 1;
          const unsigned Lsb = C2;
          if (tryUnsignedBitfieldExtract(Node, DL, VT, X, Msb, Lsb))
            return;

          // (srli (slli x, c3-c2), c3).
          if (OneUseOrZExtW) {
            SDNode *SLLI = CurDAG->getMachineNode(
                YSX::SLLI, DL, VT, X,
                CurDAG->getTargetConstant(Leading - C2, DL, VT));
            SDNode *SRLI = CurDAG->getMachineNode(
                YSX::SRLI, DL, VT, SDValue(SLLI, 0),
                CurDAG->getTargetConstant(Leading, DL, VT));
            ReplaceNode(Node, SRLI);
            return;
          }
        }
      }

      // Turn (and (shl x, c2), c1) -> (srli (slli c2+c3), c3) if c1 is a mask
      // shifted by c2 bits with c3 leading zeros.
      if (LeftShift && isShiftedMask_64(C1)) {
        unsigned Leading = XLen - llvm::bit_width(C1);

        if (C2 + Leading < XLen &&
            C1 == (maskTrailingOnes<uint64_t>(XLen - (C2 + Leading)) << C2)) {
          // Use slli.uw when possible.

          // Try to use an unsigned bitfield insert (e.g., nds.bfoz) if
          // available.
          // Transform (and (shl x, c2), c1)
          //        -> (<bfinsert> x, msb, lsb)
          // e.g.
          //     (and (shl x, 12), 0x00fff000)
          //     If XLen = 32 and C2 = 12, then
          //     Msb = 32 - 8 - 1 = 23 and Lsb = 12
          const unsigned Msb = XLen - Leading - 1;
          const unsigned Lsb = C2;
          if (tryUnsignedBitfieldInsertInZero(Node, DL, VT, X, Msb, Lsb))
            return;

          if (OneUseOrZExtW) {
            // (packh x0, X)
            // (srli (slli c2+c3), c3)
            SDNode *SLLI = CurDAG->getMachineNode(
                YSX::SLLI, DL, VT, X,
                CurDAG->getTargetConstant(C2 + Leading, DL, VT));
            SDNode *SRLI = CurDAG->getMachineNode(
                YSX::SRLI, DL, VT, SDValue(SLLI, 0),
                CurDAG->getTargetConstant(Leading, DL, VT));
            ReplaceNode(Node, SRLI);
            return;
          }
        }
      }

      // Turn (and (shr x, c2), c1) -> (slli (srli x, c2+c3), c3) if c1 is a
      // shifted mask with c2 leading zeros and c3 trailing zeros.
      if (!LeftShift && isShiftedMask_64(C1)) {
        unsigned Leading = XLen - llvm::bit_width(C1);
        unsigned Trailing = llvm::countr_zero(C1);
        if (Leading == C2 && C2 + Trailing < XLen && OneUseOrZExtW) {
          unsigned SrliOpc = YSX::SRLI;
          // If the input is zexti32 we should use SRLIW.
          if (X.getOpcode() == ISD::AND &&
              isa<ConstantSDNode>(X.getOperand(1)) &&
              X.getConstantOperandVal(1) == UINT64_C(0xFFFFFFFF)) {
            SrliOpc = YSX::SRLIW;
            X = X.getOperand(0);
          }
          SDNode *SRLI = CurDAG->getMachineNode(
              SrliOpc, DL, VT, X,
              CurDAG->getTargetConstant(C2 + Trailing, DL, VT));
          SDNode *SLLI = CurDAG->getMachineNode(
              YSX::SLLI, DL, VT, SDValue(SRLI, 0),
              CurDAG->getTargetConstant(Trailing, DL, VT));
          ReplaceNode(Node, SLLI);
          return;
        }
        // If the leading zero count is C2+32, we can use SRLIW instead of SRLI.
        if (Leading > 32 && (Leading - 32) == C2 && C2 + Trailing < 32 &&
            OneUseOrZExtW) {
          SDNode *SRLIW = CurDAG->getMachineNode(
              YSX::SRLIW, DL, VT, X,
              CurDAG->getTargetConstant(C2 + Trailing, DL, VT));
          SDNode *SLLI = CurDAG->getMachineNode(
              YSX::SLLI, DL, VT, SDValue(SRLIW, 0),
              CurDAG->getTargetConstant(Trailing, DL, VT));
          ReplaceNode(Node, SLLI);
          return;
        }
      }

      // Turn (and (shl x, c2), c1) -> (slli (srli x, c3-c2), c3) if c1 is a
      // shifted mask with no leading zeros and c3 trailing zeros.
      if (LeftShift && isShiftedMask_64(C1)) {
        unsigned Leading = XLen - llvm::bit_width(C1);
        unsigned Trailing = llvm::countr_zero(C1);
        if (Leading == 0 && C2 < Trailing && OneUseOrZExtW) {
          SDNode *SRLI = CurDAG->getMachineNode(
              YSX::SRLI, DL, VT, X,
              CurDAG->getTargetConstant(Trailing - C2, DL, VT));
          SDNode *SLLI = CurDAG->getMachineNode(
              YSX::SLLI, DL, VT, SDValue(SRLI, 0),
              CurDAG->getTargetConstant(Trailing, DL, VT));
          ReplaceNode(Node, SLLI);
          return;
        }
        // If we have (32-C2) leading zeros, we can use SRLIW instead of SRLI.
        if (C2 < Trailing && Leading + C2 == 32 && OneUseOrZExtW) {
          SDNode *SRLIW = CurDAG->getMachineNode(
              YSX::SRLIW, DL, VT, X,
              CurDAG->getTargetConstant(Trailing - C2, DL, VT));
          SDNode *SLLI = CurDAG->getMachineNode(
              YSX::SLLI, DL, VT, SDValue(SRLIW, 0),
              CurDAG->getTargetConstant(Trailing, DL, VT));
          ReplaceNode(Node, SLLI);
          return;
        }

      }
    }

    const uint64_t C1 = N1C->getZExtValue();

    if (N0.getOpcode() == ISD::SRA && isa<ConstantSDNode>(N0.getOperand(1)) &&
        N0.hasOneUse()) {
      unsigned C2 = N0.getConstantOperandVal(1);
      unsigned XLen = Subtarget->getXLen();
      assert((C2 > 0 && C2 < XLen) && "Unexpected shift amount!");

      SDValue X = N0.getOperand(0);

      // Prefer SRAIW + ANDI when possible.
      bool Skip = C2 > 32 && isInt<12>(N1C->getSExtValue()) &&
                  X.getOpcode() == ISD::SHL &&
                  isa<ConstantSDNode>(X.getOperand(1)) &&
                  X.getConstantOperandVal(1) == 32;
      // Turn (and (sra x, c2), c1) -> (srli (srai x, c2-c3), c3) if c1 is a
      // mask with c3 leading zeros and c2 is larger than c3.
      if (isMask_64(C1) && !Skip) {
        unsigned Leading = XLen - llvm::bit_width(C1);
        if (C2 > Leading) {
          SDNode *SRAI = CurDAG->getMachineNode(
              YSX::SRAI, DL, VT, X,
              CurDAG->getTargetConstant(C2 - Leading, DL, VT));
          SDNode *SRLI = CurDAG->getMachineNode(
              YSX::SRLI, DL, VT, SDValue(SRAI, 0),
              CurDAG->getTargetConstant(Leading, DL, VT));
          ReplaceNode(Node, SRLI);
          return;
        }
      }

      // Look for (and (sra y, c2), c1) where c1 is a shifted mask with c3
      // leading zeros and c4 trailing zeros. If c2 is greater than c3, we can
      // use (slli (srli (srai y, c2 - c3), c3 + c4), c4).
      if (isShiftedMask_64(C1) && !Skip) {
        unsigned Leading = XLen - llvm::bit_width(C1);
        unsigned Trailing = llvm::countr_zero(C1);
        if (C2 > Leading && Leading > 0 && Trailing > 0) {
          SDNode *SRAI = CurDAG->getMachineNode(
              YSX::SRAI, DL, VT, N0.getOperand(0),
              CurDAG->getTargetConstant(C2 - Leading, DL, VT));
          SDNode *SRLI = CurDAG->getMachineNode(
              YSX::SRLI, DL, VT, SDValue(SRAI, 0),
              CurDAG->getTargetConstant(Leading + Trailing, DL, VT));
          SDNode *SLLI = CurDAG->getMachineNode(
              YSX::SLLI, DL, VT, SDValue(SRLI, 0),
              CurDAG->getTargetConstant(Trailing, DL, VT));
          ReplaceNode(Node, SLLI);
          return;
        }
      }
    }

    // If C1 masks off the upper bits only (but can't be formed as an
    // ANDI), use an unsigned bitfield extract (e.g., th.extu), if
    // available.
    // Transform (and x, C1)
    //        -> (<bfextract> x, msb, lsb)
    if (isMask_64(C1) && !isInt<12>(N1C->getSExtValue())) {
      const unsigned Msb = llvm::bit_width(C1) - 1;
      if (tryUnsignedBitfieldExtract(Node, DL, VT, N0, Msb, 0))
        return;
    }

    if (tryShrinkShlLogicImm(Node))
      return;

    break;
  }
  case ISD::MUL: {
    // Special case for calculating (mul (and X, C2), C1) where the full product
    // fits in XLen bits. We can shift X left by the number of leading zeros in
    // C2 and shift C1 left by XLen-lzcnt(C2). This will ensure the final
    // product has XLen trailing zeros, putting it in the output of MULHU. This
    // can avoid materializing a constant in a register for C2.

    // RHS should be a constant.
    auto *N1C = dyn_cast<ConstantSDNode>(Node->getOperand(1));
    if (!N1C || !N1C->hasOneUse())
      break;

    // LHS should be an AND with constant.
    SDValue N0 = Node->getOperand(0);
    if (N0.getOpcode() != ISD::AND || !isa<ConstantSDNode>(N0.getOperand(1)))
      break;

    uint64_t C2 = N0.getConstantOperandVal(1);

    // Constant should be a mask.
    if (!isMask_64(C2))
      break;

    // If this can be an ANDI or ZEXT.H, don't do this if the ANDI/ZEXT has
    // multiple users or the constant is a simm12. This prevents inserting a
    // shift and still have uses of the AND/ZEXT. Shifting a simm12 will likely
    // make it more costly to materialize.
    bool IsANDIOrZExt = isInt<12>(C2);
    if (IsANDIOrZExt && (isInt<12>(N1C->getSExtValue()) || !N0.hasOneUse()))
      break;

    // We need to shift left the AND input and C1 by a total of XLen bits.

    // How far left do we need to shift the AND input?
    unsigned XLen = Subtarget->getXLen();
    unsigned LeadingZeros = XLen - llvm::bit_width(C2);

    // The constant gets shifted by the remaining amount unless that would
    // shift bits out.
    uint64_t C1 = N1C->getZExtValue();
    unsigned ConstantShift = XLen - LeadingZeros;
    if (ConstantShift > (XLen - llvm::bit_width(C1)))
      break;

    uint64_t ShiftedC1 = C1 << ConstantShift;
    // Create (mulhu (slli X, lzcnt(C2)), C1 << (XLen - lzcnt(C2))).
    SDNode *Imm = selectImm(CurDAG, DL, VT, ShiftedC1, *Subtarget).getNode();
    SDNode *SLLI =
        CurDAG->getMachineNode(YSX::SLLI, DL, VT, N0.getOperand(0),
                               CurDAG->getTargetConstant(LeadingZeros, DL, VT));
    SDNode *MULHU = CurDAG->getMachineNode(YSX::MULHU, DL, VT,
                                           SDValue(SLLI, 0), SDValue(Imm, 0));
    ReplaceNode(Node, MULHU);
    return;
  }
  case ISD::LOAD: {
    if (tryIndexedLoad(Node))
      return;
    break;
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    unsigned IntNo = Node->getConstantOperandVal(0);
    switch (IntNo) {
      // By default we do not custom select any intrinsic.
    default:
      break;
    }
    break;
  }
  case ISD::INTRINSIC_W_CHAIN: {
    break;
  }
  case ISD::INTRINSIC_VOID: {
    break;
  }
  case ISD::BITCAST: {
    break;
  }
  case ISD::PREFETCH:
    unsigned Locality = Node->getConstantOperandVal(3);
    if (Locality > 2)
      break;

    auto *LoadStoreMem = cast<MemSDNode>(Node);
    MachineMemOperand *MMO = LoadStoreMem->getMemOperand();
    MMO->setFlags(MachineMemOperand::MONonTemporal);

    int NontemporalLevel = 0;
    switch (Locality) {
    case 0:
      NontemporalLevel = 3; // NTL.ALL
      break;
    case 1:
      NontemporalLevel = 1; // NTL.PALL
      break;
    case 2:
      NontemporalLevel = 0; // NTL.P1
      break;
    default:
      llvm_unreachable("unexpected locality value.");
    }

    if (NontemporalLevel & 0b1)
      MMO->setFlags(MONontemporalBit0);
    if (NontemporalLevel & 0b10)
      MMO->setFlags(MONontemporalBit1);
    break;
  }

  // Select the default instruction.
  SelectCode(Node);
}

bool YSXDAGToDAGISel::SelectInlineAsmMemoryOperand(
    const SDValue &Op, InlineAsm::ConstraintCode ConstraintID,
    std::vector<SDValue> &OutOps) {
  // Always produce a register and immediate operand, as expected by
  // YSXAsmPrinter::PrintAsmMemoryOperand.
  switch (ConstraintID) {
  case InlineAsm::ConstraintCode::o:
  case InlineAsm::ConstraintCode::m: {
    SDValue Op0, Op1;
    [[maybe_unused]] bool Found = SelectAddrRegImm(Op, Op0, Op1);
    assert(Found && "SelectAddrRegImm should always succeed");
    OutOps.push_back(Op0);
    OutOps.push_back(Op1);
    return false;
  }
  case InlineAsm::ConstraintCode::A:
    OutOps.push_back(Op);
    OutOps.push_back(
        CurDAG->getTargetConstant(0, SDLoc(Op), Subtarget->getXLenVT()));
    return false;
  default:
    report_fatal_error("Unexpected asm memory constraint " +
                       InlineAsm::getMemConstraintName(ConstraintID));
  }

  return true;
}

bool YSXDAGToDAGISel::SelectAddrFrameIndex(SDValue Addr, SDValue &Base,
                                             SDValue &Offset) {
  if (auto *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), Subtarget->getXLenVT());
    Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), Subtarget->getXLenVT());
    return true;
  }

  return false;
}

// Fold constant addresses.
static bool selectConstantAddr(SelectionDAG *CurDAG, const SDLoc &DL,
                               const MVT VT, const YSXSubtarget *Subtarget,
                               SDValue Addr, SDValue &Base, SDValue &Offset,
                               bool IsPrefetch = false) {
  if (!isa<ConstantSDNode>(Addr))
    return false;

  int64_t CVal = cast<ConstantSDNode>(Addr)->getSExtValue();

  // If the constant is a simm12, we can fold the whole constant and use X0 as
  // the base. If the constant can be materialized with LUI+simm12, use LUI as
  // the base. We can't use generateInstSeq because it favors LUI+ADDIW.
  int64_t Lo12 = SignExtend64<12>(CVal);
  int64_t Hi = (uint64_t)CVal - (uint64_t)Lo12;
  if (!Subtarget->is64Bit() || isInt<32>(Hi)) {
    if (IsPrefetch && (Lo12 & 0b11111) != 0)
      return false;
    if (Hi) {
      int64_t Hi20 = (Hi >> 12) & 0xfffff;
      Base = SDValue(
          CurDAG->getMachineNode(YSX::LUI, DL, VT,
                                 CurDAG->getTargetConstant(Hi20, DL, VT)),
          0);
    } else {
      Base = CurDAG->getRegister(YSX::X0, VT);
    }
    Offset = CurDAG->getSignedTargetConstant(Lo12, DL, VT);
    return true;
  }

  // Ask how constant materialization would handle this constant.
  YSXMatInt::InstSeq Seq = YSXMatInt::generateInstSeq(CVal, *Subtarget);

  // If the last instruction would be an ADDI, we can fold its immediate and
  // emit the rest of the sequence as the base.
  if (Seq.back().getOpcode() != YSX::ADDI)
    return false;
  Lo12 = Seq.back().getImm();
  if (IsPrefetch && (Lo12 & 0b11111) != 0)
    return false;

  // Drop the last instruction.
  Seq.pop_back();
  assert(!Seq.empty() && "Expected more instructions in sequence");

  Base = selectImmSeq(CurDAG, DL, VT, Seq);
  Offset = CurDAG->getSignedTargetConstant(Lo12, DL, VT);
  return true;
}

// Is this ADD instruction only used as the base pointer of scalar loads and
// stores?
static bool isWorthFoldingAdd(SDValue Add) {
  for (auto *User : Add->users()) {
    if (User->getOpcode() != ISD::LOAD && User->getOpcode() != ISD::STORE &&
        User->getOpcode() != ISD::ATOMIC_LOAD &&
        User->getOpcode() != ISD::ATOMIC_STORE)
      return false;
    EVT VT = cast<MemSDNode>(User)->getMemoryVT();
    if (!VT.isScalarInteger())
      return false;
    // Don't allow stores of the value. It must be used as the address.
    if (User->getOpcode() == ISD::STORE &&
        cast<StoreSDNode>(User)->getValue() == Add)
      return false;
    if (User->getOpcode() == ISD::ATOMIC_STORE &&
        cast<AtomicSDNode>(User)->getVal() == Add)
      return false;
    if (isStrongerThanMonotonic(cast<MemSDNode>(User)->getSuccessOrdering()))
      return false;
  }

  return true;
}

static bool isRegImmLoadOrStore(SDNode *User, SDValue Add) {
  switch (User->getOpcode()) {
  default:
    return false;
  case ISD::LOAD:
  case ISD::ATOMIC_LOAD:
    break;
  case ISD::STORE:
    // Don't allow stores of Add. It must only be used as the address.
    if (cast<StoreSDNode>(User)->getValue() == Add)
      return false;
    break;
  case ISD::ATOMIC_STORE:
    // Don't allow stores of Add. It must only be used as the address.
    if (cast<AtomicSDNode>(User)->getVal() == Add)
      return false;
    break;
  }

  return true;
}

// To prevent SelectAddrRegImm from folding offsets that conflict with the
// fusion of PseudoMovAddr, check if the offset of every use of a given address
// is within the alignment.
bool YSXDAGToDAGISel::areOffsetsWithinAlignment(SDValue Addr,
                                                  Align Alignment) {
  assert(Addr->getOpcode() == YSXISD::ADD_LO);
  for (auto *User : Addr->users()) {
    // If the user is a load or store, then the offset is 0 which is always
    // within alignment.
    if (isRegImmLoadOrStore(User, Addr))
      continue;

    if (CurDAG->isBaseWithConstantOffset(SDValue(User, 0))) {
      int64_t CVal = cast<ConstantSDNode>(User->getOperand(1))->getSExtValue();
      if (!isInt<12>(CVal) || Alignment <= CVal)
        return false;

      // Make sure all uses are foldable load/stores.
      for (auto *AddUser : User->users())
        if (!isRegImmLoadOrStore(AddUser, SDValue(User, 0)))
          return false;

      continue;
    }

    return false;
  }

  return true;
}

bool YSXDAGToDAGISel::SelectAddrRegImm(SDValue Addr, SDValue &Base,
                                         SDValue &Offset) {
  if (SelectAddrFrameIndex(Addr, Base, Offset))
    return true;

  SDLoc DL(Addr);
  MVT VT = Addr.getSimpleValueType();

  if (Addr.getOpcode() == YSXISD::ADD_LO) {
    bool CanFold = true;
    // Unconditionally fold if operand 1 is not a global address (e.g.
    // externsymbol)
    if (auto *GA = dyn_cast<GlobalAddressSDNode>(Addr.getOperand(1))) {
      const DataLayout &DL = CurDAG->getDataLayout();
      Align Alignment = commonAlignment(
          GA->getGlobal()->getPointerAlignment(DL), GA->getOffset());
      if (!areOffsetsWithinAlignment(Addr, Alignment))
        CanFold = false;
    }
    if (CanFold) {
      Base = Addr.getOperand(0);
      Offset = Addr.getOperand(1);
      return true;
    }
  }

  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isInt<12>(CVal)) {
      Base = Addr.getOperand(0);
      if (Base.getOpcode() == YSXISD::ADD_LO) {
        SDValue LoOperand = Base.getOperand(1);
        if (auto *GA = dyn_cast<GlobalAddressSDNode>(LoOperand)) {
          // If the Lo in (ADD_LO hi, lo) is a global variable's address
          // (its low part, really), then we can rely on the alignment of that
          // variable to provide a margin of safety before low part can overflow
          // the 12 bits of the load/store offset. Check if CVal falls within
          // that margin; if so (low part + CVal) can't overflow.
          const DataLayout &DL = CurDAG->getDataLayout();
          Align Alignment = commonAlignment(
              GA->getGlobal()->getPointerAlignment(DL), GA->getOffset());
          if ((CVal == 0 || Alignment > CVal) &&
              areOffsetsWithinAlignment(Base, Alignment)) {
            int64_t CombinedOffset = CVal + GA->getOffset();
            Base = Base.getOperand(0);
            Offset = CurDAG->getTargetGlobalAddress(
                GA->getGlobal(), SDLoc(LoOperand), LoOperand.getValueType(),
                CombinedOffset, GA->getTargetFlags());
            return true;
          }
        }
      }

      if (auto *FIN = dyn_cast<FrameIndexSDNode>(Base))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), VT);
      Offset = CurDAG->getSignedTargetConstant(CVal, DL, VT);
      return true;
    }
  }

  // Handle ADD with large immediates.
  if (Addr.getOpcode() == ISD::ADD && isa<ConstantSDNode>(Addr.getOperand(1))) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    assert(!isInt<12>(CVal) && "simm12 not already handled?");

    // Handle immediates in the range [-4096,-2049] or [2048, 4094]. We can use
    // an ADDI for part of the offset and fold the rest into the load/store.
    // This mirrors the AddiPair PatFrag in YSXInstrInfo.td.
    if (CVal >= -4096 && CVal <= 4094) {
      int64_t Adj = CVal < 0 ? -2048 : 2047;
      Base = SDValue(
          CurDAG->getMachineNode(YSX::ADDI, DL, VT, Addr.getOperand(0),
                                 CurDAG->getSignedTargetConstant(Adj, DL, VT)),
          0);
      Offset = CurDAG->getSignedTargetConstant(CVal - Adj, DL, VT);
      return true;
    }

    // For larger immediates, we might be able to save one instruction from
    // constant materialization by folding the Lo12 bits of the immediate into
    // the address. We should only do this if the ADD is only used by loads and
    // stores that can fold the lo12 bits. Otherwise, the ADD will get iseled
    // separately with the full materialized immediate creating extra
    // instructions.
    if (isWorthFoldingAdd(Addr) &&
        selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr.getOperand(1), Base,
                           Offset, /*IsPrefetch=*/false)) {
      // Insert an ADD instruction with the materialized Hi52 bits.
      Base = SDValue(
          CurDAG->getMachineNode(YSX::ADD, DL, VT, Addr.getOperand(0), Base),
          0);
      return true;
    }
  }

  if (selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr, Base, Offset,
                         /*IsPrefetch=*/false))
    return true;

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, VT);
  return true;
}

/// Similar to SelectAddrRegImm, except that the offset is restricted to uimm9.
bool YSXDAGToDAGISel::SelectAddrRegImm9(SDValue Addr, SDValue &Base,
                                          SDValue &Offset) {
  if (SelectAddrFrameIndex(Addr, Base, Offset))
    return true;

  SDLoc DL(Addr);
  MVT VT = Addr.getSimpleValueType();

  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isUInt<9>(CVal)) {
      Base = Addr.getOperand(0);

      if (auto *FIN = dyn_cast<FrameIndexSDNode>(Base))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), VT);
      Offset = CurDAG->getSignedTargetConstant(CVal, DL, VT);
      return true;
    }
  }

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, VT);
  return true;
}

/// Similar to SelectAddrRegImm, except that the least significant 5 bits of
/// Offset should be all zeros.
bool YSXDAGToDAGISel::SelectAddrRegImmLsb00000(SDValue Addr, SDValue &Base,
                                                 SDValue &Offset) {
  if (SelectAddrFrameIndex(Addr, Base, Offset))
    return true;

  SDLoc DL(Addr);
  MVT VT = Addr.getSimpleValueType();

  if (CurDAG->isBaseWithConstantOffset(Addr)) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    if (isInt<12>(CVal)) {
      Base = Addr.getOperand(0);

      // Early-out if not a valid offset.
      if ((CVal & 0b11111) != 0) {
        Base = Addr;
        Offset = CurDAG->getTargetConstant(0, DL, VT);
        return true;
      }

      if (auto *FIN = dyn_cast<FrameIndexSDNode>(Base))
        Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), VT);
      Offset = CurDAG->getSignedTargetConstant(CVal, DL, VT);
      return true;
    }
  }

  // Handle ADD with large immediates.
  if (Addr.getOpcode() == ISD::ADD && isa<ConstantSDNode>(Addr.getOperand(1))) {
    int64_t CVal = cast<ConstantSDNode>(Addr.getOperand(1))->getSExtValue();
    assert(!isInt<12>(CVal) && "simm12 not already handled?");

    // Handle immediates in the range [-4096,-2049] or [2017, 4065]. We can save
    // one instruction by folding adjustment (-2048 or 2016) into the address.
    if ((-2049 >= CVal && CVal >= -4096) || (4065 >= CVal && CVal >= 2017)) {
      int64_t Adj = CVal < 0 ? -2048 : 2016;
      int64_t AdjustedOffset = CVal - Adj;
      Base =
          SDValue(CurDAG->getMachineNode(
                      YSX::ADDI, DL, VT, Addr.getOperand(0),
                      CurDAG->getSignedTargetConstant(AdjustedOffset, DL, VT)),
                  0);
      Offset = CurDAG->getSignedTargetConstant(Adj, DL, VT);
      return true;
    }

    if (selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr.getOperand(1), Base,
                           Offset, /*IsPrefetch=*/true)) {
      // Insert an ADD instruction with the materialized Hi52 bits.
      Base = SDValue(
          CurDAG->getMachineNode(YSX::ADD, DL, VT, Addr.getOperand(0), Base),
          0);
      return true;
    }
  }

  if (selectConstantAddr(CurDAG, DL, VT, Subtarget, Addr, Base, Offset,
                         /*IsPrefetch=*/true))
    return true;

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, VT);
  return true;
}

bool YSXDAGToDAGISel::SelectAddrRegReg(SDValue Addr, SDValue &Base,
                                         SDValue &Offset) {
  if (Addr.getOpcode() != ISD::ADD)
    return false;

  if (isa<ConstantSDNode>(Addr.getOperand(1)))
    return false;

  Base = Addr.getOperand(0);
  Offset = Addr.getOperand(1);
  return true;
}

bool YSXDAGToDAGISel::selectShiftMask(SDValue N, unsigned ShiftWidth,
                                        SDValue &ShAmt) {
  ShAmt = N;

  // Peek through zext.
  if (ShAmt->getOpcode() == ISD::ZERO_EXTEND)
    ShAmt = ShAmt.getOperand(0);

  // Shift instructions on RISC-V only read the lower 5 or 6 bits of the shift
  // amount. If there is an AND on the shift amount, we can bypass it if it
  // doesn't affect any of those bits.
  if (ShAmt.getOpcode() == ISD::AND &&
      isa<ConstantSDNode>(ShAmt.getOperand(1))) {
    const APInt &AndMask = ShAmt.getConstantOperandAPInt(1);

    // Since the max shift amount is a power of 2 we can subtract 1 to make a
    // mask that covers the bits needed to represent all shift amounts.
    assert(isPowerOf2_32(ShiftWidth) && "Unexpected max shift amount!");
    APInt ShMask(AndMask.getBitWidth(), ShiftWidth - 1);

    if (ShMask.isSubsetOf(AndMask)) {
      ShAmt = ShAmt.getOperand(0);
    } else {
      // SimplifyDemandedBits may have optimized the mask so try restoring any
      // bits that are known zero.
      KnownBits Known = CurDAG->computeKnownBits(ShAmt.getOperand(0));
      if (!ShMask.isSubsetOf(AndMask | Known.Zero))
        return true;
      ShAmt = ShAmt.getOperand(0);
    }
  }

  if (ShAmt.getOpcode() == ISD::ADD &&
      isa<ConstantSDNode>(ShAmt.getOperand(1))) {
    uint64_t Imm = ShAmt.getConstantOperandVal(1);
    // If we are shifting by X+N where N == 0 mod Size, then just shift by X
    // to avoid the ADD.
    if (Imm != 0 && Imm % ShiftWidth == 0) {
      ShAmt = ShAmt.getOperand(0);
      return true;
    }
  } else if (ShAmt.getOpcode() == ISD::SUB &&
             isa<ConstantSDNode>(ShAmt.getOperand(0))) {
    uint64_t Imm = ShAmt.getConstantOperandVal(0);
    // If we are shifting by N-X where N == 0 mod Size, then just shift by -X to
    // generate a NEG instead of a SUB of a constant.
    if (Imm != 0 && Imm % ShiftWidth == 0) {
      SDLoc DL(ShAmt);
      EVT VT = ShAmt.getValueType();
      SDValue Zero = CurDAG->getRegister(YSX::X0, VT);
      unsigned NegOpc = VT == MVT::i64 ? YSX::SUBW : YSX::SUB;
      MachineSDNode *Neg = CurDAG->getMachineNode(NegOpc, DL, VT, Zero,
                                                  ShAmt.getOperand(1));
      ShAmt = SDValue(Neg, 0);
      return true;
    }
    // If we are shifting by N-X where N == -1 mod Size, then just shift by ~X
    // to generate a NOT instead of a SUB of a constant.
    if (Imm % ShiftWidth == ShiftWidth - 1) {
      SDLoc DL(ShAmt);
      EVT VT = ShAmt.getValueType();
      MachineSDNode *Not = CurDAG->getMachineNode(
          YSX::XORI, DL, VT, ShAmt.getOperand(1),
          CurDAG->getAllOnesConstant(DL, VT, /*isTarget=*/true));
      ShAmt = SDValue(Not, 0);
      return true;
    }
  }

  return true;
}

/// RISC-V doesn't have general instructions for integer setne/seteq, but we can
/// check for equality with 0. This function emits instructions that convert the
/// seteq/setne into something that can be compared with 0.
/// \p ExpectedCCVal indicates the condition code to attempt to match (e.g.
/// ISD::SETNE).
bool YSXDAGToDAGISel::selectSETCC(SDValue N, ISD::CondCode ExpectedCCVal,
                                    SDValue &Val) {
  assert(ISD::isIntEqualitySetCC(ExpectedCCVal) &&
         "Unexpected condition code!");

  // We're looking for a setcc.
  if (N->getOpcode() != ISD::SETCC)
    return false;

  // Must be an equality comparison.
  ISD::CondCode CCVal = cast<CondCodeSDNode>(N->getOperand(2))->get();
  if (CCVal != ExpectedCCVal)
    return false;

  SDValue LHS = N->getOperand(0);
  SDValue RHS = N->getOperand(1);

  if (!LHS.getValueType().isScalarInteger())
    return false;

  // If the RHS side is 0, we don't need any extra instructions, return the LHS.
  if (isNullConstant(RHS)) {
    Val = LHS;
    return true;
  }

  SDLoc DL(N);

  if (auto *C = dyn_cast<ConstantSDNode>(RHS)) {
    int64_t CVal = C->getSExtValue();
    // If the RHS is -2048, we can use xori to produce 0 if the LHS is -2048 and
    // non-zero otherwise.
    if (CVal == -2048) {
      Val = SDValue(
          CurDAG->getMachineNode(
              YSX::XORI, DL, N->getValueType(0), LHS,
              CurDAG->getSignedTargetConstant(CVal, DL, N->getValueType(0))),
          0);
      return true;
    }
    // If the RHS is [-2047,2048], we can use addi/addiw with -RHS to produce 0
    // if the LHS is equal to the RHS and non-zero otherwise.
    if (isInt<12>(CVal) || CVal == 2048) {
      unsigned Opc = YSX::ADDI;
      if (LHS.getOpcode() == ISD::SIGN_EXTEND_INREG &&
          cast<VTSDNode>(LHS.getOperand(1))->getVT() == MVT::i32) {
        Opc = YSX::ADDIW;
        LHS = LHS.getOperand(0);
      }

      Val = SDValue(CurDAG->getMachineNode(Opc, DL, N->getValueType(0), LHS,
                                           CurDAG->getSignedTargetConstant(
                                               -CVal, DL, N->getValueType(0))),
                    0);
      return true;
    }
    // Larger-immediate equality folding is intentionally omitted for the
    // rv64ima-only YSX surface.
  }

  // If nothing else we can XOR the LHS and RHS to produce zero if they are
  // equal and a non-zero value if they aren't.
  Val = SDValue(
      CurDAG->getMachineNode(YSX::XOR, DL, N->getValueType(0), LHS, RHS), 0);
  return true;
}

bool YSXDAGToDAGISel::selectSExtBits(SDValue N, unsigned Bits, SDValue &Val) {
  if (N.getOpcode() == ISD::SIGN_EXTEND_INREG &&
      cast<VTSDNode>(N.getOperand(1))->getVT().getSizeInBits() == Bits) {
    Val = N.getOperand(0);
    return true;
  }

  auto UnwrapShlSra = [](SDValue N, unsigned ShiftAmt) {
    if (N.getOpcode() != ISD::SRA || !isa<ConstantSDNode>(N.getOperand(1)))
      return N;

    SDValue N0 = N.getOperand(0);
    if (N0.getOpcode() == ISD::SHL && isa<ConstantSDNode>(N0.getOperand(1)) &&
        N.getConstantOperandVal(1) == ShiftAmt &&
        N0.getConstantOperandVal(1) == ShiftAmt)
      return N0.getOperand(0);

    return N;
  };

  MVT VT = N.getSimpleValueType();
  if (CurDAG->ComputeNumSignBits(N) > (VT.getSizeInBits() - Bits)) {
    Val = UnwrapShlSra(N, VT.getSizeInBits() - Bits);
    return true;
  }

  return false;
}

bool YSXDAGToDAGISel::selectZExtBits(SDValue N, unsigned Bits, SDValue &Val) {
  if (N.getOpcode() == ISD::AND) {
    auto *C = dyn_cast<ConstantSDNode>(N.getOperand(1));
    if (C && C->getZExtValue() == maskTrailingOnes<uint64_t>(Bits)) {
      Val = N.getOperand(0);
      return true;
    }
  }
  MVT VT = N.getSimpleValueType();
  APInt Mask = APInt::getBitsSetFrom(VT.getSizeInBits(), Bits);
  if (CurDAG->MaskedValueIsZero(N, Mask)) {
    Val = N;
    return true;
  }

  return false;
}

bool YSXDAGToDAGISel::orDisjoint(const SDNode *N) const {
  assert(N->getOpcode() == ISD::OR);
  if (N->getFlags().hasDisjoint())
    return true;
  return CurDAG->haveNoCommonBitsSet(N->getOperand(0), N->getOperand(1));
}

bool YSXDAGToDAGISel::selectImm64IfCheaper(int64_t Imm, int64_t OrigImm,
                                             SDValue N, SDValue &Val) {
  int OrigCost =
      YSXMatInt::getIntMatCost(APInt(64, OrigImm), 64, *Subtarget);
  int Cost = YSXMatInt::getIntMatCost(APInt(64, Imm), 64, *Subtarget);
  if (OrigCost <= Cost)
    return false;

  Val = selectImm(CurDAG, SDLoc(N), N->getSimpleValueType(0), Imm, *Subtarget);
  return true;
}

bool YSXDAGToDAGISel::selectZExtImm32(SDValue N, SDValue &Val) {
  if (!isa<ConstantSDNode>(N))
    return false;
  int64_t Imm = cast<ConstantSDNode>(N)->getSExtValue();
  if ((Imm >> 31) != 1)
    return false;

  for (const SDNode *U : N->users()) {
    switch (U->getOpcode()) {
    case ISD::ADD:
      break;
    case ISD::OR:
      if (orDisjoint(U))
        break;
      return false;
    default:
      return false;
    }
  }

  return selectImm64IfCheaper(0xffffffff00000000 | Imm, Imm, N, Val);
}

bool YSXDAGToDAGISel::selectNegImm(SDValue N, SDValue &Val) {
  if (!isa<ConstantSDNode>(N))
    return false;
  int64_t Imm = cast<ConstantSDNode>(N)->getSExtValue();
  if (isInt<32>(Imm))
    return false;

  for (const SDNode *U : N->users()) {
    switch (U->getOpcode()) {
    case ISD::ADD:
      break;
    default:
      return false;
    }
  }

  return selectImm64IfCheaper(-Imm, Imm, N, Val);
}

bool YSXDAGToDAGISel::selectInvLogicImm(SDValue N, SDValue &Val) {
  if (!isa<ConstantSDNode>(N))
    return false;
  int64_t Imm = cast<ConstantSDNode>(N)->getSExtValue();

  // For 32-bit signed constants, we can only substitute LUI+ADDI with LUI.
  if (isInt<32>(Imm) && ((Imm & 0xfff) != 0xfff || Imm == -1))
    return false;

  // Abandon this transform if the constant is needed elsewhere.
  for (const SDNode *U : N->users()) {
    switch (U->getOpcode()) {
    case ISD::AND:
    case ISD::OR:
    case ISD::XOR:
      return false;
    default:
      return false;
    }
  }

  if (isInt<32>(Imm)) {
    Val =
        selectImm(CurDAG, SDLoc(N), N->getSimpleValueType(0), ~Imm, *Subtarget);
    return true;
  }

  // For 64-bit constants, the instruction sequences get complex,
  // so we select inverted only if it's cheaper.
  return selectImm64IfCheaper(~Imm, Imm, N, Val);
}

// Return true if all users of this SDNode* only consume the lower \p Bits.
// This can be used to form W instructions for add/sub/mul/shl even when the
// root isn't a sext_inreg. This can allow the ADDW/SUBW/MULW/SLLIW to CSE if
// SimplifyDemandedBits has made it so some users see a sext_inreg and some
// don't. The sext_inreg+add/sub/mul/shl will get selected, but still leave
// the add/sub/mul/shl to become non-W instructions. By checking the users we
// may be able to use a W instruction and CSE with the other instruction if
// this has happened. We could try to detect that the CSE opportunity exists
// before doing this, but that would be more complicated.
bool YSXDAGToDAGISel::hasAllNBitUsers(SDNode *Node, unsigned Bits,
                                        const unsigned Depth) const {
  assert((Node->getOpcode() == ISD::ADD || Node->getOpcode() == ISD::SUB ||
          Node->getOpcode() == ISD::MUL || Node->getOpcode() == ISD::SHL ||
          Node->getOpcode() == ISD::SRL || Node->getOpcode() == ISD::AND ||
          Node->getOpcode() == ISD::OR || Node->getOpcode() == ISD::XOR ||
          Node->getOpcode() == ISD::SIGN_EXTEND_INREG ||
          isa<ConstantSDNode>(Node) || Depth != 0) &&
         "Unexpected opcode");

  if (Depth >= SelectionDAG::MaxRecursionDepth)
    return false;

  // The PatFrags that call this may run before YSXGenDAGISel.inc has checked
  // the VT. Ensure the type is scalar to avoid wasting time on vectors.
  if (Depth == 0 && !Node->getValueType(0).isScalarInteger())
    return false;

  for (SDUse &Use : Node->uses()) {
    SDNode *User = Use.getUser();
    // Users of this node should have already been instruction selected
    if (!User->isMachineOpcode())
      return false;

    // TODO: Add more opcodes?
    switch (User->getMachineOpcode()) {
    default:
      return false;
    case YSX::ADDW:
    case YSX::ADDIW:
    case YSX::SUBW:
    case YSX::MULW:
    case YSX::SLLW:
    case YSX::SLLIW:
    case YSX::SRAW:
    case YSX::SRAIW:
    case YSX::SRLW:
    case YSX::SRLIW:
    case YSX::DIVW:
    case YSX::DIVUW:
    case YSX::REMW:
    case YSX::REMUW:
      if (Bits >= 32)
        break;
      return false;
    case YSX::SLL:
    case YSX::SRA:
    case YSX::SRL:
      // Shift amount operands only use log2(Xlen) bits.
      if (Use.getOperandNo() == 1 && Bits >= Log2_32(Subtarget->getXLen()))
        break;
      return false;
    case YSX::SLLI:
      // SLLI only uses the lower (XLen - ShAmt) bits.
      if (Bits >= Subtarget->getXLen() - User->getConstantOperandVal(1))
        break;
      return false;
    case YSX::ANDI:
      if (Bits >= (unsigned)llvm::bit_width(User->getConstantOperandVal(1)))
        break;
      goto RecCheck;
    case YSX::ORI: {
      uint64_t Imm = cast<ConstantSDNode>(User->getOperand(1))->getSExtValue();
      if (Bits >= (unsigned)llvm::bit_width<uint64_t>(~Imm))
        break;
      [[fallthrough]];
    }
    case YSX::AND:
    case YSX::OR:
    case YSX::XOR:
    case YSX::XORI:
    RecCheck:
      if (hasAllNBitUsers(User, Bits, Depth + 1))
        break;
      return false;
    case YSX::SRLI: {
      unsigned ShAmt = User->getConstantOperandVal(1);
      // If we are shifting right by less than Bits, and users don't demand any
      // bits that were shifted into [Bits-1:0], then we can consider this as an
      // N-Bit user.
      if (Bits > ShAmt && hasAllNBitUsers(User, Bits - ShAmt, Depth + 1))
        break;
      return false;
    }
    case YSX::SB:
      if (Use.getOperandNo() == 0 && Bits >= 8)
        break;
      return false;
    case YSX::SH:
      if (Use.getOperandNo() == 0 && Bits >= 16)
        break;
      return false;
    case YSX::SW:
      if (Use.getOperandNo() == 0 && Bits >= 32)
        break;
      return false;
    }
  }

  return true;
}

// Select a constant that can be represented as (sign_extend(imm5) << imm2).
bool YSXDAGToDAGISel::selectSimm5Shl2(SDValue N, SDValue &Simm5,
                                        SDValue &Shl2) {
  auto *C = dyn_cast<ConstantSDNode>(N);
  if (!C)
    return false;

  int64_t Offset = C->getSExtValue();
  for (unsigned Shift = 0; Shift < 4; Shift++) {
    if (isInt<5>(Offset >> Shift) && ((Offset % (1LL << Shift)) == 0)) {
      EVT VT = N->getValueType(0);
      Simm5 = CurDAG->getSignedTargetConstant(Offset >> Shift, SDLoc(N), VT);
      Shl2 = CurDAG->getTargetConstant(Shift, SDLoc(N), VT);
      return true;
    }
  }

  return false;
}

// Try to remove sext.w if the input is a W instruction or can be made into
// a W instruction cheaply.
bool YSXDAGToDAGISel::doPeepholeSExtW(SDNode *N) {
  // Look for the sext.w pattern, addiw rd, rs1, 0.
  if (N->getMachineOpcode() != YSX::ADDIW ||
      !isNullConstant(N->getOperand(1)))
    return false;

  SDValue N0 = N->getOperand(0);
  if (!N0.isMachineOpcode())
    return false;

  switch (N0.getMachineOpcode()) {
  default:
    break;
  case YSX::ADD:
  case YSX::ADDI:
  case YSX::SUB:
  case YSX::MUL:
  case YSX::SLLI: {
    // Convert sext.w+add/sub/mul to their W instructions. This will create
    // a new independent instruction. This improves latency.
    unsigned Opc;
    switch (N0.getMachineOpcode()) {
    default:
      llvm_unreachable("Unexpected opcode!");
    case YSX::ADD:  Opc = YSX::ADDW;  break;
    case YSX::ADDI: Opc = YSX::ADDIW; break;
    case YSX::SUB:  Opc = YSX::SUBW;  break;
    case YSX::MUL:  Opc = YSX::MULW;  break;
    case YSX::SLLI: Opc = YSX::SLLIW; break;
    }

    SDValue N00 = N0.getOperand(0);
    SDValue N01 = N0.getOperand(1);

    // Shift amount needs to be uimm5.
    if (N0.getMachineOpcode() == YSX::SLLI &&
        !isUInt<5>(cast<ConstantSDNode>(N01)->getSExtValue()))
      break;

    SDNode *Result =
        CurDAG->getMachineNode(Opc, SDLoc(N), N->getValueType(0),
                               N00, N01);
    ReplaceUses(N, Result);
    return true;
  }
  case YSX::ADDW:
  case YSX::ADDIW:
  case YSX::SUBW:
  case YSX::MULW:
  case YSX::SLLIW:
    if (N0.getValueType() == MVT::i32)
      break;

    // Result is already sign extended just remove the sext.w.
    // NOTE: We only handle the nodes that are selected with hasAllWUsers.
    ReplaceUses(N, N0.getNode());
    return true;
  }

  return false;
}

// This pass converts a legalized DAG into a YSX-specific DAG, ready
// for instruction scheduling.
FunctionPass *llvm::createYSXISelDag(YSXTargetMachine &TM,
                                       CodeGenOptLevel OptLevel) {
  return new YSXDAGToDAGISelLegacy(TM, OptLevel);
}

char YSXDAGToDAGISelLegacy::ID = 0;

YSXDAGToDAGISelLegacy::YSXDAGToDAGISelLegacy(YSXTargetMachine &TM,
                                                 CodeGenOptLevel OptLevel)
    : SelectionDAGISelLegacy(
          ID, std::make_unique<YSXDAGToDAGISel>(TM, OptLevel)) {}

INITIALIZE_PASS(YSXDAGToDAGISelLegacy, DEBUG_TYPE, PASS_NAME, false, false)
