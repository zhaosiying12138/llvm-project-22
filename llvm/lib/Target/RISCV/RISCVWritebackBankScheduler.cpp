//===-- RISCVWritebackBankScheduler.cpp - Banked RF writeback PoC ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass is a RISC-V proof of concept for a target with fixed-latency result
// writeback and a two-bank vector register file. It is intentionally narrower
// than a production scheduler: it models selected post-RA RVV LMUL=1
// instructions, schedules only inside one basic block, drains at unmodelled
// boundaries, and uses a conservative destination-register rename fallback.
//
// The model:
//   * one instruction issues per cycle;
//   * a result writes back at issue_cycle + fixed_latency(opcode);
//   * odd VRs use bank 0, even VRs use bank 1;
//   * each bank has one write port per cycle;
//   * a physical register cannot be redefined while an older delayed write to
//     the same register is still pending.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "riscv-wb-bank-scheduler"
#define RISCV_WB_BANK_SCHEDULER_NAME                                          \
  "RISC-V Writeback Bank Scheduler PoC"

static cl::opt<bool> EnableRISCVWritebackBankScheduler(
    "riscv-wb-bank-scheduler",
    cl::desc("Enable the RISC-V writeback-bank scheduling PoC"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> RISCVWritebackBankVerifyOnly(
    "riscv-wb-bank-verify-only",
    cl::desc("Only verify the RISC-V writeback-bank model"),
    cl::init(false), cl::Hidden);

static cl::opt<bool> RISCVWritebackBankDisableRename(
    "riscv-wb-bank-disable-rename",
    cl::desc("Disable destination-register renaming in the RISC-V writeback-bank "
             "scheduler"),
    cl::init(false), cl::Hidden);

STATISTIC(NumNopsInserted, "Number of writeback-bank nops inserted");
STATISTIC(NumInstsRenamed, "Number of writeback-bank destination renames");
STATISTIC(NumRegionsScheduled, "Number of writeback-bank regions scheduled");

namespace {

struct WritebackEvent {
  unsigned Cycle;
  MCRegister Reg;
  unsigned Bank;
};

class BankedWritebackState {
  const TargetRegisterInfo *TRI;
  SmallVector<WritebackEvent, 8> PendingWrites;

public:
  BankedWritebackState(const TargetRegisterInfo *TRI) : TRI(TRI) {}

  void reset() { PendingWrites.clear(); }

  void retire(unsigned Cycle) {
    PendingWrites.erase(
        std::remove_if(PendingWrites.begin(), PendingWrites.end(),
                       [Cycle](const WritebackEvent &Event) {
                         return Event.Cycle <= Cycle;
                       }),
        PendingWrites.end());
  }

  bool empty() const { return PendingWrites.empty(); }

  bool hasPendingWriteTo(MCRegister Reg) const {
    for (const WritebackEvent &Event : PendingWrites)
      if (TRI->regsOverlap(Event.Reg, Reg))
        return true;
    return false;
  }

  bool hasUnavailableRead(MCRegister Reg, unsigned Cycle) const {
    for (const WritebackEvent &Event : PendingWrites)
      if (Event.Cycle > Cycle && TRI->regsOverlap(Event.Reg, Reg))
        return true;
    return false;
  }

  bool hasBankSlot(unsigned Cycle, unsigned Bank) const {
    for (const WritebackEvent &Event : PendingWrites)
      if (Event.Cycle == Cycle && Event.Bank == Bank)
        return true;
    return false;
  }

  void reserve(unsigned Cycle, MCRegister Reg, unsigned Bank) {
    PendingWrites.push_back({Cycle, Reg, Bank});
  }
};

class RISCVWritebackBankDAG;
class RISCVWritebackBankSchedStrategy;

class RISCVWritebackBankScheduler : public MachineFunctionPass {
public:
  static char ID;

  RISCVWritebackBankScheduler() : MachineFunctionPass(ID) {}

  StringRef getPassName() const override {
    return RISCV_WB_BANK_SCHEDULER_NAME;
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().setNoVRegs();
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  const TargetInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;
  MachineRegisterInfo *MRI = nullptr;
  TargetSchedModel SchedModel;

  bool scheduleBlock(MachineBasicBlock &MBB);
  bool scheduleRegion(MachineBasicBlock &MBB, MachineBasicBlock::iterator Begin,
                      MachineBasicBlock::iterator End);
  void verifyBlock(const MachineBasicBlock &MBB) const;
  void insertNop(MachineBasicBlock &MBB,
                 MachineBasicBlock::iterator InsertPt) const;

  bool isRegionInstruction(const MachineInstr &MI) const;
  std::optional<unsigned> getVRBank(MCRegister Reg) const;
  unsigned getWritebackLatency(const MachineInstr &MI) const;
  void collectReadRegs(const MachineInstr &MI,
                       SmallVectorImpl<MCRegister> &Regs) const;
  void collectWriteRegs(const MachineInstr &MI,
                        SmallVectorImpl<MCRegister> &Regs) const;
  bool canIssue(const MachineInstr &MI, unsigned Cycle,
                const BankedWritebackState &State) const;
  void reserveWrites(const MachineInstr &MI, unsigned Cycle,
                     BankedWritebackState &State) const;
  bool tryRenameForIssue(MachineBasicBlock &MBB, ArrayRef<SUnit> Nodes,
                         SUnit &SU, unsigned Cycle,
                         const BankedWritebackState &State);
  bool isRegMentionedInBlock(const MachineBasicBlock &MBB,
                             MCRegister Reg) const;
  bool isLiveOut(const MachineBasicBlock &MBB, MCRegister Reg) const;

  friend class RISCVWritebackBankDAG;
  friend class RISCVWritebackBankSchedStrategy;
};

class RISCVWritebackBankSchedStrategy : public PostGenericScheduler {
  RISCVWritebackBankScheduler &Pass;
  BankedWritebackState State;
  unsigned Cycle = 0;
  bool NeedNoop = false;

  SUnit *pickWritebackReadyNode();

public:
  RISCVWritebackBankSchedStrategy(const MachineSchedContext *C,
                                  RISCVWritebackBankScheduler &Pass)
      : PostGenericScheduler(C), Pass(Pass), State(Pass.TRI) {}

  void initPolicy(MachineBasicBlock::iterator Begin,
                  MachineBasicBlock::iterator End,
                  unsigned NumRegionInstrs) override;
  void initialize(ScheduleDAGMI *Dag) override;
  SUnit *pickNode(bool &IsTopNode) override;
  void schedNode(SUnit *SU, bool IsTopNode) override;
  bool shouldInsertNoop() const { return NeedNoop; }
  void emitNoop();
};

class RISCVWritebackBankDAG : public ScheduleDAGMI {
  RISCVWritebackBankScheduler &Pass;
  bool Changed = false;

  static std::unique_ptr<MachineSchedStrategy>
  createStrategy(MachineSchedContext *C, RISCVWritebackBankScheduler &Pass) {
    return std::make_unique<RISCVWritebackBankSchedStrategy>(C, Pass);
  }

  RISCVWritebackBankSchedStrategy &getStrategy() {
    return *static_cast<RISCVWritebackBankSchedStrategy *>(SchedImpl.get());
  }

  void clearDependencyLatencies();
  void insertNoopAtTop();

public:
  RISCVWritebackBankDAG(MachineSchedContext *C,
                        RISCVWritebackBankScheduler &Pass)
      : ScheduleDAGMI(C, createStrategy(C, Pass), /*RemoveKillFlags=*/false),
        Pass(Pass) {}

  void schedule() override;
  bool changed() const { return Changed; }
};

} // end anonymous namespace

char RISCVWritebackBankScheduler::ID = 0;

INITIALIZE_PASS(RISCVWritebackBankScheduler, DEBUG_TYPE,
                RISCV_WB_BANK_SCHEDULER_NAME, false, false)

FunctionPass *llvm::createRISCVWritebackBankSchedulerPass() {
  return new RISCVWritebackBankScheduler();
}

std::optional<unsigned>
RISCVWritebackBankScheduler::getVRBank(MCRegister Reg) const {
  if (!RISCV::VRRegClass.contains(Register(Reg)))
    return std::nullopt;
  return (TRI->getEncodingValue(Reg) & 1) ? 0 : 1;
}

unsigned
RISCVWritebackBankScheduler::getWritebackLatency(const MachineInstr &MI) const {
  if (!SchedModel.hasInstrSchedModel())
    return 0;

  bool HasVectorWrite = false;
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isDef())
      continue;
    Register Reg = MO.getReg();
    if (Reg.isPhysical() && getVRBank(Reg.asMCReg())) {
      HasVectorWrite = true;
      break;
    }
  }
  if (!HasVectorWrite)
    return 0;

  const MCSchedClassDesc *SC = SchedModel.resolveSchedClass(&MI);
  if (!SC || !SC->isValid() || !SC->NumWriteLatencyEntries)
    return 0;

  unsigned Latency = SchedModel.computeInstrLatency(&MI);
  return Latency >= 1000 ? 0 : Latency;
}

void RISCVWritebackBankScheduler::collectReadRegs(
    const MachineInstr &MI, SmallVectorImpl<MCRegister> &Regs) const {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.readsReg())
      continue;
    Register Reg = MO.getReg();
    if (!Reg.isPhysical())
      continue;
    if (getVRBank(Reg.asMCReg()))
      Regs.push_back(Reg.asMCReg());
  }
}

