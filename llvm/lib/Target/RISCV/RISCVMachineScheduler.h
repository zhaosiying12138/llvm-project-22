//===--- RISCVMachineScheduler.h - Custom RISC-V MI scheduler ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Custom RISC-V MI scheduler.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVMACHINESCHEDULER_H
#define LLVM_LIB_TARGET_RISCV_RISCVMACHINESCHEDULER_H

#include "RISCVSubtarget.h"
#include "RISCVVSETVLIInfoAnalysis.h"
#include "llvm/CodeGen/MachineScheduler.h"

namespace llvm {

class AAResults;
class ScheduleDAGMutation;
class TargetInstrInfo;
class TargetRegisterInfo;

/// A GenericScheduler implementation for RISCV pre RA scheduling.
class RISCVPreRAMachineSchedStrategy : public GenericScheduler {
  const RISCVSubtarget *ST;
  RISCV::RISCVVSETVLIInfoAnalysis VIA;
  RISCV::VSETVLIInfo TopInfo;
  RISCV::VSETVLIInfo BottomInfo;
  bool RVVPressureAwareRegion = false;

  RISCV::VSETVLIInfo getVSETVLIInfo(const MachineInstr *MI) const;
  bool isRVVReg(Register Reg) const;
  bool hasRVVRegDef(const MachineInstr &MI) const;
  bool hasRVVRegUse(const MachineInstr &MI) const;
  bool isRVVLoad(const MachineInstr &MI) const;
  bool isRVVConsumerOrStore(const MachineInstr &MI) const;
  bool tryVSETVLIInfo(const RISCV::VSETVLIInfo &TryInfo,
                      const RISCV::VSETVLIInfo &CandInfo,
                      SchedCandidate &TryCand, SchedCandidate &Cand,
                      CandReason Reason) const;

public:
  RISCVPreRAMachineSchedStrategy(const MachineSchedContext *C)
      : GenericScheduler(C), ST(&C->MF->getSubtarget<RISCVSubtarget>()),
        VIA(ST, C->LIS) {}

protected:
  void initPolicy(MachineBasicBlock::iterator Begin,
                  MachineBasicBlock::iterator End,
                  unsigned NumRegionInstrs) override;
  void initialize(ScheduleDAGMI *DAG) override;
  bool tryCandidate(SchedCandidate &Cand, SchedCandidate &TryCand,
                    SchedBoundary *Zone) const override;
  void enterMBB(MachineBasicBlock *MBB) override;
  void leaveMBB() override;
  void schedNode(SUnit *SU, bool IsTopNode) override;
};

bool isRISCVRVVPressureDAGSchedEnabled();
bool isRISCVRVVPressureRematRequested();
std::unique_ptr<ScheduleDAGMutation>
createRISCVVRegPressureLoadClusterDAGMutation(const TargetInstrInfo *TII,
                                              const TargetRegisterInfo *TRI,
                                              bool ReorderWhileClustering,
                                              AAResults *AA);
std::unique_ptr<ScheduleDAGMutation>
createRISCVVRegPressureStoreClusterDAGMutation(const TargetInstrInfo *TII,
                                               const TargetRegisterInfo *TRI,
                                               bool ReorderWhileClustering,
                                               AAResults *AA);

} // end namespace llvm

#endif
