//===-- YSXISelLowering.h - RISC-V DAG Lowering Interface -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that RISC-V uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_YSX_YSXISELLOWERING_H
#define LLVM_LIB_TARGET_YSX_YSXISELLOWERING_H

#include "YSX.h"
#include "YSXCallingConv.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include <optional>

namespace llvm {
class InstructionCost;
class YSXSubtarget;
struct YSXRegisterInfo;

class YSXTargetLowering : public TargetLowering {
  const YSXSubtarget &Subtarget;

public:
  explicit YSXTargetLowering(const TargetMachine &TM,
                               const YSXSubtarget &STI);

  const YSXSubtarget &getSubtarget() const { return Subtarget; }

  bool getTgtMemIntrinsic(IntrinsicInfo &Info, const CallBase &I,
                          MachineFunction &MF,
                          unsigned Intrinsic) const override;
  bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM, Type *Ty,
                             unsigned AS,
                             Instruction *I = nullptr) const override;
  bool isLegalICmpImmediate(int64_t Imm) const override;
  bool isLegalAddImmediate(int64_t Imm) const override;
  bool isTruncateFree(Type *SrcTy, Type *DstTy) const override;
  bool isTruncateFree(EVT SrcVT, EVT DstVT) const override;
  bool isTruncateFree(SDValue Val, EVT VT2) const override;
  bool isZExtFree(SDValue Val, EVT VT2) const override;
  bool isSExtCheaperThanZExt(EVT SrcVT, EVT DstVT) const override;
  bool signExtendConstant(const ConstantInt *CI) const override;
  bool isCheapToSpeculateCttz(Type *Ty) const override;
  bool isCheapToSpeculateCtlz(Type *Ty) const override;
  bool isMaskAndCmp0FoldingBeneficial(const Instruction &AndI) const override;
  bool hasAndNotCompare(SDValue Y) const override;
  bool hasAndNot(SDValue Y) const override;
  bool hasBitTest(SDValue X, SDValue Y) const override;
  bool shouldProduceAndByConstByHoistingConstFromShiftsLHSOfAnd(
      SDValue X, ConstantSDNode *XC, ConstantSDNode *CC, SDValue Y,
      unsigned OldShiftOpcode, unsigned NewShiftOpcode,
      SelectionDAG &DAG) const override;
  bool isOffsetFoldingLegal(const GlobalAddressSDNode *GA) const override;

  bool isIntDivCheap(EVT VT, AttributeList Attr) const override;

  MVT getRegisterTypeForCallingConv(LLVMContext &Context, CallingConv::ID CC,
                                    EVT VT) const override;

  /// Return the number of registers for a given MVT, for inline assembly
  unsigned
  getNumRegisters(LLVMContext &Context, EVT VT,
                  std::optional<MVT> RegisterVT = std::nullopt) const override;

  unsigned getNumRegistersForCallingConv(LLVMContext &Context,
                                         CallingConv::ID CC,
                                         EVT VT) const override;

  // Provide custom lowering hooks for some operations.
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  bool targetShrinkDemandedConstant(SDValue Op, const APInt &DemandedBits,
                                    const APInt &DemandedElts,
                                    TargetLoweringOpt &TLO) const override;

  void computeKnownBitsForTargetNode(const SDValue Op,
                                     KnownBits &Known,
                                     const APInt &DemandedElts,
                                     const SelectionDAG &DAG,
                                     unsigned Depth) const override;
  unsigned ComputeNumSignBitsForTargetNode(SDValue Op,
                                           const APInt &DemandedElts,
                                           const SelectionDAG &DAG,
                                           unsigned Depth) const override;

  bool SimplifyDemandedBitsForTargetNode(SDValue Op, const APInt &DemandedBits,
                                         const APInt &DemandedElts,
                                         KnownBits &Known,
                                         TargetLoweringOpt &TLO,
                                         unsigned Depth) const override;

  bool canCreateUndefOrPoisonForTargetNode(SDValue Op,
                                           const APInt &DemandedElts,
                                           const SelectionDAG &DAG,
                                           bool PoisonOnly, bool ConsiderFlags,
                                           unsigned Depth) const override;

