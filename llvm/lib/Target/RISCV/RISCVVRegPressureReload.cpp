//===- RISCVVRegPressureReload.cpp - RVV reload rematerialization ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Conservative same-block RVV load cloning for pressure-heavy reduce kernels.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVMachineScheduler.h"
#include "RISCVRegisterInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
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

#define DEBUG_TYPE "riscv-v-reg-pressure-reload"

namespace {

class RISCVVRegPressureReload : public MachineFunctionPass {
public:
  static char ID;

  RISCVVRegPressureReload() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "RISC-V RVV register pressure reload rematerialization";
  }

private:
  const TargetInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;
  MachineRegisterInfo *MRI = nullptr;

  bool isRVVReg(Register Reg) const;
  bool hasRVVUse(const MachineInstr &MI, Register Reg) const;
  bool hasRegDef(const MachineInstr &MI, Register Reg) const;
  bool hasRegDefOrClobber(const MachineInstr &MI, Register Reg) const;
  bool hasRVVRegUse(const MachineInstr &MI) const;
  bool hasRVVRegDef(const MachineInstr &MI) const;
  bool hasHighRVVPressure(const MachineBasicBlock &MBB) const;
  bool isUnsafeMemory(const MachineInstr &MI) const;
  bool isAliasBarrier(const MachineInstr &MI) const;
  bool isReductionUse(const MachineInstr &MI) const;
  bool isFaultFirstLoad(const MachineInstr &MI) const;
  bool isLateElementwiseUse(const MachineInstr &MI) const;
  void collectLoadInputRegs(const MachineInstr &Load,
                            SmallVectorImpl<Register> &Regs) const;
  void clearLoadInputKillFlags(MachineInstr &Load, MachineInstr &Use,
                               ArrayRef<Register> LoadInputRegs) const;
  std::optional<Register> getSimpleRVVLoadDef(const MachineInstr &MI) const;
  bool hasBarrierBetween(const MachineInstr &Load,
                         const MachineInstr &Use,
                         ArrayRef<Register> LoadInputRegs) const;
  bool cloneLoadForUse(MachineFunction &MF, MachineInstr &Load,
                       MachineInstr &Use, Register OldReg);
  bool processLoad(MachineFunction &MF, MachineInstr &Load);
};

} // end anonymous namespace

char RISCVVRegPressureReload::ID = 0;

INITIALIZE_PASS(RISCVVRegPressureReload, DEBUG_TYPE,
                "RISC-V RVV pressure reload rematerialization", false, false)

bool RISCVVRegPressureReload::isRVVReg(Register Reg) const {
  if (!Reg || !Reg.isVirtual())
    return false;
  return RISCVRegisterInfo::isRVVRegClass(MRI->getRegClass(Reg));
}

bool RISCVVRegPressureReload::hasRVVUse(const MachineInstr &MI,
                                        Register Reg) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isUse() && !MO.isUndef() && MO.getReg() == Reg;
  });
}

bool RISCVVRegPressureReload::hasRegDef(const MachineInstr &MI,
                                        Register Reg) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isDef() && MO.getReg() == Reg;
  });
}

bool RISCVVRegPressureReload::hasRegDefOrClobber(const MachineInstr &MI,
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

bool RISCVVRegPressureReload::hasRVVRegUse(const MachineInstr &MI) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isUse() && !MO.isUndef() && isRVVReg(MO.getReg());
  });
}

bool RISCVVRegPressureReload::hasRVVRegDef(const MachineInstr &MI) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isDef() && isRVVReg(MO.getReg());
  });
}

bool RISCVVRegPressureReload::hasHighRVVPressure(
    const MachineBasicBlock &MBB) const {
  unsigned RVVOps = 0;
  unsigned RVVDefs = 0;
  for (const MachineInstr &MI : MBB) {
    bool HasRVVOperand = false;
    bool HasRVVDef = false;
    for (const MachineOperand &MO : MI.operands()) {
      if (!MO.isReg() || !isRVVReg(MO.getReg()))
        continue;
      HasRVVOperand = true;
      HasRVVDef |= MO.isDef();
    }
    RVVOps += HasRVVOperand;
    RVVDefs += HasRVVDef;
  }
  return RVVOps >= 12 && RVVDefs >= 8;
}

bool RISCVVRegPressureReload::isUnsafeMemory(const MachineInstr &MI) const {
  for (MachineMemOperand *MMO : MI.memoperands())
    if (MMO->isVolatile() || MMO->isAtomic() || !MMO->isUnordered())
      return true;
  return false;
}

bool RISCVVRegPressureReload::isAliasBarrier(const MachineInstr &MI) const {
  return MI.mayStore() || MI.isCall() || MI.hasUnmodeledSideEffects() ||
         isUnsafeMemory(MI);
}

bool RISCVVRegPressureReload::isReductionUse(const MachineInstr &MI) const {
  StringRef Name = TII->getName(MI.getOpcode());
  return Name.contains("VRED") || Name.contains("VFRED") ||
         Name.contains("VWRED");
}

