//===- RISCVVRegPressureRemat.cpp - RVV pressure rematerialization ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Conservative same-block RVV def cloning for pressure-heavy reduce kernels.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVMachineScheduler.h"
#include "RISCVRegisterInfo.h"
#include "MCTargetDesc/RISCVBaseInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include <iterator>
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "riscv-v-reg-pressure-remat"

namespace {

class RISCVVRegPressureRemat : public MachineFunctionPass {
public:
  static char ID;

  RISCVVRegPressureRemat() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<AAResultsWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return "RISC-V RVV register pressure rematerialization";
  }

private:
  const TargetInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;
  MachineRegisterInfo *MRI = nullptr;
  AliasAnalysis *AA = nullptr;

  bool isRVVReg(Register Reg) const;
  bool hasRVVUse(const MachineInstr &MI, Register Reg) const;
  bool hasRegDef(const MachineInstr &MI, Register Reg) const;
  bool hasRegDefOrClobber(const MachineInstr &MI, Register Reg) const;
  bool hasRVVRegUse(const MachineInstr &MI) const;
  bool hasRVVRegDef(const MachineInstr &MI) const;
  MachineInstr *getUniqueVRegDef(Register Reg) const;
  unsigned getRVVRegWeight(Register Reg) const;
  bool hasHighRVVPressure(const MachineBasicBlock &MBB) const;
  bool isUnsafeMemory(const MachineInstr &MI) const;
  bool isAliasBarrier(const MachineInstr &MI) const;
  bool isReductionUse(const MachineInstr &MI) const;
  bool isReductionResultMoveLikeDef(const MachineInstr &MI) const;
  bool isReductionResultDef(
      const MachineInstr &MI,
      SmallPtrSetImpl<const MachineInstr *> &Visited) const;
  bool isReductionResultReg(
      Register Reg, SmallPtrSetImpl<const MachineInstr *> &Visited) const;
  bool isReductionResultDef(const MachineInstr &MI) const;
  bool isReductionRematBarrier(const MachineInstr &MI) const;
  bool isFaultFirstLoad(const MachineInstr &MI) const;
  bool hasDynamicFRMUse(const MachineInstr &MI) const;
  bool blockDefinesFRM(const MachineBasicBlock &MBB) const;
  bool isMovableElementwiseDef(const MachineInstr &MI) const;
  bool isPureRecomputableDef(const MachineInstr &MI) const;
  bool isLateElementwiseUse(const MachineInstr &MI) const;
  std::optional<Register> getCloneableRVVDef(const MachineInstr &MI) const;
  bool reachesReductionBefore(Register Reg, const MachineInstr &Limit,
                              SmallVectorImpl<Register> &Visited) const;
  bool reachesReductionBefore(Register Reg, const MachineInstr &Limit) const;
  void collectLoadInputRegs(const MachineInstr &Load,
                            SmallVectorImpl<Register> &Regs) const;
  void collectRecomputeInputRegs(const MachineInstr &DefMI,
                                 SmallVectorImpl<Register> &Regs) const;
  void clearInputKillFlags(MachineInstr &DefMI, MachineInstr &Use,
                           ArrayRef<Register> InputRegs) const;
  unsigned countInstrsBetween(const MachineInstr &DefMI,
                              const MachineInstr &Use) const;
  bool improvesWeightedPressure(Register Reg, const MachineInstr &DefMI,
                                const MachineInstr &Use) const;
  std::optional<Register> getSimpleRVVLoadDef(const MachineInstr &MI) const;
  std::optional<Register> getSingleRVVDef(const MachineInstr &MI) const;
  bool hasBarrierBetween(const MachineInstr &Load, const MachineInstr &Use,
                         ArrayRef<Register> LoadInputRegs) const;
  bool isBeforeInBlock(const MachineInstr &First,
                       const MachineInstr &Second) const;
  bool cloneDefChainForUse(MachineFunction &MF, MachineInstr &DefMI,
                           MachineInstr &Use, Register OldReg,
                           DenseMap<Register, Register> &ClonedRegs,
                           SmallPtrSetImpl<MachineInstr *> &Visiting,
                           SmallVectorImpl<MachineInstr *> &Created);
  bool cloneDefChainForUse(MachineFunction &MF, MachineInstr &DefMI,
                           MachineInstr &Use, Register OldReg);
  bool processDef(MachineFunction &MF, MachineInstr &DefMI);
};

} // end anonymous namespace