  const Constant *getTargetConstantFromLoad(LoadSDNode *LD) const override;

  MachineMemOperand::Flags
  getTargetMMOFlags(const Instruction &I) const override;

  MachineMemOperand::Flags
  getTargetMMOFlags(const MemSDNode &Node) const override;

  bool
  areTwoSDNodeTargetMMOFlagsMergeable(const MemSDNode &NodeX,
                                      const MemSDNode &NodeY) const override;

  ConstraintType getConstraintType(StringRef Constraint) const override;

  InlineAsm::ConstraintCode
  getInlineAsmMemConstraint(StringRef ConstraintCode) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  void LowerAsmOperandForConstraint(SDValue Op, StringRef Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

  void AdjustInstrPostInstrSelection(MachineInstr &MI,
                                     SDNode *Node) const override;

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override;

  bool shouldFormOverflowOp(unsigned Opcode, EVT VT,
                            bool MathUsed) const override {
    if (VT == MVT::i8 || VT == MVT::i16)
      return false;

    return TargetLowering::shouldFormOverflowOp(Opcode, VT, MathUsed);
  }

  bool convertSetCCLogicToBitwiseLogic(EVT VT) const override {
    return VT.isScalarInteger();
  }
  bool convertSelectOfConstantsToMath(EVT VT) const override { return true; }

  bool isCtpopFast(EVT VT) const override;

  unsigned getCustomCtpopCost(EVT VT, ISD::CondCode Cond) const override;

  bool preferZeroCompareBranch() const override { return true; }

  // Note that one specific case requires fence insertion for an
  // AtomicCmpXchgInst but is handled via the YSXZacasABIFix pass rather
  // than this hook due to limitations in the interface here.
  bool shouldInsertFencesForAtomic(const Instruction *I) const override;

  Instruction *emitLeadingFence(IRBuilderBase &Builder, Instruction *Inst,
                                AtomicOrdering Ord) const override;
  Instruction *emitTrailingFence(IRBuilderBase &Builder, Instruction *Inst,
                                 AtomicOrdering Ord) const override;

  ISD::NodeType getExtendForAtomicOps() const override {
    return ISD::SIGN_EXTEND;
  }

  ISD::NodeType getExtendForAtomicCmpSwapArg() const override;
  ISD::NodeType getExtendForAtomicRMWArg(unsigned Op) const override;

  bool shouldTransformSignedTruncationCheck(EVT XVT,
                                            unsigned KeptBits) const override;

  TargetLowering::ShiftLegalizationStrategy
  preferredShiftLegalizationStrategy(SelectionDAG &DAG, SDNode *N,
                                     unsigned ExpansionFactor) const override {
    if (DAG.getMachineFunction().getFunction().hasMinSize())
      return ShiftLegalizationStrategy::LowerToLibcall;
    return TargetLowering::preferredShiftLegalizationStrategy(DAG, N,
                                                              ExpansionFactor);
  }

  bool isDesirableToCommuteWithShift(const SDNode *N,
                                     CombineLevel Level) const override;

  /// If a physical register, this returns the register that receives the
  /// exception address on entry to an EH pad.
  Register
  getExceptionPointerRegister(const Constant *PersonalityFn) const override;

  /// If a physical register, this returns the register that receives the
  /// exception typeid on entry to a landing pad.
  Register
  getExceptionSelectorRegister(const Constant *PersonalityFn) const override;

  bool shouldExtendTypeInLibCall(EVT Type) const override;
  bool shouldSignExtendTypeInLibCall(Type *Ty, bool IsSigned) const override;

  /// Returns the register with the specified architectural or ABI name. This
  /// method is necessary to lower the llvm.read_register.* and
  /// llvm.write_register.* intrinsics. Allocatable registers must be reserved
  /// with the clang -ffixed-xX flag for access to be allowed.
  Register getRegisterByName(const char *RegName, LLT VT,
                             const MachineFunction &MF) const override;

  // Lower incoming arguments, copy physregs into vregs
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;
  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context, const Type *RetTy) const override;
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;
  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  bool shouldConvertConstantLoadToIntImm(const APInt &Imm,
                                         Type *Ty) const override;
  bool isUsedByReturnOnly(SDNode *N, SDValue &Chain) const override;
  bool mayBeEmittedAsTailCall(const CallInst *CI) const override;
  bool shouldConsiderGEPOffsetSplit() const override { return true; }