bool RISCVVRegPressureReload::isFaultFirstLoad(const MachineInstr &MI) const {
  return TII->getName(MI.getOpcode()).contains("FF_V");
}

bool RISCVVRegPressureReload::isLateElementwiseUse(
    const MachineInstr &MI) const {
  return hasRVVRegUse(MI) && !isReductionUse(MI) &&
         (hasRVVRegDef(MI) || MI.mayStore());
}

void RISCVVRegPressureReload::collectLoadInputRegs(
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

void RISCVVRegPressureReload::clearLoadInputKillFlags(
    MachineInstr &Load, MachineInstr &Use,
    ArrayRef<Register> LoadInputRegs) const {
  for (Register Reg : LoadInputRegs) {
    for (auto I = Load.getIterator(), E = Use.getIterator();; ++I) {
      I->clearRegisterKills(Reg, TRI);
      if (I == E)
        break;
    }
  }
}

std::optional<Register>
RISCVVRegPressureReload::getSimpleRVVLoadDef(const MachineInstr &MI) const {
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

bool RISCVVRegPressureReload::hasBarrierBetween(
    const MachineInstr &Load, const MachineInstr &Use,
    ArrayRef<Register> LoadInputRegs) const {
  if (Load.getParent() != Use.getParent())
    return true;
  for (auto I = std::next(Load.getIterator()), E = Use.getIterator(); I != E;
       ++I) {
    if (isAliasBarrier(*I))
      return true;
    for (Register Reg : LoadInputRegs)
      if (hasRegDefOrClobber(*I, Reg))
        return true;
  }
  return false;
}

bool RISCVVRegPressureReload::cloneLoadForUse(MachineFunction &MF,
                                              MachineInstr &Load,
                                              MachineInstr &Use,
                                              Register OldReg) {
  SmallVector<Register, 8> LoadInputRegs;
  collectLoadInputRegs(Load, LoadInputRegs);
  clearLoadInputKillFlags(Load, Use, LoadInputRegs);

  const TargetRegisterClass *RC = MRI->getRegClass(OldReg);
  Register NewReg = MRI->createVirtualRegister(RC);
  MachineInstr *Clone = MF.CloneMachineInstr(&Load);
  Clone->clearKillInfo();

  for (MachineOperand &MO : Clone->operands()) {
    if (MO.isReg() && MO.isDef() && MO.getReg() == OldReg)
      MO.setReg(NewReg);
    if (MO.isReg() && MO.isUse() && MO.isUndef() && MO.getReg() == OldReg)
      MO.setReg(NewReg);
  }

  MachineBasicBlock *MBB = Use.getParent();
  MBB->insert(Use.getIterator(), Clone);

  bool Rewrote = false;
  for (MachineOperand &MO : Use.operands()) {
    if (MO.isReg() && MO.isUse() && MO.getReg() == OldReg) {
      MO.setReg(NewReg);
      Rewrote = true;
    }
  }
  if (!Rewrote)
    Clone->eraseFromParent();
  return Rewrote;
}

bool RISCVVRegPressureReload::processLoad(MachineFunction &MF,
                                          MachineInstr &Load) {
  std::optional<Register> Def = getSimpleRVVLoadDef(Load);
  if (!Def)
    return false;

  bool SawReductionUse = false;
  SmallVector<Register, 8> LoadInputRegs;
  SmallVector<MachineInstr *, 4> LateUses;
  collectLoadInputRegs(Load, LoadInputRegs);
  MachineBasicBlock *MBB = Load.getParent();
  for (auto I = std::next(Load.getIterator()), E = MBB->instr_end(); I != E;
       ++I) {
    MachineInstr &MI = *I;
    if (hasRegDef(MI, *Def))
      break;
    if (!hasRVVUse(MI, *Def))
      continue;
    if (isReductionUse(MI)) {
      SawReductionUse = true;
      continue;
    }
    if (SawReductionUse && isLateElementwiseUse(MI) && !isAliasBarrier(MI) &&
        !hasBarrierBetween(Load, MI, LoadInputRegs))
      LateUses.push_back(&MI);
  }

  bool Changed = false;
  for (MachineInstr *Use : LateUses)
    Changed |= cloneLoadForUse(MF, Load, *Use, *Def);
  return Changed;
}

bool RISCVVRegPressureReload::runOnMachineFunction(MachineFunction &MF) {
  if (!isRISCVVRegPressureAwareSchedEnabled())
    return false;

  TII = MF.getSubtarget().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  MRI = &MF.getRegInfo();

  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    if (!hasHighRVVPressure(MBB))
      continue;

    SmallVector<MachineInstr *, 8> Loads;
    for (MachineInstr &MI : MBB)
      if (getSimpleRVVLoadDef(MI))
        Loads.push_back(&MI);
    for (MachineInstr *Load : Loads)
      Changed |= processLoad(MF, *Load);
  }
  return Changed;
}

FunctionPass *llvm::createRISCVVRegPressureReloadPass() {
  return new RISCVVRegPressureReload();
}
