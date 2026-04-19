//===-- YSXInstrInfo.h - RISC-V Instruction Information -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the RISC-V implementation of the TargetInstrInfo class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_YSX_YSXINSTRINFO_H
#define LLVM_LIB_TARGET_YSX_YSXINSTRINFO_H

#include "YSX.h"
#include "YSXRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/DiagnosticInfo.h"

#define GET_INSTRINFO_HEADER
#define GET_INSTRINFO_OPERAND_ENUM
#include "YSXGenInstrInfo.inc"
#include "YSXGenRegisterInfo.inc"

namespace llvm {

// If Value is of the form C1<<C2, where C1 = 3, 5 or 9,
// returns log2(C1 - 1) and assigns Shift = C2.
// Otherwise, returns 0.
template <typename T> int isShifted359(T Value, int &Shift) {
  if (Value == 0)
    return 0;
  Shift = llvm::countr_zero(Value);
  switch (Value >> Shift) {
  case 3:
    return 1;
  case 5:
    return 2;
  case 9:
    return 3;
  default:
    return 0;
  }
}

class YSXSubtarget;

static const MachineMemOperand::Flags MONontemporalBit0 =
    MachineMemOperand::MOTargetFlag1;
static const MachineMemOperand::Flags MONontemporalBit1 =
    MachineMemOperand::MOTargetFlag2;

namespace YSXCC {

enum CondCode {
  COND_EQ,
  COND_NE,
  COND_LT,
  COND_GE,
  COND_LTU,
  COND_GEU,
  COND_INVALID
};

CondCode getInverseBranchCondition(CondCode);
unsigned getBrCond(CondCode CC, unsigned SelectOpc = 0);

} // end of namespace YSXCC

class YSXInstrInfo : public YSXGenInstrInfo {
  const YSXRegisterInfo RegInfo;

public:
  explicit YSXInstrInfo(const YSXSubtarget &STI);

  const YSXRegisterInfo &getRegisterInfo() const { return RegInfo; }

  MCInst getNop() const override;

  Register isLoadFromStackSlot(const MachineInstr &MI,
                               int &FrameIndex) const override;
  Register isLoadFromStackSlot(const MachineInstr &MI, int &FrameIndex,
                               TypeSize &MemBytes) const override;
  Register isStoreToStackSlot(const MachineInstr &MI,
                              int &FrameIndex) const override;
  Register isStoreToStackSlot(const MachineInstr &MI, int &FrameIndex,
                              TypeSize &MemBytes) const override;

  bool isReMaterializableImpl(const MachineInstr &MI) const override;

  bool shouldBreakCriticalEdgeToSink(MachineInstr &MI) const override {
    return MI.getOpcode() == YSX::ADDI && MI.getOperand(1).isReg() &&
           MI.getOperand(1).getReg() == YSX::X0;
  }

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                   const DebugLoc &DL, Register DstReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;

  void storeRegToStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register SrcReg,
      bool IsKill, int FrameIndex, const TargetRegisterClass *RC,

      Register VReg,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  void loadRegFromStackSlot(
      MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI, Register DstReg,
      int FrameIndex, const TargetRegisterClass *RC, Register VReg,
      unsigned SubReg = 0,
      MachineInstr::MIFlag Flags = MachineInstr::NoFlags) const override;

  using TargetInstrInfo::foldMemoryOperandImpl;
  MachineInstr *foldMemoryOperandImpl(MachineFunction &MF, MachineInstr &MI,
                                      ArrayRef<unsigned> Ops,
                                      MachineBasicBlock::iterator InsertPt,
                                      int FrameIndex,
                                      LiveIntervals *LIS = nullptr,
                                      VirtRegMap *VRM = nullptr) const override;

  MachineInstr *foldMemoryOperandImpl(
      MachineFunction &MF, MachineInstr &MI, ArrayRef<unsigned> Ops,
      MachineBasicBlock::iterator InsertPt, MachineInstr &LoadMI,
      LiveIntervals *LIS = nullptr) const override;

  // Materializes the given integer Val into DstReg.
  void movImm(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
              const DebugLoc &DL, Register DstReg, uint64_t Val,
              MachineInstr::MIFlag Flag = MachineInstr::NoFlags,
              bool DstRenamable = false, bool DstIsDead = false) const;

  unsigned getInstSizeInBytes(const MachineInstr &MI) const override;

  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;

  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &dl,
                        int *BytesAdded = nullptr) const override;

  void insertIndirectBranch(MachineBasicBlock &MBB,
                            MachineBasicBlock &NewDestBB,
                            MachineBasicBlock &RestoreBB, const DebugLoc &DL,
                            int64_t BrOffset, RegScavenger *RS) const override;

  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;

  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

  bool optimizeCondBranch(MachineInstr &MI) const override;

  MachineBasicBlock *getBranchDestBlock(const MachineInstr &MI) const override;

  bool isBranchOffsetInRange(unsigned BranchOpc,
                             int64_t BrOffset) const override;

