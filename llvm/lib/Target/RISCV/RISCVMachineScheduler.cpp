//===- RISCVMachineScheduler.cpp - MI Scheduler for RISC-V ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "RISCVMachineScheduler.h"
#include "RISCVRegisterInfo.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/CodeGen/ScheduleDAGMutation.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/IR/Argument.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "riscv-prera-sched-strategy"

static cl::opt<bool> EnableRVVPressureDAGSched(
    "riscv-rvv-pressure-dag-sched", cl::Hidden, cl::init(false),
    cl::desc("Enable experimental RISCV RVV pressure-aware MachineScheduler "
             "DAG scheduling"));

static cl::opt<bool> EnableRVVPressureRemat(
    "riscv-rvv-pressure-remat", cl::Hidden, cl::init(false),
    cl::desc("Enable experimental RISCV RVV pressure-aware load "
             "rematerialization and recomputation"));

bool llvm::isRISCVRVVPressureDAGSchedEnabled() {
  return EnableRVVPressureDAGSched;
}

bool llvm::isRISCVRVVPressureRematRequested() { return EnableRVVPressureRemat; }

static bool isRVVReg(const MachineRegisterInfo &MRI, Register Reg) {
  if (!Reg || !Reg.isVirtual())
    return false;
  return RISCVRegisterInfo::isRVVRegClass(MRI.getRegClass(Reg));
}

static unsigned getRVVRegWeight(const MachineRegisterInfo &MRI, Register Reg) {
  if (!isRVVReg(MRI, Reg))
    return 0;

  const TargetRegisterClass *RC = MRI.getRegClass(Reg);
  if (RISCV::VRM8RegClass.hasSubClassEq(RC))
    return 8;
  if (RISCV::VRM4RegClass.hasSubClassEq(RC))
    return 4;
  if (RISCV::VRM2RegClass.hasSubClassEq(RC))
    return 2;
  return 1;
}

static bool hasRVVRegOperand(const MachineRegisterInfo &MRI,
                             const MachineInstr &MI) {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && isRVVReg(MRI, MO.getReg());
  });
}

static bool hasUnsafeMemory(const MachineInstr &MI) {
  if (!MI.mayLoadOrStore())
    return false;
  if (MI.memoperands_empty())
    return true;
  for (MachineMemOperand *MMO : MI.memoperands()) {
    if (MMO->isVolatile() || MMO->isAtomic() || !MMO->isUnordered())
      return true;
    if (!MMO->getSize().hasValue() || !MMO->getValue())
      return true;
  }
  return false;
}

static bool hasUncleanRVVOpcode(const TargetInstrInfo &TII,
                                const MachineInstr &MI) {
  StringRef Name = TII.getName(MI.getOpcode());
  // VRGATHER is a register permute. Keep rejecting indexed memory operations
  // below, but do not reject register-only gathers just because of the name.
  return Name.contains("VRED") || Name.contains("VFRED") ||
         Name.contains("VWRED") || Name.contains("_MASK") ||
         Name.contains("SEG") || Name.contains("VLUX") ||
         Name.contains("VLOX") || Name.contains("VSUX") ||
         Name.contains("VSOX") || Name.contains("SCATTER");
}

static bool isRVVReductionOpcode(const TargetInstrInfo &TII,
                                 const MachineInstr &MI) {
  StringRef Name = TII.getName(MI.getOpcode());
  return Name.contains("VRED") || Name.contains("VFRED") ||
         Name.contains("VWRED");
}

static bool hasUnsupportedRVVPressureOpcode(const TargetInstrInfo &TII,
                                            const MachineInstr &MI) {
  StringRef Name = TII.getName(MI.getOpcode());
  return Name.contains("_MASK") || Name.contains("SEG") ||
         Name.contains("VLUX") || Name.contains("VLOX") ||
         Name.contains("VSUX") || Name.contains("VSOX") ||
         Name.contains("SCATTER");
}

namespace {

struct RVVMemRef {
  const MachineInstr *MI = nullptr;
  const MachineMemOperand *MMO = nullptr;
  const Value *Ptr = nullptr;
  const Value *Base = nullptr;
  int64_t Offset = 0;
  LocationSize Size = LocationSize::beforeOrAfterPointer();
  bool IsStore = false;
};

} // end anonymous namespace

static bool isNoAliasBase(const Value *V) {
  const auto *Arg = dyn_cast_or_null<Argument>(V);
  return Arg && Arg->hasNoAliasAttr();
}