  bool decomposeMulByConstant(LLVMContext &Context, EVT VT,
                              SDValue C) const override;

  bool isMulAddWithConstProfitable(SDValue AddNode,
                                   SDValue ConstNode) const override;

  TargetLowering::AtomicExpansionKind
  shouldExpandAtomicRMWInIR(AtomicRMWInst *AI) const override;
  Value *emitMaskedAtomicRMWIntrinsic(IRBuilderBase &Builder, AtomicRMWInst *AI,
                                      Value *AlignedAddr, Value *Incr,
                                      Value *Mask, Value *ShiftAmt,
                                      AtomicOrdering Ord) const override;
  TargetLowering::AtomicExpansionKind
  shouldExpandAtomicCmpXchgInIR(AtomicCmpXchgInst *CI) const override;
  Value *emitMaskedAtomicCmpXchgIntrinsic(IRBuilderBase &Builder,
                                          AtomicCmpXchgInst *CI,
                                          Value *AlignedAddr, Value *CmpVal,
                                          Value *NewVal, Value *Mask,
                                          AtomicOrdering Ord) const override;

  /// Returns true if the target allows unaligned memory accesses of the
  /// specified type.
  bool allowsMisalignedMemoryAccesses(
      EVT VT, unsigned AddrSpace = 0, Align Alignment = Align(1),
      MachineMemOperand::Flags Flags = MachineMemOperand::MONone,
      unsigned *Fast = nullptr) const override;

  bool splitValueIntoRegisterParts(
      SelectionDAG & DAG, const SDLoc &DL, SDValue Val, SDValue *Parts,
      unsigned NumParts, MVT PartVT, std::optional<CallingConv::ID> CC)
      const override;

  SDValue joinRegisterPartsIntoValue(
      SelectionDAG & DAG, const SDLoc &DL, const SDValue *Parts,
      unsigned NumParts, MVT PartVT, EVT ValueVT,
      std::optional<CallingConv::ID> CC) const override;

  unsigned getJumpTableEncoding() const override;

  const MCExpr *LowerCustomJumpTableEntry(const MachineJumpTableInfo *MJTI,
                                          const MachineBasicBlock *MBB,
                                          unsigned uid,
                                          MCContext &Ctx) const override;

  bool getIndexedAddressParts(SDNode *Op, SDValue &Base, SDValue &Offset,
                              ISD::MemIndexedMode &AM, SelectionDAG &DAG) const;
  bool getPreIndexedAddressParts(SDNode *N, SDValue &Base, SDValue &Offset,
                                 ISD::MemIndexedMode &AM,
                                 SelectionDAG &DAG) const override;
  bool getPostIndexedAddressParts(SDNode *N, SDNode *Op, SDValue &Base,
                                  SDValue &Offset, ISD::MemIndexedMode &AM,
                                  SelectionDAG &DAG) const override;

  /// If the target has a standard location for the stack protector cookie,
  /// returns the address of that location. Otherwise, returns nullptr.
  Value *getIRStackGuard(IRBuilderBase &IRB) const override;

  bool supportKCFIBundles() const override { return true; }

  SDValue expandIndirectJTBranch(const SDLoc &dl, SDValue Value, SDValue Addr,
                                 int JTI, SelectionDAG &DAG) const override;

  MachineInstr *EmitKCFICheck(MachineBasicBlock &MBB,
                              MachineBasicBlock::instr_iterator &MBBI,
                              const TargetInstrInfo *TII) const override;

  /// True if stack clash protection is enabled for this functions.
  bool hasInlineStackProbe(const MachineFunction &MF) const override;