void RISCVWritebackBankScheduler::collectWriteRegs(
    const MachineInstr &MI, SmallVectorImpl<MCRegister> &Regs) const {
  for (const MachineOperand &MO : MI.operands()) {
    if (!MO.isReg() || !MO.isDef())
      continue;
    Register Reg = MO.getReg();
    if (!Reg.isPhysical())
      continue;
    if (getVRBank(Reg.asMCReg()))
      Regs.push_back(Reg.asMCReg());
  }
}

bool RISCVWritebackBankScheduler::isRegionInstruction(
    const MachineInstr &MI) const {
  if (MI.isDebugInstr() || MI.isPosition() || MI.isCFIInstruction() ||
      MI.isMetaInstruction())
    return false;

  if (!getWritebackLatency(MI))
    return false;

  if (MI.isCall() || MI.isTerminator() || MI.isBranch() || MI.isReturn() ||
      MI.isInlineAsm() || MI.hasUnmodeledSideEffects() || MI.mayLoad() ||
      MI.mayStore())
    return false;

  return true;
}

bool RISCVWritebackBankScheduler::canIssue(
    const MachineInstr &MI, unsigned Cycle,
    const BankedWritebackState &State) const {
  SmallVector<MCRegister, 4> Reads;
  collectReadRegs(MI, Reads);
  for (MCRegister Reg : Reads)
    if (State.hasUnavailableRead(Reg, Cycle))
      return false;

  unsigned Latency = getWritebackLatency(MI);
  if (!Latency)
    return true;

  SmallVector<MCRegister, 2> Writes;
  collectWriteRegs(MI, Writes);
  SmallVector<unsigned, 2> BanksThisInst;
  for (MCRegister Reg : Writes) {
    std::optional<unsigned> Bank = getVRBank(Reg);
    if (!Bank)
      continue;

    if (State.hasPendingWriteTo(Reg))
      return false;
    if (State.hasBankSlot(Cycle + Latency, *Bank))
      return false;
    if (is_contained(BanksThisInst, *Bank))
      return false;
    BanksThisInst.push_back(*Bank);
  }
  return true;
}