static std::optional<RVVMemRef> getRVVMemRef(const MachineInstr &MI,
                                             const MachineMemOperand &MMO) {
  if (!MMO.getSize().hasValue() || !MMO.getValue())
    return std::nullopt;

  int64_t BaseOffset = 0;
  const MachineFunction *MF = MI.getMF();
  const DataLayout &DL = MF->getDataLayout();
  const Value *Base =
      GetPointerBaseWithConstantOffset(MMO.getValue(), BaseOffset, DL);
  if (!Base)
    return std::nullopt;

  RVVMemRef Ref;
  Ref.MI = &MI;
  Ref.MMO = &MMO;
  Ref.Ptr = MMO.getValue();
  Ref.Base = Base;
  Ref.Offset = BaseOffset + MMO.getOffset();
  Ref.Size = MMO.getSize();
  Ref.IsStore = MMO.isStore();
  return Ref;
}

static bool areDisjointByConstantRange(const RVVMemRef &A, const RVVMemRef &B) {
  if (A.Base != B.Base || !A.Size.hasValue() || !B.Size.hasValue())
    return false;

  TypeSize ASize = A.Size.getValue();
  TypeSize BSize = B.Size.getValue();
  if (ASize.isScalable() || BSize.isScalable())
    return false;

  int64_t AEnd = A.Offset + static_cast<int64_t>(ASize.getFixedValue());
  int64_t BEnd = B.Offset + static_cast<int64_t>(BSize.getFixedValue());
  return AEnd <= B.Offset || BEnd <= A.Offset;
}

static bool areMemRefsIndependent(const RVVMemRef &A, const RVVMemRef &B,
                                  AAResults *AA) {
  if (!A.IsStore && !B.IsStore)
    return true;

  if (AA && A.Ptr && B.Ptr &&
      AA->isNoAlias(MemoryLocation(A.Ptr, A.Size, A.MMO->getAAInfo()),
                    MemoryLocation(B.Ptr, B.Size, B.MMO->getAAInfo())))
    return true;

  if (areDisjointByConstantRange(A, B))
    return true;

  if (A.Base != B.Base && (isNoAliasBase(A.Base) || isNoAliasBase(B.Base)))
    return true;

  return false;
}

static bool hasIndependentMemory(ArrayRef<RVVMemRef> MemRefs, AAResults *AA) {
  for (unsigned I = 0, E = MemRefs.size(); I != E; ++I)
    for (unsigned J = I + 1; J != E; ++J)
      if (!areMemRefsIndependent(MemRefs[I], MemRefs[J], AA))
        return false;
  return true;
}

static bool isCleanRVVPressureRegion(const MachineRegisterInfo &MRI,
                                     const TargetInstrInfo &TII,
                                     ArrayRef<const MachineInstr *> Instrs,
                                     AAResults *AA = nullptr) {
  unsigned WeightedRVVDefs = 0;
  unsigned RVVOps = 0;
  unsigned RVVLoads = 0;
  unsigned RVVStores = 0;
  unsigned RVVPureOps = 0;
  SmallVector<RVVMemRef, 32> MemRefs;

  for (const MachineInstr *MI : Instrs) {
    if (!MI)
      continue;
    if (MI->isCall() || MI->hasUnmodeledSideEffects() || MI->isInlineAsm() ||
        hasUnsafeMemory(*MI))
      return false;
    if (MI->mayLoadOrStore()) {
      for (MachineMemOperand *MMO : MI->memoperands()) {
        std::optional<RVVMemRef> Ref = getRVVMemRef(*MI, *MMO);
        if (!Ref)
          return false;
        MemRefs.push_back(*Ref);
      }
    }

    bool HasRVVOperand = hasRVVRegOperand(MRI, *MI);
    if (!HasRVVOperand)
      continue;

    if (hasUncleanRVVOpcode(TII, *MI))
      return false;

    ++RVVOps;
    bool HasRVVDef = false;
    bool HasRVVUse = false;
    for (const MachineOperand &MO : MI->operands()) {
      if (!MO.isReg())
        continue;
      unsigned Weight = getRVVRegWeight(MRI, MO.getReg());
      if (!Weight)
        continue;
      HasRVVDef |= MO.isDef();
      HasRVVUse |= MO.isUse() && !MO.isUndef();
      if (MO.isDef())
        WeightedRVVDefs += Weight;
    }

    if (MI->mayLoad() && !MI->mayStore() && HasRVVDef)
      ++RVVLoads;
    else if (MI->mayStore() && HasRVVUse)
      ++RVVStores;
    else if (!MI->mayLoadOrStore() && HasRVVDef && HasRVVUse)
      ++RVVPureOps;
  }

  return RVVOps >= 12 && WeightedRVVDefs >= 32 && RVVLoads >= 2 &&
         RVVPureOps >= 1 && RVVStores >= 1 && hasIndependentMemory(MemRefs, AA);
}