char RISCVVRegPressureRemat::ID = 0;

INITIALIZE_PASS_BEGIN(RISCVVRegPressureRemat, DEBUG_TYPE,
                      "RISC-V RVV pressure rematerialization", false,
                      false)
INITIALIZE_PASS_DEPENDENCY(AAResultsWrapperPass)
INITIALIZE_PASS_END(RISCVVRegPressureRemat, DEBUG_TYPE,
                    "RISC-V RVV pressure rematerialization", false,
                    false)

bool RISCVVRegPressureRemat::isRVVReg(Register Reg) const {
  if (!Reg || !Reg.isVirtual())
    return false;
  return RISCVRegisterInfo::isRVVRegClass(MRI->getRegClass(Reg));
}

bool RISCVVRegPressureRemat::hasRVVUse(const MachineInstr &MI,
                                        Register Reg) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isUse() && !MO.isUndef() && MO.getReg() == Reg;
  });
}

bool RISCVVRegPressureRemat::hasRegDef(const MachineInstr &MI,
                                        Register Reg) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isDef() && MO.getReg() == Reg;
  });
}

bool RISCVVRegPressureRemat::hasRegDefOrClobber(const MachineInstr &MI,
                                                 Register Reg) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    if (MO.isRegMask())
      return Reg.isPhysical() && MO.clobbersPhysReg(Reg);
    if (!MO.isReg() || !MO.isDef())
      return false;
    Register DefReg = MO.getReg();
    return DefReg && TRI->regsOverlap(DefReg, Reg);
  });
}

bool RISCVVRegPressureRemat::hasRVVRegUse(const MachineInstr &MI) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isUse() && !MO.isUndef() && isRVVReg(MO.getReg());
  });
}

bool RISCVVRegPressureRemat::hasRVVRegDef(const MachineInstr &MI) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isDef() && isRVVReg(MO.getReg());
  });
}

MachineInstr *RISCVVRegPressureRemat::getUniqueVRegDef(Register Reg) const {
  if (!Reg || !Reg.isVirtual())
    return nullptr;

  auto I = MRI->def_instr_begin(Reg);
  auto E = MRI->def_instr_end();
  if (I == E)
    return nullptr;
  MachineInstr *DefMI = &*I++;
  if (I != E)
    return nullptr;
  return DefMI;
}

unsigned RISCVVRegPressureRemat::getRVVRegWeight(Register Reg) const {
  if (!isRVVReg(Reg))
    return 0;

  const TargetRegisterClass *RC = MRI->getRegClass(Reg);
  if (RISCV::VRM8RegClass.hasSubClassEq(RC))
    return 8;
  if (RISCV::VRM4RegClass.hasSubClassEq(RC))
    return 4;
  if (RISCV::VRM2RegClass.hasSubClassEq(RC))
    return 2;
  return 1;
}

bool RISCVVRegPressureRemat::hasHighRVVPressure(
    const MachineBasicBlock &MBB) const {
  unsigned RVVOps = 0;
  unsigned WeightedRVVDefs = 0;
  for (const MachineInstr &MI : MBB) {
    bool HasRVVOperand = false;
    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !isRVVReg(MO.getReg()))
        continue;
      HasRVVOperand = true;
      if (MO.isDef())
        WeightedRVVDefs += getRVVRegWeight(MO.getReg());
    }
    RVVOps += HasRVVOperand;
  }
  return RVVOps >= 6 && WeightedRVVDefs >= 16;
}

bool RISCVVRegPressureRemat::isUnsafeMemory(const MachineInstr &MI) const {
  for (MachineMemOperand *MMO : MI.memoperands())
    if (MMO->isVolatile() || MMO->isAtomic() || !MMO->isUnordered())
      return true;
  return false;
}

bool RISCVVRegPressureRemat::isAliasBarrier(const MachineInstr &MI) const {
  return MI.isCall() || MI.hasUnmodeledSideEffects() || isUnsafeMemory(MI);
}

bool RISCVVRegPressureRemat::isReductionUse(const MachineInstr &MI) const {
  StringRef Name = TII->getName(MI.getOpcode());
  return Name.contains("VRED") || Name.contains("VFRED") ||
         Name.contains("VWRED");
}

bool RISCVVRegPressureRemat::isReductionResultMoveLikeDef(
    const MachineInstr &MI) const {
  if (MI.isCopyLike() || MI.isInsertSubreg() || MI.isExtractSubreg() ||
      MI.isRegSequence())
    return true;

  StringRef Name = TII->getName(MI.getOpcode());
  return Name.contains("VMV") || Name.contains("VFMV");
}