void RISCVWritebackBankScheduler::reserveWrites(
    const MachineInstr &MI, unsigned Cycle, BankedWritebackState &State) const {
  unsigned Latency = getWritebackLatency(MI);
  if (!Latency)
    return;

  SmallVector<MCRegister, 2> Writes;
  collectWriteRegs(MI, Writes);
  for (MCRegister Reg : Writes) {
    std::optional<unsigned> Bank = getVRBank(Reg);
    if (!Bank)
      continue;
    State.reserve(Cycle + Latency, Reg, *Bank);
  }
}

static bool operandMentionsReg(const MachineOperand &MO, MCRegister Reg,
                               const TargetRegisterInfo *TRI) {
  return MO.isReg() && MO.getReg().isPhysical() &&
         TRI->regsOverlap(MO.getReg(), Reg);
}

bool RISCVWritebackBankScheduler::isRegMentionedInBlock(
    const MachineBasicBlock &MBB, MCRegister Reg) const {
  for (const MachineInstr &MI : MBB)
    for (const MachineOperand &MO : MI.operands())
      if (operandMentionsReg(MO, Reg, TRI))
        return true;
  for (const MachineBasicBlock::RegisterMaskPair &LiveIn : MBB.liveins())
    if (TRI->regsOverlap(LiveIn.PhysReg, Reg))
      return true;
  return isLiveOut(MBB, Reg);
}