static bool isReduceRVVPressureRegion(const MachineRegisterInfo &MRI,
                                      const TargetInstrInfo &TII,
                                      ArrayRef<const MachineInstr *> Instrs,
                                      AAResults *AA = nullptr) {
  unsigned WeightedRVVDefs = 0;
  unsigned RVVOps = 0;
  unsigned RVVLoads = 0;
  unsigned RVVReductions = 0;
  unsigned RVVLateUses = 0;
  SmallVector<RVVMemRef, 32> MemRefs;

  for (const MachineInstr *MI : Instrs) {
    if (!MI)
      continue;
    if (MI->isCall() || MI->hasUnmodeledSideEffects() || MI->isInlineAsm() ||
        hasUnsafeMemory(*MI))
      return false;
    if (MI->mayLoadOrStore()) {
      for (MachineMemOperand *MMO : MI->memoperands()) {
        std::optional<RVVMemRef> Ref = getRVVMemRef(*MI, *MMO);
        if (!Ref)
          return false;
        MemRefs.push_back(*Ref);
      }
    }

    bool HasRVVOperand = hasRVVRegOperand(MRI, *MI);
    if (!HasRVVOperand)
      continue;

    if (hasUnsupportedRVVPressureOpcode(TII, *MI))
      return false;

    ++RVVOps;
    bool HasRVVDef = false;
    bool HasRVVUse = false;
    for (const MachineOperand &MO : MI->operands()) {
      if (!MO.isReg())
        continue;
      unsigned Weight = getRVVRegWeight(MRI, MO.getReg());
      if (!Weight)
        continue;
      HasRVVDef |= MO.isDef();
      HasRVVUse |= MO.isUse() && !MO.isUndef();
      if (MO.isDef())
        WeightedRVVDefs += Weight;
    }

    if (MI->mayLoad() && !MI->mayStore() && HasRVVDef)
      ++RVVLoads;
    if (isRVVReductionOpcode(TII, *MI) && HasRVVUse)
      ++RVVReductions;
    else if ((MI->mayStore() || (!MI->mayLoadOrStore() && HasRVVDef)) &&
             HasRVVUse)
      ++RVVLateUses;
  }

  return RVVOps >= 12 && WeightedRVVDefs >= 32 && RVVLoads >= 2 &&
         RVVReductions >= 1 && RVVLateUses >= 1 &&
         hasIndependentMemory(MemRefs, AA);
}

static bool isCleanRVVPressureRegion(const ScheduleDAGInstrs &DAG,
                                     AAResults *AA = nullptr) {
  SmallVector<const MachineInstr *, 32> Instrs;
  for (const SUnit &SU : DAG.SUnits)
    Instrs.push_back(SU.getInstr());
  return isCleanRVVPressureRegion(DAG.MRI, *DAG.TII, Instrs, AA);
}

static bool isCleanRVVPressureRegion(const MachineRegisterInfo &MRI,
                                     const TargetInstrInfo &TII,
                                     MachineBasicBlock::iterator Begin,
                                     MachineBasicBlock::iterator End,
                                     AAResults *AA = nullptr) {
  SmallVector<const MachineInstr *, 32> Instrs;
  for (auto I = Begin; I != End; ++I)
    Instrs.push_back(&*I);
  return isCleanRVVPressureRegion(MRI, TII, Instrs, AA);
}

static bool isReduceRVVPressureRegion(const ScheduleDAGInstrs &DAG,
                                      AAResults *AA = nullptr) {
  SmallVector<const MachineInstr *, 32> Instrs;
  for (const SUnit &SU : DAG.SUnits)
    Instrs.push_back(SU.getInstr());
  return isReduceRVVPressureRegion(DAG.MRI, *DAG.TII, Instrs, AA);
}

static bool isReduceRVVPressureRegion(const MachineRegisterInfo &MRI,
                                      const TargetInstrInfo &TII,
                                      MachineBasicBlock::iterator Begin,
                                      MachineBasicBlock::iterator End,
                                      AAResults *AA = nullptr) {
  SmallVector<const MachineInstr *, 32> Instrs;
  for (auto I = Begin; I != End; ++I)
    Instrs.push_back(&*I);
  return isReduceRVVPressureRegion(MRI, TII, Instrs, AA);
}

static bool isRVVLoadSU(const MachineRegisterInfo &MRI, const SUnit &SU) {
  const MachineInstr *MI = SU.getInstr();
  return MI && MI->mayLoad() && !MI->mayStore() &&
         any_of(MI->operands(), [&](const MachineOperand &MO) {
           return MO.isReg() && MO.isDef() && isRVVReg(MRI, MO.getReg());
         });
}