bool RISCVVRegPressureRemat::isReductionResultReg(
    Register Reg, SmallPtrSetImpl<const MachineInstr *> &Visited) const {
  if (!Reg || !Reg.isVirtual())
    return false;

  MachineInstr *DefMI = getUniqueVRegDef(Reg);
  return DefMI && isReductionResultDef(*DefMI, Visited);
}

bool RISCVVRegPressureRemat::isReductionResultDef(
    const MachineInstr &MI,
    SmallPtrSetImpl<const MachineInstr *> &Visited) const {
  if (isReductionUse(MI))
    return true;
  if (!isReductionResultMoveLikeDef(MI))
    return false;
  if (!Visited.insert(&MI).second)
    return false;

  for (const MachineOperand &MO : MI.operands())
    if (MO.isReg() && MO.isUse() && !MO.isUndef() &&
        isReductionResultReg(MO.getReg(), Visited))
      return true;

  return false;
}

bool RISCVVRegPressureRemat::isReductionResultDef(
    const MachineInstr &MI) const {
  SmallPtrSet<const MachineInstr *, 8> Visited;
  return isReductionResultDef(MI, Visited);
}

bool RISCVVRegPressureRemat::isReductionRematBarrier(
    const MachineInstr &MI) const {
  return isReductionResultDef(MI);
}

bool RISCVVRegPressureRemat::isFaultFirstLoad(const MachineInstr &MI) const {
  return TII->getName(MI.getOpcode()).contains("FF_V");
}

bool RISCVVRegPressureRemat::hasDynamicFRMUse(
    const MachineInstr &MI) const {
  int FRMIdx = RISCVII::getFRMOpNum(MI.getDesc());
  return FRMIdx >= 0 && FRMIdx < static_cast<int>(MI.getNumOperands()) &&
         MI.getOperand(FRMIdx).isImm() &&
         MI.getOperand(FRMIdx).getImm() == RISCVFPRndMode::DYN;
}

bool RISCVVRegPressureRemat::blockDefinesFRM(
    const MachineBasicBlock &MBB) const {
  return any_of(MBB, [&](const MachineInstr &MI) {
    return hasRegDefOrClobber(MI, RISCV::FRM);
  });
}

bool RISCVVRegPressureRemat::isMovableElementwiseDef(
    const MachineInstr &MI) const {
  if (MI.mayLoadOrStore() || MI.isCall() || MI.hasUnmodeledSideEffects() ||
      MI.isCopy() || MI.isPHI() || isReductionUse(MI) || !hasRVVRegDef(MI) ||
      !hasRVVRegUse(MI) || MI.modifiesRegister(RISCV::VXSAT, TRI))
    return false;
  if (hasDynamicFRMUse(MI) && blockDefinesFRM(*MI.getParent()))
    return false;

  StringRef Name = TII->getName(MI.getOpcode());
  if (Name.contains("_MASK") || Name.contains("SEG") || Name.contains("VLUX") ||
      Name.contains("VLOX") || Name.contains("VSUX") || Name.contains("VSOX") ||
      Name.contains("GATHER") || Name.contains("SCATTER"))
    return false;

  return !Name.contains("VF") || MI.getFlag(MachineInstr::MIFlag::NoFPExcept);
}

static bool hasFullyUnsafeFPMathForRecompute(const MachineInstr &MI) {
  return MI.getFlag(MachineInstr::MIFlag::NoFPExcept) &&
         MI.getFlag(MachineInstr::MIFlag::FmNoNans) &&
         MI.getFlag(MachineInstr::MIFlag::FmNoInfs) &&
         MI.getFlag(MachineInstr::MIFlag::FmNsz) &&
         MI.getFlag(MachineInstr::MIFlag::FmArcp) &&
         MI.getFlag(MachineInstr::MIFlag::FmContract) &&
         MI.getFlag(MachineInstr::MIFlag::FmAfn) &&
         MI.getFlag(MachineInstr::MIFlag::FmReassoc);
}

bool RISCVVRegPressureRemat::isPureRecomputableDef(
    const MachineInstr &MI) const {
  if (!isMovableElementwiseDef(MI))
    return false;
  if (isReductionResultDef(MI))
    return false;

  StringRef Name = TII->getName(MI.getOpcode());
  if (Name.contains("VF") || Name.contains("YUSHUXIN"))
    return hasFullyUnsafeFPMathForRecompute(MI);

  return true;
}