bool RISCVWritebackBankScheduler::isLiveOut(const MachineBasicBlock &MBB,
                                            MCRegister Reg) const {
  LivePhysRegs Live(*TRI);
  Live.addLiveOutsNoPristines(MBB);
  return Live.contains(Reg);
}

bool RISCVWritebackBankScheduler::tryRenameForIssue(
    MachineBasicBlock &MBB, ArrayRef<SUnit> Nodes, SUnit &SU,
    unsigned Cycle, const BankedWritebackState &State) {
  if (RISCVWritebackBankDisableRename)
    return false;

  MachineInstr &MI = *SU.getInstr();
  unsigned Latency = getWritebackLatency(MI);
  if (!Latency)
    return false;

  int DefOpIdx = -1;
  std::optional<unsigned> TiedUndefUseOpIdx;
  MCRegister OldReg;
  for (unsigned I = 0, E = MI.getNumOperands(); I != E; ++I) {
    MachineOperand &MO = MI.getOperand(I);
    if (!MO.isReg() || !MO.isDef() || MO.isImplicit() || !MO.isRenamable())
      continue;
    Register Reg = MO.getReg();
    if (!Reg.isPhysical() || !getVRBank(Reg.asMCReg()))
      continue;
    unsigned TiedUseOpIdx;
    // RVV pseudos tie rd to an undef passthru operand in tail-agnostic forms.
    if (MI.isRegTiedToUseOperand(I, &TiedUseOpIdx)) {
      MachineOperand &TiedUse = MI.getOperand(TiedUseOpIdx);
      if (!TiedUse.isReg() || !TiedUse.isUndef() || TiedUse.isImplicit() ||
          !TiedUse.isRenamable() || TiedUse.getReg() != Reg)
        continue;
      TiedUndefUseOpIdx = TiedUseOpIdx;
    }
    DefOpIdx = I;
    OldReg = Reg.asMCReg();
    break;
  }
  if (DefOpIdx < 0)
    return false;

  bool HasLaterOldDef = false;
  SmallVector<std::pair<MachineInstr *, unsigned>, 4> UsesToRewrite;
  bool InCurrentValue = false;
  for (SUnit const &Node : Nodes) {
    MachineInstr &ScanMI = *Node.getInstr();
    if (&ScanMI == &MI) {
      InCurrentValue = true;
      continue;
    }
    if (!InCurrentValue)
      continue;

    for (unsigned OpIdx = 0, E = ScanMI.getNumOperands(); OpIdx != E;
         ++OpIdx) {
      MachineOperand &MO = ScanMI.getOperand(OpIdx);
      if (!operandMentionsReg(MO, OldReg, TRI))
        continue;
      if (MO.isDef()) {
        HasLaterOldDef = true;
        InCurrentValue = false;
        break;
      }
      if (MO.readsReg()) {
        if (!MO.isRenamable())
          return false;
        UsesToRewrite.push_back({&ScanMI, OpIdx});
      }
    }
    if (!InCurrentValue)
      break;
  }

  if (!HasLaterOldDef && isLiveOut(MBB, OldReg))
    return false;

  bool SeenDef = false;
  for (MachineBasicBlock::iterator I = MI.getIterator(), E = MBB.end();
       I != E; ++I) {
    if (&*I == &MI) {
      SeenDef = true;
      continue;
    }
    if (!SeenDef)
      continue;

    for (unsigned OpIdx = 0, OpEnd = I->getNumOperands(); OpIdx != OpEnd;
         ++OpIdx) {
      MachineOperand &MO = I->getOperand(OpIdx);
      if (!operandMentionsReg(MO, OldReg, TRI))
        continue;
      if (MO.isDef())
        goto OldValueEnded;
      if (!MO.readsReg())
        continue;
      bool RewrittenInRegion = llvm::any_of(UsesToRewrite, [&](auto Use) {
        return Use.first == &*I && Use.second == OpIdx;
      });
      if (!RewrittenInRegion)
        return false;
    }
  }
OldValueEnded:

  std::optional<unsigned> OldBank = getVRBank(OldReg);
  if (!OldBank)
    return false;

  SmallVector<MCPhysReg, 32> RenameCandidates(RISCV::VRRegClass.begin(),
                                              RISCV::VRRegClass.end());
  llvm::sort(RenameCandidates, [this](MCPhysReg LHS, MCPhysReg RHS) {
    return TRI->getEncodingValue(LHS) < TRI->getEncodingValue(RHS);
  });
  for (MCPhysReg NewReg : RenameCandidates) {
    std::optional<unsigned> NewBank = getVRBank(NewReg);
    if (!NewBank || *NewBank == *OldBank || MRI->isReserved(NewReg))
      continue;
    if (State.hasPendingWriteTo(NewReg))
      continue;
    if (State.hasBankSlot(Cycle + Latency, *NewBank))
      continue;
    if (isRegMentionedInBlock(MBB, NewReg))
      continue;

    LLVM_DEBUG(dbgs() << "Renaming writeback-bank def ";
                      MI.getOperand(DefOpIdx).print(dbgs());
               dbgs() << " -> " << printReg(NewReg, TRI) << " in ";
               MI.print(dbgs()));

    MI.getOperand(DefOpIdx).setReg(NewReg);
    if (TiedUndefUseOpIdx)
      MI.getOperand(*TiedUndefUseOpIdx).setReg(NewReg);
    MI.clearKillInfo();
    for (auto [UseMI, UseOpIdx] : UsesToRewrite) {
      UseMI->getOperand(UseOpIdx).setReg(NewReg);
      UseMI->clearKillInfo();
    }
    ++NumInstsRenamed;
    return true;
  }
  return false;
}