static unsigned getRVVLoadDefWeight(const MachineRegisterInfo &MRI,
                                    const SUnit &SU) {
  const MachineInstr *MI = SU.getInstr();
  if (!MI)
    return 0;

  unsigned Weight = 0;
  for (const MachineOperand &MO : MI->operands())
    if (MO.isReg() && MO.isDef())
      Weight += getRVVRegWeight(MRI, MO.getReg());
  return Weight;
}

static bool isRVVPureDefSU(const MachineRegisterInfo &MRI, const SUnit &SU) {
  const MachineInstr *MI = SU.getInstr();
  return MI && !MI->mayLoadOrStore() &&
         any_of(MI->operands(), [&](const MachineOperand &MO) {
           return MO.isReg() && MO.isDef() && isRVVReg(MRI, MO.getReg());
         });
}

static unsigned countDistinctRVVRegUses(const MachineRegisterInfo &MRI,
                                        const MachineInstr &MI) {
  SmallVector<Register, 4> Uses;
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isUse() || MO.isUndef())
      continue;
    Register Reg = MO.getReg();
    if (isRVVReg(MRI, Reg) && !is_contained(Uses, Reg))
      Uses.push_back(Reg);
  }
  return Uses.size();
}

static bool reachesRVVReduction(SUnit &SU, const TargetInstrInfo &TII) {
  SmallVector<SUnit *, 16> Worklist;
  SmallPtrSet<SUnit *, 32> Seen;
  Worklist.push_back(&SU);
  Seen.insert(&SU);
  while (!Worklist.empty()) {
    SUnit *Cur = Worklist.pop_back_val();
    for (SDep &SuccDep : Cur->Succs) {
      if (SuccDep.getKind() != SDep::Data)
        continue;
      SUnit *Succ = SuccDep.getSUnit();
      if (!Succ || !Seen.insert(Succ).second)
        continue;

      const MachineInstr *MI = Succ->getInstr();
      if (MI && isRVVReductionOpcode(TII, *MI))
        return true;
      Worklist.push_back(Succ);
    }
  }
  return false;
}

static bool isRVVTransparentSliceDef(const MachineRegisterInfo &MRI,
                                     const SUnit &SU, bool PreferReduction) {
  if (!isRVVPureDefSU(MRI, SU))
    return false;

  if (!PreferReduction)
    return true;

  const MachineInstr *MI = SU.getInstr();
  return MI && countDistinctRVVRegUses(MRI, *MI) == 1;
}

static bool isRVVReductionMergeBoundary(const MachineRegisterInfo &MRI,
                                        const TargetInstrInfo &TII,
                                        SUnit &SU) {
  const MachineInstr *MI = SU.getInstr();
  return MI && isRVVPureDefSU(MRI, SU) &&
         countDistinctRVVRegUses(MRI, *MI) > 1 &&
         reachesRVVReduction(SU, TII);
}

static SUnit *laterSUnit(SUnit *A, SUnit *B) {
  if (!A)
    return B;
  if (!B)
    return A;
  return A->NodeNum < B->NodeNum ? B : A;
}

static SUnit *findRVVSliceClose(SUnit &LoadSU, const MachineRegisterInfo &MRI,
                                const TargetInstrInfo &TII,
                                bool PreferReduction) {
  SmallVector<SUnit *, 8> Worklist;
  SmallPtrSet<SUnit *, 16> Seen;
  SUnit *LatestClose = nullptr;

  Worklist.push_back(&LoadSU);
  Seen.insert(&LoadSU);
  while (!Worklist.empty()) {
    SUnit *SU = Worklist.pop_back_val();
    for (SDep &SuccDep : SU->Succs) {
      if (SuccDep.getKind() != SDep::Data)
        continue;
      SUnit *Succ = SuccDep.getSUnit();
      if (!Succ || !Seen.insert(Succ).second)
        continue;

      const MachineInstr *MI = Succ->getInstr();
      if (!MI || !hasRVVRegOperand(MRI, *MI))
        continue;

      if (PreferReduction && isRVVReductionOpcode(TII, *MI))
        return Succ;

      if (MI->mayStore()) {
        LatestClose = laterSUnit(LatestClose, Succ);
        continue;
      }

      // A pressure slice follows the lifetime of a load-derived vector value,
      // not the semantic result of the whole expression. In reduction trees,
      // single-input elementwise ops preserve that leaf value, while the first
      // multi-input pure RVV op that feeds a reduction merges several leaf
      // slices into a new accumulator slice. Close the leaf slice at that merge
      // boundary; otherwise all leaf slices close at the final reduction, the
      // close-to-next-load deps tend to become cyclic, and the scheduler may
      // open too many wide temporaries before starting the reduction tree.
      if (PreferReduction && isRVVReductionMergeBoundary(MRI, TII, *Succ)) {
        LatestClose = laterSUnit(LatestClose, Succ);
        continue;
      }

      if (isRVVTransparentSliceDef(MRI, *Succ, PreferReduction))
        Worklist.push_back(Succ);
      else
        LatestClose = laterSUnit(LatestClose, Succ);
    }
  }

  return LatestClose;
}