bool RISCVVRegPressureRemat::isLateElementwiseUse(
    const MachineInstr &MI) const {
  return hasRVVRegUse(MI) && !isReductionUse(MI) &&
         (hasRVVRegDef(MI) || MI.mayStore());
}

std::optional<Register>
RISCVVRegPressureRemat::getCloneableRVVDef(const MachineInstr &MI) const {
  if (std::optional<Register> Def = getSimpleRVVLoadDef(MI))
    return Def;
  if (isPureRecomputableDef(MI))
    return getSingleRVVDef(MI);
  return std::nullopt;
}

bool RISCVVRegPressureRemat::reachesReductionBefore(
    Register Reg, const MachineInstr &Limit,
    SmallVectorImpl<Register> &Visited) const {
  if (!Reg || !Reg.isVirtual() || is_contained(Visited, Reg))
    return false;
  Visited.push_back(Reg);

  for (const MachineOperand &MO : MRI->use_nodbg_operands(Reg)) {
    if (MO.isUndef())
      continue;
    const MachineInstr *UseMI = MO.getParent();
    if (!UseMI || UseMI->getParent() != Limit.getParent() ||
        !isBeforeInBlock(*UseMI, Limit))
      continue;
    if (isReductionRematBarrier(*UseMI))
      return true;
    if (!isMovableElementwiseDef(*UseMI))
      continue;
    if (std::optional<Register> Def = getSingleRVVDef(*UseMI))
      if (reachesReductionBefore(*Def, Limit, Visited))
        return true;
  }

  return false;
}

bool RISCVVRegPressureRemat::reachesReductionBefore(
    Register Reg, const MachineInstr &Limit) const {
  SmallVector<Register, 8> Visited;
  return reachesReductionBefore(Reg, Limit, Visited);
}

void RISCVVRegPressureRemat::collectLoadInputRegs(
    const MachineInstr &Load, SmallVectorImpl<Register> &Regs) const {
  for (const MachineOperand &MO : Load.operands()) {
    if (!MO.isReg() || !MO.isUse() || MO.isUndef())
      continue;
    Register Reg = MO.getReg();
    if (!Reg || isRVVReg(Reg) || is_contained(Regs, Reg))
      continue;
    Regs.push_back(Reg);
  }
}

void RISCVVRegPressureRemat::collectRecomputeInputRegs(
    const MachineInstr &DefMI, SmallVectorImpl<Register> &Regs) const {
  for (const MachineOperand &MO : DefMI.operands()) {
    if (!MO.isReg() || !MO.isUse() || MO.isUndef())
      continue;
    Register Reg = MO.getReg();
    if (!Reg || is_contained(Regs, Reg))
      continue;
    Regs.push_back(Reg);
  }
}

void RISCVVRegPressureRemat::clearInputKillFlags(
    MachineInstr &DefMI, MachineInstr &Use,
    ArrayRef<Register> InputRegs) const {
  for (Register Reg : InputRegs) {
    for (auto I = DefMI.getIterator(), E = Use.getIterator();; ++I) {
      I->clearRegisterKills(Reg, TRI);
      if (I == E)
        break;
    }
  }
}

unsigned
RISCVVRegPressureRemat::countInstrsBetween(const MachineInstr &DefMI,
                                            const MachineInstr &Use) const {
  if (DefMI.getParent() != Use.getParent())
    return 0;

  unsigned Count = 0;
  for (auto I = std::next(DefMI.getIterator()), E = Use.getIterator(); I != E;
       ++I)
    if (!I->isDebugInstr())
      ++Count;
  return Count;
}

bool RISCVVRegPressureRemat::improvesWeightedPressure(
    Register Reg, const MachineInstr &DefMI, const MachineInstr &Use) const {
  unsigned Weight = getRVVRegWeight(Reg);
  if (!Weight)
    return false;
  return countInstrsBetween(DefMI, Use) >= Weight;
}

std::optional<Register>
RISCVVRegPressureRemat::getSimpleRVVLoadDef(const MachineInstr &MI) const {
  if (!MI.mayLoad() || MI.mayStore() || MI.hasUnmodeledSideEffects() ||
      MI.memoperands_empty() || isUnsafeMemory(MI) || isFaultFirstLoad(MI))
    return std::nullopt;

  Register Def;
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg())
      continue;
    if (MO.isUse() && isRVVReg(MO.getReg()) && !MO.isUndef())
      return std::nullopt;
    if (MO.isDef() && isRVVReg(MO.getReg())) {
      if (Def)
        return std::nullopt;
      Def = MO.getReg();
    }
  }
  if (!Def)
    return std::nullopt;
  return Def;
}