void RISCVWritebackBankScheduler::insertNop(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator InsertPt) const {
  TII->insertNoop(MBB, InsertPt);
  ++NumNopsInserted;
}

void RISCVWritebackBankSchedStrategy::initPolicy(
    MachineBasicBlock::iterator Begin, MachineBasicBlock::iterator End,
    unsigned NumRegionInstrs) {
  PostGenericScheduler::initPolicy(Begin, End, NumRegionInstrs);
  RegionPolicy.OnlyTopDown = true;
  RegionPolicy.OnlyBottomUp = false;
}

void RISCVWritebackBankSchedStrategy::initialize(ScheduleDAGMI *Dag) {
  PostGenericScheduler::initialize(Dag);
  State.reset();
  Cycle = 0;
  NeedNoop = false;
}

SUnit *RISCVWritebackBankSchedStrategy::pickWritebackReadyNode() {
  Top.releasePending();

  SUnit *BestSU = nullptr;
  unsigned BestLatency = 0;
  for (SUnit *SU : Top.Available) {
    if (SU->isScheduled)
      continue;
    MachineInstr &MI = *SU->getInstr();
    if (!Pass.canIssue(MI, Cycle, State))
      continue;
    unsigned Latency = Pass.getWritebackLatency(MI);
    if (!BestSU || Latency > BestLatency ||
        (Latency == BestLatency && SU->NodeNum < BestSU->NodeNum)) {
      BestSU = SU;
      BestLatency = Latency;
    }
  }
  if (BestSU)
    return BestSU;

  for (SUnit *SU : Top.Available) {
    if (SU->isScheduled)
      continue;
    MachineBasicBlock &MBB = *SU->getInstr()->getParent();
    if (Pass.tryRenameForIssue(MBB, DAG->SUnits, *SU, Cycle, State) &&
        Pass.canIssue(*SU->getInstr(), Cycle, State))
      return SU;
  }
  return nullptr;
}