  bool analyzeSelect(const MachineInstr &MI,
                     SmallVectorImpl<MachineOperand> &Cond, unsigned &TrueOp,
                     unsigned &FalseOp, bool &Optimizable) const override;

  MachineInstr *optimizeSelect(MachineInstr &MI,
                               SmallPtrSetImpl<MachineInstr *> &SeenMIs,
                               bool) const override;

  bool isAsCheapAsAMove(const MachineInstr &MI) const override;

  std::optional<DestSourcePair>
  isCopyInstrImpl(const MachineInstr &MI) const override;

  bool verifyInstruction(const MachineInstr &MI,
                         StringRef &ErrInfo) const override;

  bool canFoldIntoAddrMode(const MachineInstr &MemI, Register Reg,
                           const MachineInstr &AddrI,
                           ExtAddrMode &AM) const override;

  MachineInstr *emitLdStWithAddr(MachineInstr &MemI,
                                 const ExtAddrMode &AM) const override;

  bool getMemOperandsWithOffsetWidth(
      const MachineInstr &MI, SmallVectorImpl<const MachineOperand *> &BaseOps,
      int64_t &Offset, bool &OffsetIsScalable, LocationSize &Width,
      const TargetRegisterInfo *TRI) const override;

  bool shouldClusterMemOps(ArrayRef<const MachineOperand *> BaseOps1,
                           int64_t Offset1, bool OffsetIsScalable1,
                           ArrayRef<const MachineOperand *> BaseOps2,
                           int64_t Offset2, bool OffsetIsScalable2,
                           unsigned ClusterSize,
                           unsigned NumBytes) const override;

  bool getMemOperandWithOffsetWidth(const MachineInstr &LdSt,
                                    const MachineOperand *&BaseOp,
                                    int64_t &Offset, LocationSize &Width,
                                    const TargetRegisterInfo *TRI) const;

  bool areMemAccessesTriviallyDisjoint(const MachineInstr &MIa,
                                       const MachineInstr &MIb) const override;

  std::pair<unsigned, unsigned>
  decomposeMachineOperandsTargetFlags(unsigned TF) const override;

  ArrayRef<std::pair<unsigned, const char *>>
  getSerializableDirectMachineOperandTargetFlags() const override;

  // Return true if the function can safely be outlined from.
  bool isFunctionSafeToOutlineFrom(MachineFunction &MF,
                                   bool OutlineFromLinkOnceODRs) const override;

  // Return true if MBB is safe to outline from, and return any target-specific
  // information in Flags.
  bool isMBBSafeToOutlineFrom(MachineBasicBlock &MBB,
                              unsigned &Flags) const override;

  bool shouldOutlineFromFunctionByDefault(MachineFunction &MF) const override;

  // Calculate target-specific information for a set of outlining candidates.
  std::optional<std::unique_ptr<outliner::OutlinedFunction>>
  getOutliningCandidateInfo(
      const MachineModuleInfo &MMI,
      std::vector<outliner::Candidate> &RepeatedSequenceLocs,
      unsigned MinRepeats) const override;

  // Return if/how a given MachineInstr should be outlined.
  outliner::InstrType getOutliningTypeImpl(const MachineModuleInfo &MMI,
                                           MachineBasicBlock::iterator &MBBI,
                                           unsigned Flags) const override;

  // Insert a custom frame for outlined functions.
  void buildOutlinedFrame(MachineBasicBlock &MBB, MachineFunction &MF,
                          const outliner::OutlinedFunction &OF) const override;

  // Insert a call to an outlined function into a given basic block.
  MachineBasicBlock::iterator
  insertOutlinedCall(Module &M, MachineBasicBlock &MBB,
                     MachineBasicBlock::iterator &It, MachineFunction &MF,
                     outliner::Candidate &C) const override;

  std::optional<RegImmPair> isAddImmediate(const MachineInstr &MI,
                                           Register Reg) const override;

  bool findCommutedOpIndices(const MachineInstr &MI, unsigned &SrcOpIdx1,
                             unsigned &SrcOpIdx2) const override;
  MachineInstr *commuteInstructionImpl(MachineInstr &MI, bool NewMI,
                                       unsigned OpIdx1,
                                       unsigned OpIdx2) const override;

  bool simplifyInstruction(MachineInstr &MI) const override;

  MachineInstr *convertToThreeAddress(MachineInstr &MI, LiveVariables *LV,
                                      LiveIntervals *LIS) const override;

  // MIR printer helper function to annotate Operands with a comment.
  std::string
  createMIROperandComment(const MachineInstr &MI, const MachineOperand &Op,
                          unsigned OpIdx,
                          const TargetRegisterInfo *TRI) const override;

  /// Generate code to multiply the value in DestReg by Amt - handles all
  /// the common optimizations for this idiom, and supports fallback for
  /// subtargets which don't support multiply instructions.
  void mulImm(MachineFunction &MF, MachineBasicBlock &MBB,
              MachineBasicBlock::iterator II, const DebugLoc &DL,
              Register DestReg, uint32_t Amt, MachineInstr::MIFlag Flag) const;