static bool reachesSUnit(SUnit *From, SUnit *To) {
  if (From == To)
    return true;

  SmallVector<SUnit *, 16> Worklist;
  SmallPtrSet<SUnit *, 32> Seen;
  Worklist.push_back(From);
  Seen.insert(From);
  while (!Worklist.empty()) {
    SUnit *SU = Worklist.pop_back_val();
    for (const SDep &SuccDep : SU->Succs) {
      SUnit *Succ = SuccDep.getSUnit();
      if (!Succ || !Seen.insert(Succ).second)
        continue;
      if (Succ == To)
        return true;
      Worklist.push_back(Succ);
    }
  }
  return false;
}

static bool isRVVMemorySU(const MachineRegisterInfo &MRI, const SUnit *SU) {
  if (!SU)
    return false;
  const MachineInstr *MI = SU->getInstr();
  return MI && MI->mayLoadOrStore() && hasRVVRegOperand(MRI, *MI);
}

static void removeIndependentRVVMemoryOrderDeps(ScheduleDAGInstrs &DAG) {
  for (SUnit &SU : DAG.SUnits) {
    if (!isRVVMemorySU(DAG.MRI, &SU))
      continue;

    SmallVector<SDep, 8> ToRemove;
    for (const SDep &PredDep : SU.Preds) {
      if (PredDep.getKind() != SDep::Order)
        continue;
      if (isRVVMemorySU(DAG.MRI, PredDep.getSUnit()))
        ToRemove.push_back(PredDep);
    }

    for (const SDep &PredDep : ToRemove)
      SU.removePred(PredDep);
  }
}

static void addRVVPressureWindowDeps(ScheduleDAGInstrs &DAG,
                                     bool PreferReduction) {
  struct SliceClose {
    SUnit *Start = nullptr;
    SUnit *Close = nullptr;
    unsigned Weight = 0;
  };

  constexpr unsigned MaxOpenRVVLoadWeight = 8;
  SmallVector<SliceClose, 32> SliceCloses;

  for (SUnit &SU : DAG.SUnits) {
    if (!isRVVLoadSU(DAG.MRI, SU))
      continue;
    if (SUnit *Close =
            findRVVSliceClose(SU, DAG.MRI, *DAG.TII, PreferReduction))
      SliceCloses.push_back({&SU, Close, getRVVLoadDefWeight(DAG.MRI, SU)});
  }

  llvm::sort(SliceCloses, [](const SliceClose &A, const SliceClose &B) {
    if (A.Close->NodeNum != B.Close->NodeNum)
      return A.Close->NodeNum < B.Close->NodeNum;
    return A.Start->NodeNum < B.Start->NodeNum;
  });

  unsigned Added = 0;
  unsigned SkippedCycle = 0;
  for (unsigned I = 0, E = SliceCloses.size(); I != E; ++I) {
    unsigned WindowWeight = 0;
    unsigned J = I;
    for (; J != E; ++J) {
      WindowWeight += SliceCloses[J].Weight;
      if (WindowWeight > MaxOpenRVVLoadWeight)
        break;
    }
    if (J == E)
      break;

    SUnit *Close = SliceCloses[I].Close;
    SUnit *NextLoad = SliceCloses[J].Start;
    if (Close == NextLoad)
      continue;
    if (reachesSUnit(NextLoad, Close)) {
      ++SkippedCycle;
      continue;
    }
    NextLoad->addPred(SDep(Close, SDep::Artificial));
    ++Added;
  }
  LLVM_DEBUG(dbgs() << "RISCV RVV pressure DAG sched: function="
                    << DAG.MF.getName() << " pressure-window-slices="
                    << SliceCloses.size() << " pressure-window-added=" << Added
                    << " pressure-window-cycle-skips=" << SkippedCycle << "\n");
}

namespace {

class RISCVVRegPressureClusterMutation : public ScheduleDAGMutation {
  std::unique_ptr<ScheduleDAGMutation> Cluster;
  AAResults *AA = nullptr;

public:
  RISCVVRegPressureClusterMutation(std::unique_ptr<ScheduleDAGMutation> Cluster,
                                   AAResults *AA)
      : Cluster(std::move(Cluster)), AA(AA) {}

