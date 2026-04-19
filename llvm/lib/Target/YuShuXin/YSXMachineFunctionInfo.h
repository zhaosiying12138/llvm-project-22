//=- YSXMachineFunctionInfo.h - RISC-V machine function info ----*- C++ -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares YSX-specific per-machine-function information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_YSX_YSXMACHINEFUNCTIONINFO_H
#define LLVM_LIB_TARGET_YSX_YSXMACHINEFUNCTIONINFO_H

#include "YSXSubtarget.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"

namespace llvm {

class YSXMachineFunctionInfo;

namespace yaml {
struct YSXMachineFunctionInfo final : public yaml::MachineFunctionInfo {
  int VarArgsFrameIndex;
  int VarArgsSaveSize;

  YSXMachineFunctionInfo() = default;
  YSXMachineFunctionInfo(const llvm::YSXMachineFunctionInfo &MFI);

  void mappingImpl(yaml::IO &YamlIO) override;
  ~YSXMachineFunctionInfo() override = default;
};

template <> struct MappingTraits<YSXMachineFunctionInfo> {
  static void mapping(IO &YamlIO, YSXMachineFunctionInfo &MFI) {
    YamlIO.mapOptional("varArgsFrameIndex", MFI.VarArgsFrameIndex);
    YamlIO.mapOptional("varArgsSaveSize", MFI.VarArgsSaveSize);
  }
};
} // end namespace yaml

/// YSXMachineFunctionInfo - This class is derived from MachineFunctionInfo
/// and contains private YSX-specific information for each MachineFunction.
class YSXMachineFunctionInfo : public MachineFunctionInfo {
private:
  /// FrameIndex for start of varargs area
  int VarArgsFrameIndex = 0;
  /// Size of the save area used for varargs
  int VarArgsSaveSize = 0;
  /// FrameIndex of the spill slot for the scratch register in BranchRelaxation.
  int BranchRelaxationScratchFrameIndex = -1;
  /// Size of any opaque stack adjustment due to save/restore libcalls.
  unsigned LibCallStackSize = 0;
  /// Size of stack frame to save callee saved registers
  unsigned CalleeSavedStackSize = 0;
  /// Registers that have been sign extended from i32.
  SmallVector<Register, 8> SExt32Registers;

  /// Store Frame Indexes for Interrupt-Related CSR Spills.
  SmallVector<int, 2> InterruptCSRFrameIndexes;

  int64_t StackProbeSize = 0;

  /// Does it probe the stack for a dynamic allocation?
  bool HasDynamicAllocation = false;

public:
  YSXMachineFunctionInfo(const Function &F, const YSXSubtarget *STI);

  MachineFunctionInfo *
  clone(BumpPtrAllocator &Allocator, MachineFunction &DestMF,
        const DenseMap<MachineBasicBlock *, MachineBasicBlock *> &Src2DstMBB)
      const override;

  int getVarArgsFrameIndex() const { return VarArgsFrameIndex; }
  void setVarArgsFrameIndex(int Index) { VarArgsFrameIndex = Index; }

  unsigned getVarArgsSaveSize() const { return VarArgsSaveSize; }
  void setVarArgsSaveSize(int Size) { VarArgsSaveSize = Size; }

  int getBranchRelaxationScratchFrameIndex() const {
    return BranchRelaxationScratchFrameIndex;
  }
  void setBranchRelaxationScratchFrameIndex(int Index) {
    BranchRelaxationScratchFrameIndex = Index;
  }

  unsigned getReservedSpillsSize() const {
    return LibCallStackSize;
  }

  unsigned getLibCallStackSize() const { return LibCallStackSize; }
  void setLibCallStackSize(unsigned Size) { LibCallStackSize = Size; }

  bool useSaveRestoreLibCalls(const MachineFunction &MF) const {
    // We cannot use fixed locations for the callee saved spill slots if the
    // function uses a varargs save area, or is an interrupt handler.
    return MF.getSubtarget<YSXSubtarget>().enableSaveRestore() &&
           VarArgsSaveSize == 0 && !MF.getFrameInfo().hasTailCall() &&
           !MF.getFunction().hasFnAttribute("interrupt");
  }

  unsigned getCalleeSavedStackSize() const { return CalleeSavedStackSize; }
  void setCalleeSavedStackSize(unsigned Size) { CalleeSavedStackSize = Size; }

  enum class InterruptStackKind {
    None = 0,
    SiFiveCLICPreemptible,
    SiFiveCLICStackSwap,
    SiFiveCLICPreemptibleStackSwap
  };

  InterruptStackKind getInterruptStackKind(const MachineFunction &MF) const;

  bool useSiFiveInterrupt(const MachineFunction &MF) const {
    InterruptStackKind Kind = getInterruptStackKind(MF);
    return Kind == InterruptStackKind::SiFiveCLICPreemptible ||
           Kind == InterruptStackKind::SiFiveCLICStackSwap ||
           Kind == InterruptStackKind::SiFiveCLICPreemptibleStackSwap;
  }

  bool isSiFivePreemptibleInterrupt(const MachineFunction &MF) const {
    InterruptStackKind Kind = getInterruptStackKind(MF);
    return Kind == InterruptStackKind::SiFiveCLICPreemptible ||
           Kind == InterruptStackKind::SiFiveCLICPreemptibleStackSwap;
  }

  bool isSiFiveStackSwapInterrupt(const MachineFunction &MF) const {
    InterruptStackKind Kind = getInterruptStackKind(MF);
    return Kind == InterruptStackKind::SiFiveCLICStackSwap ||
           Kind == InterruptStackKind::SiFiveCLICPreemptibleStackSwap;
  }

  void pushInterruptCSRFrameIndex(int FI) {
    InterruptCSRFrameIndexes.push_back(FI);
  }
  int getInterruptCSRFrameIndex(size_t Idx) const {
    return InterruptCSRFrameIndexes[Idx];
  }

  // Some Stack Management Variants automatically update FP in a frame-pointer
  // convention compatible way - which means we don't need to manually update
  // the FP, but we still need to emit the correct CFI information for
  // calculating the CFA based on FP.
  bool hasImplicitFPUpdates(const MachineFunction &MF) const;

  void initializeBaseYamlFields(const yaml::YSXMachineFunctionInfo &YamlMFI);

  void addSExt32Register(Register Reg);
  bool isSExt32Register(Register Reg) const;

  bool hasDynamicAllocation() const { return HasDynamicAllocation; }
  void setDynamicAllocation() { HasDynamicAllocation = true; }
};

} // end namespace llvm

#endif // LLVM_LIB_TARGET_YSX_YSXMACHINEFUNCTIONINFO_H