  bool useMachineCombiner() const override { return true; }

  MachineTraceStrategy getMachineCombinerTraceStrategy() const override;

  CombinerObjective getCombinerObjective(unsigned Pattern) const override;

  bool getMachineCombinerPatterns(MachineInstr &Root,
                                  SmallVectorImpl<unsigned> &Patterns,
                                  bool DoRegPressureReduce) const override;

  void
  finalizeInsInstrs(MachineInstr &Root, unsigned &Pattern,
                    SmallVectorImpl<MachineInstr *> &InsInstrs) const override;

  void genAlternativeCodeSequence(
      MachineInstr &Root, unsigned Pattern,
      SmallVectorImpl<MachineInstr *> &InsInstrs,
      SmallVectorImpl<MachineInstr *> &DelInstrs,
      DenseMap<Register, unsigned> &InstrIdxForVirtReg) const override;

  bool hasReassociableOperands(const MachineInstr &Inst,
                               const MachineBasicBlock *MBB) const override;

  bool hasReassociableSibling(const MachineInstr &Inst,
                              bool &Commuted) const override;

  bool isAssociativeAndCommutative(const MachineInstr &Inst,
                                   bool Invert) const override;

  std::optional<unsigned> getInverseOpcode(unsigned Opcode) const override;

  void getReassociateOperandIndices(
      const MachineInstr &Root, unsigned Pattern,
      std::array<unsigned, 5> &OperandIndices) const override;

  ArrayRef<std::pair<MachineMemOperand::Flags, const char *>>
  getSerializableMachineMemOperandTargetFlags() const override;

  unsigned getTailDuplicateSize(CodeGenOptLevel OptLevel) const override;

  std::unique_ptr<TargetInstrInfo::PipelinerLoopInfo>
  analyzeLoopForPipelining(MachineBasicBlock *LoopBB) const override;

  bool isHighLatencyDef(int Opc) const override;

  /// Return true if pairing the given load or store may be paired with another.
  static bool isPairableLdStInstOpc(unsigned Opc);

  static bool isLdStSafeToPair(const MachineInstr &LdSt,
                               const TargetRegisterInfo *TRI);
#define GET_INSTRINFO_HELPER_DECLS
#include "YSXGenInstrInfo.inc"

  static YSXCC::CondCode getCondFromBranchOpc(unsigned Opc);

  /// Return the result of the evaluation of C0 CC C1, where CC is a
  /// YSXCC::CondCode.
  static bool evaluateCondBranch(YSXCC::CondCode CC, int64_t C0, int64_t C1);

  /// Return true if the operand is a load immediate instruction and
  /// sets Imm to the immediate value.
  static bool isFromLoadImm(const MachineRegisterInfo &MRI,
                            const MachineOperand &Op, int64_t &Imm);

protected:
  const YSXSubtarget &STI;

private:
  unsigned getInstBundleLength(const MachineInstr &MI) const;
};

namespace YSX {

// Returns true if the given MI is an YSXVec instruction opcode for which we may
// expect to see a FrameIndex operand.
bool isYSXVecSpill(const MachineInstr &MI);

// Special immediate for AVL operand of V pseudo instructions to indicate VLMax.
static constexpr int64_t VLMaxSentinel = -1LL;

// Mask assignments for floating-point
static constexpr unsigned FPMASK_Negative_Infinity = 0x001;
static constexpr unsigned FPMASK_Negative_Normal = 0x002;
static constexpr unsigned FPMASK_Negative_Subnormal = 0x004;
static constexpr unsigned FPMASK_Negative_Zero = 0x008;
static constexpr unsigned FPMASK_Positive_Zero = 0x010;
static constexpr unsigned FPMASK_Positive_Subnormal = 0x020;
static constexpr unsigned FPMASK_Positive_Normal = 0x040;
static constexpr unsigned FPMASK_Positive_Infinity = 0x080;
static constexpr unsigned FPMASK_Signaling_NaN = 0x100;
static constexpr unsigned FPMASK_Quiet_NaN = 0x200;
} // namespace YSX

namespace YSXVPseudosTable {

struct PseudoInfo {
  uint16_t Pseudo;
  uint16_t BaseInstr;
};

#define GET_YSXVPseudosTable_DECL
#include "YSXGenSearchableTables.inc"

inline const PseudoInfo *getPseudoInfo(unsigned) { return nullptr; }

} // end namespace YSXVPseudosTable

namespace YSX {

struct YSXMaskedPseudoInfo {
  uint16_t MaskedPseudo;
  uint16_t UnmaskedPseudo;
  uint8_t MaskOpIdx;
};
#define GET_YSXMaskedPseudosTable_DECL
#include "YSXGenSearchableTables.inc"

inline const YSXMaskedPseudoInfo *getMaskedPseudoInfo(unsigned) {
  return nullptr;
}
} // end namespace YSX

} // end namespace llvm
#endif