  void apply(ScheduleDAGInstrs *DAG) override {
    bool CleanRegion = isCleanRVVPressureRegion(*DAG, AA);
    bool ReduceRegion = !CleanRegion && isReduceRVVPressureRegion(*DAG, AA);
    LLVM_DEBUG(dbgs() << "RISCV RVV pressure DAG sched: function="
                      << DAG->MF.getName()
                      << " cluster-clean-region=" << CleanRegion
                      << " cluster-reduce-region=" << ReduceRegion << "\n");
    if (CleanRegion || ReduceRegion) {
      removeIndependentRVVMemoryOrderDeps(*DAG);
      addRVVPressureWindowDeps(*DAG, ReduceRegion);
      return;
    }
    Cluster->apply(DAG);
  }
};

} // end anonymous namespace

static std::unique_ptr<ScheduleDAGMutation>
wrapRVVRegPressureClusterMutation(std::unique_ptr<ScheduleDAGMutation> Cluster,
                                  AAResults *AA) {
  if (!Cluster)
    return nullptr;
  return std::make_unique<RISCVVRegPressureClusterMutation>(std::move(Cluster),
                                                            AA);
}

std::unique_ptr<ScheduleDAGMutation>
llvm::createRISCVVRegPressureLoadClusterDAGMutation(
    const TargetInstrInfo *TII, const TargetRegisterInfo *TRI,
    bool ReorderWhileClustering, AAResults *AA) {
  return wrapRVVRegPressureClusterMutation(
      createLoadClusterDAGMutation(TII, TRI, ReorderWhileClustering), AA);
}

std::unique_ptr<ScheduleDAGMutation>
llvm::createRISCVVRegPressureStoreClusterDAGMutation(
    const TargetInstrInfo *TII, const TargetRegisterInfo *TRI,
    bool ReorderWhileClustering, AAResults *AA) {
  return wrapRVVRegPressureClusterMutation(
      createStoreClusterDAGMutation(TII, TRI, ReorderWhileClustering), AA);
}

RISCV::VSETVLIInfo
RISCVPreRAMachineSchedStrategy::getVSETVLIInfo(const MachineInstr *MI) const {
  unsigned TSFlags = MI->getDesc().TSFlags;
  if (!RISCVII::hasSEWOp(TSFlags))
    return RISCV::VSETVLIInfo();
  return VIA.computeInfoForInstr(*MI);
}

bool RISCVPreRAMachineSchedStrategy::isRVVReg(Register Reg) const {
  return ::isRVVReg(DAG->MRI, Reg);
}

bool RISCVPreRAMachineSchedStrategy::hasRVVRegDef(
    const MachineInstr &MI) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isDef() && isRVVReg(MO.getReg());
  });
}

bool RISCVPreRAMachineSchedStrategy::hasRVVRegUse(
    const MachineInstr &MI) const {
  return any_of(MI.operands(), [&](const MachineOperand &MO) {
    return MO.isReg() && MO.isUse() && isRVVReg(MO.getReg());
  });
}

bool RISCVPreRAMachineSchedStrategy::isRVVLoad(const MachineInstr &MI) const {
  return MI.mayLoad() && !MI.mayStore() && hasRVVRegDef(MI);
}

bool RISCVPreRAMachineSchedStrategy::isRVVConsumerOrStore(
    const MachineInstr &MI) const {
  if (isRVVLoad(MI))
    return false;
  return hasRVVRegUse(MI) && (hasRVVRegDef(MI) || MI.mayStore());
}

void RISCVPreRAMachineSchedStrategy::initPolicy(
    MachineBasicBlock::iterator Begin, MachineBasicBlock::iterator End,
    unsigned NumRegionInstrs) {
  GenericScheduler::initPolicy(Begin, End, NumRegionInstrs);
  RVVPressureAwareRegion = false;
  bool CleanRegion = false;
  bool ReduceRegion = false;
  if (EnableRVVPressureDAGSched && Begin != End) {
    MachineFunction *MF = Begin->getMF();
    CleanRegion = ::isCleanRVVPressureRegion(MF->getRegInfo(),
                                             *MF->getSubtarget().getInstrInfo(),
                                             Begin, End, Context->AA);
    ReduceRegion = !CleanRegion &&
                   ::isReduceRVVPressureRegion(
                       MF->getRegInfo(), *MF->getSubtarget().getInstrInfo(),
                       Begin, End, Context->AA);
    RVVPressureAwareRegion = CleanRegion || ReduceRegion;
  }
  if (EnableRVVPressureDAGSched)
    RegionPolicy.ShouldTrackPressure = true;
  if (RVVPressureAwareRegion) {
    RegionPolicy.OnlyTopDown = true;
    RegionPolicy.OnlyBottomUp = false;
  }
  LLVM_DEBUG({
    dbgs() << "RISCV RVV pressure DAG sched:";
    if (Begin != End)
      dbgs() << " function=" << Begin->getMF()->getName();
    dbgs() << " pressure-region=" << RVVPressureAwareRegion
           << " clean-region=" << CleanRegion
           << " reduce-region=" << ReduceRegion
           << " track-pressure=" << RegionPolicy.ShouldTrackPressure
           << " top-down=" << RegionPolicy.OnlyTopDown << "\n";
  });
}