SUnit *RISCVWritebackBankSchedStrategy::pickNode(bool &IsTopNode) {
  NeedNoop = false;
  State.retire(Cycle);

  if (DAG->top() == DAG->bottom()) {
    NeedNoop = !State.empty();
    return nullptr;
  }

  SUnit *SU = pickWritebackReadyNode();
  if (!SU) {
    NeedNoop = true;
    return nullptr;
  }

  IsTopNode = true;
  if (SU->isTopReady())
    Top.removeReady(SU);
  if (SU->isBottomReady())
    Bot.removeReady(SU);
  return SU;
}

void RISCVWritebackBankSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
  assert(IsTopNode && "writeback-bank scheduler is top-down only");
  Pass.reserveWrites(*SU->getInstr(), Cycle, State);
  PostGenericScheduler::schedNode(SU, IsTopNode);
  ++Cycle;
}

void RISCVWritebackBankSchedStrategy::emitNoop() {
  ++Cycle;
  State.retire(Cycle);
  NeedNoop = false;
}

void RISCVWritebackBankDAG::clearDependencyLatencies() {
  // Keep the DAG ordering constraints, but do not let ScheduleDAGMI turn
  // SchedModel latencies into invisible scheduler stalls. This pass consumes
  // the same SchedModel latencies in BankedWritebackState and materializes the
  // required delay cycles as explicit target NOPs.
  for (SUnit &SU : SUnits) {
    for (SDep &Succ : SU.Succs)
      Succ.setLatency(0);
    for (SDep &Pred : SU.Preds)
      Pred.setLatency(0);
  }
}

void RISCVWritebackBankDAG::insertNoopAtTop() {
  Pass.insertNop(*BB, CurrentTop);
  Changed = true;
}

void RISCVWritebackBankDAG::schedule() {
  LLVM_DEBUG(dbgs() << "RISCVWritebackBankDAG::schedule starting\n");
  buildSchedGraph(AA);
  clearDependencyLatencies();
  postProcessDAG();

  SmallVector<SUnit *, 8> TopRoots, BotRoots;
  findRootsAndBiasEdges(TopRoots, BotRoots);
  SchedImpl->initialize(this);
  initQueues(TopRoots, BotRoots);

  while (true) {
    if (!checkSchedLimit())
      break;

    bool IsTopNode = true;
    SUnit *SU = SchedImpl->pickNode(IsTopNode);
    if (!SU) {
      RISCVWritebackBankSchedStrategy &Strategy = getStrategy();
      if (!Strategy.shouldInsertNoop())
        break;
      insertNoopAtTop();
      Strategy.emitNoop();
      continue;
    }

    assert(IsTopNode && "writeback-bank scheduler is top-down only");
    assert(!SU->isScheduled && "Node already scheduled");
    assert(SU->isTopReady() && "node still has unscheduled dependencies");

    MachineInstr *MI = SU->getInstr();
    if (&*CurrentTop == MI)
      CurrentTop = skipDebugInstructionsForward(++CurrentTop, CurrentBottom);
    else {
      moveInstruction(MI, CurrentTop);
      Changed = true;
    }

    SchedImpl->schedNode(SU, IsTopNode);
    updateQueues(SU, IsTopNode);
  }

  assert(CurrentTop == CurrentBottom && "Nonempty unscheduled zone.");
  placeDebugValues();

#ifndef NDEBUG
  VerifyScheduledDAG(/*isBottomUp=*/false);
#endif
}