std::optional<Register>
RISCVVRegPressureRemat::getSingleRVVDef(const MachineInstr &MI) const {
  Register Def;
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isDef() || !isRVVReg(MO.getReg()))
      continue;
    if (Def)
      return std::nullopt;
    Def = MO.getReg();
  }
  if (!Def)
    return std::nullopt;
  return Def;
}

bool RISCVVRegPressureRemat::hasBarrierBetween(
    const MachineInstr &Load, const MachineInstr &Use,
    ArrayRef<Register> LoadInputRegs) const {
  if (Load.getParent() != Use.getParent())
    return true;
  for (auto I = std::next(Load.getIterator()), E = Use.getIterator(); I != E;
       ++I) {
    if (isAliasBarrier(*I))
      return true;
    if (Load.mayLoad() && I->mayStore() &&
        Load.mayAlias(AA, *I, /*UseTBAA*/ false))
      return true;
    for (Register Reg : LoadInputRegs)
      if (hasRegDefOrClobber(*I, Reg))
        return true;
  }
  return false;
}

bool RISCVVRegPressureRemat::isBeforeInBlock(
    const MachineInstr &First, const MachineInstr &Second) const {
  if (First.getParent() != Second.getParent() || &First == &Second)
    return false;

  for (auto I = First.getIterator(), E = First.getParent()->instr_end(); I != E;
       ++I)
    if (&*I == &Second)
      return true;
  return false;
}

bool RISCVVRegPressureRemat::cloneDefChainForUse(
    MachineFunction &MF, MachineInstr &DefMI, MachineInstr &Use,
    Register OldReg, DenseMap<Register, Register> &ClonedRegs,
    SmallPtrSetImpl<MachineInstr *> &Visiting,
    SmallVectorImpl<MachineInstr *> &Created) {
  if (ClonedRegs.contains(OldReg))
    return true;
  if (isReductionRematBarrier(DefMI) || isReductionRematBarrier(Use))
    return false;
  if (DefMI.getParent() != Use.getParent() || !isBeforeInBlock(DefMI, Use))
    return false;
  if (!Visiting.insert(&DefMI).second)
    return false;
  unsigned CreatedStart = Created.size();
  auto Fail = [&]() {
    while (Created.size() != CreatedStart)
      Created.pop_back_val()->eraseFromParent();
    Visiting.erase(&DefMI);
    return false;
  };

  std::optional<Register> Def = getCloneableRVVDef(DefMI);
  if (!Def || *Def != OldReg)
    return Fail();

  SmallVector<Register, 8> InputRegs;
  if (DefMI.mayLoad())
    collectLoadInputRegs(DefMI, InputRegs);
  else
    collectRecomputeInputRegs(DefMI, InputRegs);
  if (hasBarrierBetween(DefMI, Use, InputRegs))
    return Fail();

  for (const MachineOperand &MO : DefMI.operands()) {
    if (!MO.isReg() || !MO.isUse() || MO.isUndef())
      continue;
    Register InputReg = MO.getReg();
    if (!isRVVReg(InputReg))
      continue;
    MachineInstr *InputDef = getUniqueVRegDef(InputReg);
    if (!InputDef || InputDef->getParent() != Use.getParent() ||
        !isBeforeInBlock(*InputDef, Use))
      return Fail();
    if (isReductionRematBarrier(*InputDef))
      continue;
    std::optional<Register> InputCloneDef = getCloneableRVVDef(*InputDef);
    if (!InputCloneDef || *InputCloneDef != InputReg)
      return Fail();
    if (!cloneDefChainForUse(MF, *InputDef, Use, InputReg, ClonedRegs,
                             Visiting, Created))
      return Fail();
  }

  clearInputKillFlags(DefMI, Use, InputRegs);
  const TargetRegisterClass *RC = MRI->getRegClass(OldReg);
  Register NewReg = MRI->createVirtualRegister(RC);
  MachineInstr *Clone = MF.CloneMachineInstr(&DefMI);
  Clone->clearKillInfo();

  for (MachineOperand &MO : Clone->operands()) {
    if (!MO.isReg())
      continue;
    if (MO.isDef() && MO.getReg() == OldReg)
      MO.setReg(NewReg);
    if (MO.isUse()) {
      auto It = ClonedRegs.find(MO.getReg());
      if (It != ClonedRegs.end())
        MO.setReg(It->second);
      else if (MO.isUndef() && MO.getReg() == OldReg)
        MO.setReg(NewReg);
    }
  }

  Use.getParent()->insert(Use.getIterator(), Clone);
  Created.push_back(Clone);
  ClonedRegs[OldReg] = NewReg;
  Visiting.erase(&DefMI);
  return true;
}