void RISCVPreRAMachineSchedStrategy::initialize(ScheduleDAGMI *DAG) {
  GenericScheduler::initialize(DAG);
}

bool RISCVPreRAMachineSchedStrategy::tryVSETVLIInfo(
    const RISCV::VSETVLIInfo &TryInfo, const RISCV::VSETVLIInfo &CandInfo,
    SchedCandidate &TryCand, SchedCandidate &Cand, CandReason Reason) const {
  // Do not compare the vsetvli info changes between top and bottom
  // boundary.
  if (Cand.AtTop != TryCand.AtTop)
    return false;

  auto IsCompatible = [&](const RISCV::VSETVLIInfo &FirstInfo,
                          const RISCV::VSETVLIInfo &SecondInfo) {
    return FirstInfo.isValid() && SecondInfo.isValid() &&
           FirstInfo.isCompatible(RISCV::DemandedFields::all(), SecondInfo,
                                  Context->LIS);
  };

  // Try Cand first.
  // We prefer the top node as it is straightforward from the perspective of
  // vsetvli dataflow.
  if (Cand.AtTop && IsCompatible(CandInfo, TopInfo))
    return true;

  if (!Cand.AtTop && IsCompatible(CandInfo, BottomInfo))
    return true;

  // Then try TryCand.
  if (TryCand.AtTop && IsCompatible(TryInfo, TopInfo)) {
    TryCand.Reason = Reason;
    return true;
  }

  if (!TryCand.AtTop && IsCompatible(TryInfo, BottomInfo)) {
    TryCand.Reason = Reason;
    return true;
  }

  return false;
}