bool RISCVWritebackBankScheduler::scheduleRegion(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator Begin,
    MachineBasicBlock::iterator End) {
  unsigned RegionInstrs = 0;
  for (MachineBasicBlock::iterator I = Begin; I != End; ++I)
    ++RegionInstrs;
  if (!RegionInstrs)
    return false;

  MachineSchedContext Context;
  Context.MF = MBB.getParent();

  RISCVWritebackBankDAG DAG(&Context, *this);
  DAG.startBlock(&MBB);
  DAG.enterRegion(&MBB, Begin, End, RegionInstrs);
  DAG.schedule();
  DAG.exitRegion();
  bool Changed = DAG.changed();
  DAG.finishBlock();

  ++NumRegionsScheduled;
  return Changed;
}

bool RISCVWritebackBankScheduler::scheduleBlock(MachineBasicBlock &MBB) {
  bool Changed = false;
  MachineBasicBlock::iterator RegionBegin = MBB.end();

  for (MachineBasicBlock::iterator I = MBB.begin(), E = MBB.end(); I != E;) {
    MachineBasicBlock::iterator Next = std::next(I);
    if (isRegionInstruction(*I)) {
      if (RegionBegin == MBB.end())
        RegionBegin = I;
    } else if (RegionBegin != MBB.end()) {
      Changed |= scheduleRegion(MBB, RegionBegin, I);
      RegionBegin = MBB.end();
    }
    I = Next;
  }

  if (RegionBegin != MBB.end())
    Changed |= scheduleRegion(MBB, RegionBegin, MBB.end());

  return Changed;
}

void RISCVWritebackBankScheduler::verifyBlock(
    const MachineBasicBlock &MBB) const {
  BankedWritebackState State(TRI);
  unsigned Cycle = 0;

  for (const MachineInstr &MI : MBB) {
    if (MI.isDebugInstr() || MI.isPosition() || MI.isCFIInstruction() ||
        MI.isMetaInstruction())
      continue;

    State.retire(Cycle);
    if (!canIssue(MI, Cycle, State)) {
      errs() << "RISC-V writeback-bank conflict in " << MBB.getFullName()
             << " at issue cycle " << Cycle << ": ";
      MI.print(errs());
      report_fatal_error("RISC-V writeback-bank verification failed");
    }
    reserveWrites(MI, Cycle, State);
    ++Cycle;
  }
}

bool RISCVWritebackBankScheduler::runOnMachineFunction(MachineFunction &MF) {
  if (skipFunction(MF.getFunction()))
    return false;
  if (!EnableRISCVWritebackBankScheduler && !RISCVWritebackBankVerifyOnly)
    return false;

  TII = MF.getSubtarget().getInstrInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  MRI = &MF.getRegInfo();
  SchedModel.init(&MF.getSubtarget());

  bool Changed = false;
  if (!RISCVWritebackBankVerifyOnly) {
    for (MachineBasicBlock &MBB : MF)
      Changed |= scheduleBlock(MBB);
  }

  for (const MachineBasicBlock &MBB : MF)
    verifyBlock(MBB);

  return Changed;
}