bool RISCVVRegPressureRemat::cloneDefChainForUse(MachineFunction &MF,
                                                  MachineInstr &DefMI,
                                                  MachineInstr &Use,
                                                  Register OldReg) {
  bool HasRetargetableUse = false;
  for (const MachineOperand &MO : Use.operands()) {
    if (!MO.isReg() || !MO.isUse() || MO.getReg() != OldReg)
      continue;
    if (MO.isTied())
      return false;
    HasRetargetableUse = true;
  }
  if (!HasRetargetableUse)
    return false;

  DenseMap<Register, Register> ClonedRegs;
  SmallPtrSet<MachineInstr *, 8> Visiting;
  SmallVector<MachineInstr *, 8> Created;
  if (!cloneDefChainForUse(MF, DefMI, Use, OldReg, ClonedRegs, Visiting,
                           Created))
    return false;

  Register NewReg = ClonedRegs.lookup(OldReg);
  if (!NewReg)
    return false;

  bool Rewrote = false;
  for (MachineOperand &MO : Use.operands()) {
    if (MO.isReg() && MO.isUse() && MO.getReg() == OldReg) {
      MO.setReg(NewReg);
      Rewrote = true;
    }
  }
  return Rewrote;
}

bool RISCVVRegPressureRemat::processDef(MachineFunction &MF,
                                         MachineInstr &DefMI) {
  std::optional<Register> Def = getCloneableRVVDef(DefMI);
  if (!Def)
    return false;

  bool SawReductionUse = false;
  SmallVector<Register, 8> InputRegs;
  SmallVector<MachineInstr *, 4> LateUses;
  if (DefMI.mayLoad())
    collectLoadInputRegs(DefMI, InputRegs);
  else
    collectRecomputeInputRegs(DefMI, InputRegs);

  MachineBasicBlock *MBB = DefMI.getParent();
  for (auto I = std::next(DefMI.getIterator()), E = MBB->instr_end(); I != E;
       ++I) {
    MachineInstr &MI = *I;
    if (hasRegDef(MI, *Def))
      break;
    if (!hasRVVUse(MI, *Def))
      continue;
    if (isReductionRematBarrier(MI)) {
      SawReductionUse = true;
      continue;
    }
    if (!SawReductionUse && reachesReductionBefore(*Def, MI))
      SawReductionUse = true;
    if (SawReductionUse && isLateElementwiseUse(MI) && !isAliasBarrier(MI) &&
        !hasBarrierBetween(DefMI, MI, InputRegs) &&
        improvesWeightedPressure(*Def, DefMI, MI))
      LateUses.push_back(&MI);
  }

  bool Changed = false;
  for (MachineInstr *Use : LateUses)
    Changed |= cloneDefChainForUse(MF, DefMI, *Use, *Def);
  return Changed;
}

bool RISCVVRegPressureRemat::runOnMachineFunction(MachineFunction &MF) {
  if (!isRISCVRVVPressureDAGSchedEnabled()) {
    if (isRISCVRVVPressureRematRequested()) {
      static bool Warned = false;
      if (!Warned) {
        errs() << "warning: -riscv-rvv-pressure-remat requires "
                  "-riscv-rvv-pressure-dag-sched; ignoring remat\n";
        Warned = true;
      }
    }
    return false;
  }

  if (!isRISCVRVVPressureRematRequested())
    return false;

  TII = MF.getSubtarget().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  MRI = &MF.getRegInfo();
  AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    if (!hasHighRVVPressure(MBB))
      continue;

    SmallVector<MachineInstr *, 8> Defs;
    for (MachineInstr &MI : MBB)
      if (getCloneableRVVDef(MI))
        Defs.push_back(&MI);
    for (MachineInstr *DefMI : Defs)
      Changed |= processDef(MF, *DefMI);
  }
  return Changed;
}

FunctionPass *llvm::createRISCVVRegPressureRematPass() {
  return new RISCVVRegPressureRemat();
}