bool RISCVPreRAMachineSchedStrategy::tryCandidate(SchedCandidate &Cand,
                                                  SchedCandidate &TryCand,
                                                  SchedBoundary *Zone) const {
  //-------------------------------------------------------------------------//
  // Below is copied from `GenericScheduler::tryCandidate`.
  // FIXME: Is there a way to not replicate this?
  //-------------------------------------------------------------------------//
  // Initialize the candidate if needed.
  if (!Cand.isValid()) {
    TryCand.Reason = FirstValid;
    return true;
  }

  if (RVVPressureAwareRegion && Zone && Zone->isTop()) {
    const MachineInstr &TryMI = *TryCand.SU->getInstr();
    const MachineInstr &CandMI = *Cand.SU->getInstr();
    bool TryUsesCurrentVector = isRVVConsumerOrStore(TryMI);
    bool CandUsesCurrentVector = isRVVConsumerOrStore(CandMI);
    bool TryStartsVectorLiveRange = isRVVLoad(TryMI);
    bool CandStartsVectorLiveRange = isRVVLoad(CandMI);
    if (tryGreater(TryUsesCurrentVector && CandStartsVectorLiveRange,
                   CandUsesCurrentVector && TryStartsVectorLiveRange, TryCand,
                   Cand, RegMax))
      return TryCand.Reason != NoCand;
  }

  // Bias PhysReg Defs and copies to their uses and defined respectively.
  if (tryGreater(biasPhysReg(TryCand.SU, TryCand.AtTop),
                 biasPhysReg(Cand.SU, Cand.AtTop), TryCand, Cand, PhysReg))
    return TryCand.Reason != NoCand;

  // Avoid exceeding the target's limit.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.Excess, Cand.RPDelta.Excess, TryCand, Cand,
                  RegExcess, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  // Avoid increasing the max critical pressure in the scheduled region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CriticalMax, Cand.RPDelta.CriticalMax,
                  TryCand, Cand, RegCritical, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  // We only compare a subset of features when comparing nodes between
  // Top and Bottom boundary. Some properties are simply incomparable, in many
  // other instances we should only override the other boundary if something
  // is a clear good pick on one boundary. Skip heuristics that are more
  // "tie-breaking" in nature.
  bool SameBoundary = Zone != nullptr;
  if (SameBoundary) {
    // For loops that are acyclic path limited, aggressively schedule for
    // latency. Within an single cycle, whenever CurrMOps > 0, allow normal
    // heuristics to take precedence.
    if (Rem.IsAcyclicLatencyLimited && !Zone->getCurrMOps() &&
        tryLatency(TryCand, Cand, *Zone))
      return TryCand.Reason != NoCand;

    // Prioritize instructions that read unbuffered resources by stall cycles.
    if (tryLess(Zone->getLatencyStallCycles(TryCand.SU),
                Zone->getLatencyStallCycles(Cand.SU), TryCand, Cand, Stall))
      return TryCand.Reason != NoCand;
  }

  // Keep clustered nodes together to encourage downstream peephole
  // optimizations which may reduce resource requirements.
  //
  // This is a best effort to set things up for a post-RA pass. Optimizations
  // like generating loads of multiple registers should ideally be done within
  // the scheduler pass by combining the loads during DAG postprocessing.
  unsigned CandZoneCluster = Cand.AtTop ? TopClusterID : BotClusterID;
  unsigned TryCandZoneCluster = TryCand.AtTop ? TopClusterID : BotClusterID;
  bool CandIsClusterSucc =
      isTheSameCluster(CandZoneCluster, Cand.SU->ParentClusterIdx);
  bool TryCandIsClusterSucc =
      isTheSameCluster(TryCandZoneCluster, TryCand.SU->ParentClusterIdx);

  if (tryGreater(TryCandIsClusterSucc, CandIsClusterSucc, TryCand, Cand,
                 Cluster))
    return TryCand.Reason != NoCand;

  if (SameBoundary) {
    // Weak edges are for clustering and other constraints.
    if (tryLess(getWeakLeft(TryCand.SU, TryCand.AtTop),
                getWeakLeft(Cand.SU, Cand.AtTop), TryCand, Cand, Weak))
      return TryCand.Reason != NoCand;
  }

  // Avoid increasing the max pressure of the entire region.
  if (DAG->isTrackingPressure() &&
      tryPressure(TryCand.RPDelta.CurrentMax, Cand.RPDelta.CurrentMax, TryCand,
                  Cand, RegMax, TRI, DAG->MF))
    return TryCand.Reason != NoCand;

  if (SameBoundary) {
    // Avoid critical resource consumption and balance the schedule.
    TryCand.initResourceDelta(DAG, SchedModel);
    if (tryLess(TryCand.ResDelta.CritResources, Cand.ResDelta.CritResources,
                TryCand, Cand, ResourceReduce))
      return TryCand.Reason != NoCand;
    if (tryGreater(TryCand.ResDelta.DemandedResources,
                   Cand.ResDelta.DemandedResources, TryCand, Cand,
                   ResourceDemand))
      return TryCand.Reason != NoCand;

    // Avoid serializing long latency dependence chains.
    // For acyclic path limited loops, latency was already checked above.
    if (!RegionPolicy.DisableLatencyHeuristic && TryCand.Policy.ReduceLatency &&
        !Rem.IsAcyclicLatencyLimited && tryLatency(TryCand, Cand, *Zone))
      return TryCand.Reason != NoCand;

    // Fall through to original instruction order.
    if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum) ||
        (!Zone->isTop() && TryCand.SU->NodeNum > Cand.SU->NodeNum))
      TryCand.Reason = NodeOrder;
  }

  //-------------------------------------------------------------------------//
  // Below is RISC-V specific scheduling heuristics.
  //-------------------------------------------------------------------------//

  // Add RISC-V specific heuristic only when TryCand isn't selected or
  // selected as node order.
  if (TryCand.Reason != NodeOrder && TryCand.Reason != NoCand)
    return true;

  // TODO: We should not use `CandReason::Cluster` here, but is there a
  // mechanism to extend this enum?
  if (ST->enableVsetvliSchedHeuristic() &&
      tryVSETVLIInfo(getVSETVLIInfo(TryCand.SU->getInstr()),
                     getVSETVLIInfo(Cand.SU->getInstr()), TryCand, Cand,
                     Cluster))
    return TryCand.Reason != NoCand;

  return TryCand.Reason != NoCand;
}

void RISCVPreRAMachineSchedStrategy::enterMBB(MachineBasicBlock *MBB) {
  TopInfo = RISCV::VSETVLIInfo();
  BottomInfo = RISCV::VSETVLIInfo();
}

void RISCVPreRAMachineSchedStrategy::leaveMBB() {
  TopInfo = RISCV::VSETVLIInfo();
  BottomInfo = RISCV::VSETVLIInfo();
}

void RISCVPreRAMachineSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
  GenericScheduler::schedNode(SU, IsTopNode);
  if (ST->enableVsetvliSchedHeuristic()) {
    MachineInstr *MI = SU->getInstr();
    const RISCV::VSETVLIInfo &Info = getVSETVLIInfo(MI);
    if (Info.isValid()) {
      if (IsTopNode)
        TopInfo = Info;
      else
        BottomInfo = Info;
      LLVM_DEBUG({
        dbgs() << "Previous scheduled Unit: \n";
        dbgs() << "  IsTop: " << IsTopNode << "\n";
        dbgs() << "  SU(" << SU->NodeNum << ") - ";
        MI->dump();
        dbgs() << "  \n";
        Info.dump();
        dbgs() << "  \n";
      });
    }
  }
}