  unsigned getStackProbeSize(const MachineFunction &MF, Align StackAlign) const;

  MachineBasicBlock *emitDynamicProbedAlloc(MachineInstr &MI,
                                            MachineBasicBlock *MBB) const;

  ArrayRef<MCPhysReg> getRoundingControlRegisters() const override;

  bool shouldFoldMaskToVariableShiftPair(SDValue Y) const override;

  /// Control the following reassociation of operands: (op (op x, c1), y) -> (op
  /// (op x, y), c1) where N0 is (op x, c1) and N1 is y.
  bool isReassocProfitable(SelectionDAG &DAG, SDValue N0,
                           SDValue N1) const override;

private:
  void analyzeInputArgs(MachineFunction &MF, CCState &CCInfo,
                        const SmallVectorImpl<ISD::InputArg> &Ins, bool IsRet,
                        YSXCCAssignFn Fn) const;
  void analyzeOutputArgs(MachineFunction &MF, CCState &CCInfo,
                         const SmallVectorImpl<ISD::OutputArg> &Outs,
                         bool IsRet, CallLoweringInfo *CLI,
                         YSXCCAssignFn Fn) const;

  template <class NodeTy>
  SDValue getAddr(NodeTy *N, SelectionDAG &DAG, bool IsLocal = true,
                  bool IsExternWeak = false) const;
  SDValue getStaticTLSAddr(GlobalAddressSDNode *N, SelectionDAG &DAG,
                           bool UseGOT) const;
  SDValue getDynamicTLSAddr(GlobalAddressSDNode *N, SelectionDAG &DAG) const;
  SDValue getTLSDescAddr(GlobalAddressSDNode *N, SelectionDAG &DAG) const;

  SDValue lowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerGlobalTLSAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSELECT(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerBRCOND(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerShiftLeftParts(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerShiftRightParts(SDValue Op, SelectionDAG &DAG, bool IsSRA) const;
  SDValue LowerINTRINSIC_WO_CHAIN(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerINTRINSIC_W_CHAIN(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerINTRINSIC_VOID(SDValue Op, SelectionDAG &DAG) const;

  SDValue lowerEH_DWARF_CFA(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerCTLZ_CTTZ_ZERO_UNDEF(SDValue Op, SelectionDAG &DAG) const;

  SDValue lowerDYNAMIC_STACKALLOC(SDValue Op, SelectionDAG &DAG) const;

  SDValue lowerINIT_TRAMPOLINE(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerADJUST_TRAMPOLINE(SDValue Op, SelectionDAG &DAG) const;

  bool isEligibleForTailCallOptimization(
      CCState &CCInfo, CallLoweringInfo &CLI, MachineFunction &MF,
      const SmallVector<CCValAssign, 16> &ArgLocs) const;

  /// Generate error diagnostics if any register used by CC has been marked
  /// reserved.
  void validateCCReservedRegs(
      const SmallVectorImpl<std::pair<llvm::Register, llvm::SDValue>> &Regs,
      MachineFunction &MF) const;

  /// Disable normalizing
  /// select(N0&N1, X, Y) => select(N0, select(N1, X, Y), Y) and
  /// select(N0|N1, X, Y) => select(N0, select(N1, X, Y, Y))
  /// RISC-V doesn't have flags so it's better to perform the and/or in a GPR.
  bool shouldNormalizeToSelectSequence(LLVMContext &, EVT) const override {
    return false;
  }

  SDValue BuildSDIVPow2(SDNode *N, const APInt &Divisor, SelectionDAG &DAG,
                        SmallVectorImpl<SDNode *> &Created) const override;

  bool shouldFoldSelectWithSingleBitTest(EVT VT,
                                         const APInt &AndMask) const override;

  unsigned getMinimumJumpTableEntries() const override;

  SDValue emitFlushICache(SelectionDAG &DAG, SDValue InChain, SDValue Start,
                          SDValue End, SDValue Flags, SDLoc DL) const;

};

} // end namespace llvm

#endif
