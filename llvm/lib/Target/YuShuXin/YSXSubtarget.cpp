//===-- YSXSubtarget.cpp - RISC-V Subtarget Information -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the RISC-V specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#include "YSXSubtarget.h"
#include "YSX.h"
#include "YSXFrameLowering.h"
#include "YSXSelectionDAGInfo.h"
#include "YSXTargetMachine.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "ysx-subtarget"

#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "YSXGenSubtargetInfo.inc"

#define GET_YSX_MACRO_FUSION_PRED_IMPL
#include "YSXGenMacroFusion.inc"

namespace llvm::YSXTuneInfoTable {

#define GET_YSXTuneInfoTable_IMPL
#include "YSXGenSearchableTables.inc"
} // namespace llvm::YSXTuneInfoTable

static cl::opt<unsigned> RVVVectorLMULMax(
    "ysx-v-fixed-length-vector-lmul-max",
    cl::desc("The maximum LMUL value to use for fixed length vectors. "
             "Fractional LMUL values are not supported."),
    cl::init(8), cl::Hidden);

static cl::opt<bool> YSXDisableUsingConstantPoolForLargeInts(
    "ysx-disable-using-constant-pool-for-large-ints",
    cl::desc("Disable using constant pool for large integers."),
    cl::init(false), cl::Hidden);

static cl::opt<unsigned> YSXMaxBuildIntsCost(
    "ysx-max-build-ints-cost",
    cl::desc("The maximum cost used for building integers."), cl::init(0),
    cl::Hidden);

static cl::opt<bool> UseAA("ysx-use-aa", cl::init(true),
                           cl::desc("Enable the use of AA during codegen."));

static cl::opt<unsigned> YSXMinimumJumpTableEntries(
    "ysx-min-jump-table-entries", cl::Hidden,
    cl::desc("Set minimum number of entries to use a jump table on YSX"));

static cl::opt<bool> UseMIPSLoadStorePairsOpt(
    "use-ysx-mips-load-store-pairs",
    cl::desc("Enable the load/store pair optimization pass"), cl::init(false),
    cl::Hidden);

static cl::opt<bool> UseMIPSCCMovInsn("use-ysx-mips-ccmov",
                                      cl::desc("Use 'mips.ccmov' instruction"),
                                      cl::init(true), cl::Hidden);

static cl::opt<bool> EnablePExtSIMDCodeGen(
    "ysx-enable-p-ext-simd-codegen",
    cl::desc("Turn on P Extension SIMD codegen(This is a temporary switch "
             "where only partial codegen is currently supported)"),
    cl::init(false), cl::Hidden);

void YSXSubtarget::anchor() {}

YSXSubtarget &
YSXSubtarget::initializeSubtargetDependencies(const Triple &TT, StringRef CPU,
                                                StringRef TuneCPU, StringRef FS,
                                                StringRef ABIName) {
  if (!TT.isYSX64())
    reportFatalUsageError("YSX only supports the ysx64 target");

  if (CPU.empty() || CPU == "generic")
    CPU = "generic-rv64";
  else if (CPU != "generic-rv64")
    reportFatalUsageError("YSX only supports -mcpu=generic-rv64");

  if (TuneCPU.empty() || TuneCPU == "generic")
    TuneCPU = CPU;
  else if (TuneCPU != "generic-rv64")
    reportFatalUsageError("YSX only supports -mtune=generic-rv64");

  if (FS.empty())
    FS = "+m,+a";

  if (!ABIName.empty() && ABIName != "lp64")
    reportFatalUsageError("YSX only supports the lp64 ABI");

  TuneInfo = YSXTuneInfoTable::getYSXTuneInfo(TuneCPU);
  // If there is no TuneInfo for this CPU, we fail back to generic.
  if (!TuneInfo)
    TuneInfo = YSXTuneInfoTable::getYSXTuneInfo("generic");
  assert(TuneInfo && "TuneInfo shouldn't be nullptr!");

  ParseSubtargetFeatures(CPU, TuneCPU, FS);
  TargetABI = YSXABI::computeTargetABI(TT, getFeatureBits(), ABIName);
  YSXFeatures::validate(TT, getFeatureBits());
  return *this;
}

YSXSubtarget::YSXSubtarget(const Triple &TT, StringRef CPU,
                               StringRef TuneCPU, StringRef FS,
                               StringRef ABIName, unsigned RVVVectorBitsMin,
                               unsigned RVVVectorBitsMax,
                               const TargetMachine &TM)
    : YSXGenSubtargetInfo(TT, CPU, TuneCPU, FS),
      IsLittleEndian(TT.isLittleEndian()), RVVVectorBitsMin(RVVVectorBitsMin),
      RVVVectorBitsMax(RVVVectorBitsMax),
      FrameLowering(
          initializeSubtargetDependencies(TT, CPU, TuneCPU, FS, ABIName)),
      InstrInfo(*this), TLInfo(TM, *this) {
  TSInfo = std::make_unique<YSXSelectionDAGInfo>();
}

YSXSubtarget::~YSXSubtarget() = default;

const SelectionDAGTargetInfo *YSXSubtarget::getSelectionDAGInfo() const {
  return TSInfo.get();
}

bool YSXSubtarget::useConstantPoolForLargeInts() const {
  return !YSXDisableUsingConstantPoolForLargeInts;
}

bool YSXSubtarget::enablePExtSIMDCodeGen() const {
  return HasStdExtP && EnablePExtSIMDCodeGen;
}

unsigned YSXSubtarget::getMaxBuildIntsCost() const {
  // Loading integer from constant pool needs two instructions (the reason why
  // the minimum cost is 2): an address calculation instruction and a load
  // instruction. Usually, address calculation and instructions used for
  // building integers (addi, slli, etc.) can be done in one cycle, so here we
  // set the default cost to (LoadLatency + 1) if no threshold is provided.
  return YSXMaxBuildIntsCost == 0
             ? getSchedModel().LoadLatency + 1
             : std::max<unsigned>(2, YSXMaxBuildIntsCost);
}

unsigned YSXSubtarget::getMaxRVVVectorSizeInBits() const {
  assert(hasVInstructions() &&
         "Tried to get vector length without Zve or V extension support!");

  // ZvlLen specifies the minimum required vlen. The upper bound provided by
  // ysx-v-vector-bits-max should be no less than it.
  if (RVVVectorBitsMax != 0 && RVVVectorBitsMax < ZvlLen)
    report_fatal_error("ysx-v-vector-bits-max specified is lower "
                       "than the Zvl*b limitation");

  return RVVVectorBitsMax;
}

unsigned YSXSubtarget::getMinRVVVectorSizeInBits() const {
  assert(hasVInstructions() &&
         "Tried to get vector length without Zve or V extension support!");

  if (RVVVectorBitsMin == -1U)
    return ZvlLen;

  // ZvlLen specifies the minimum required vlen. The lower bound provided by
  // ysx-v-vector-bits-min should be no less than it.
  if (RVVVectorBitsMin != 0 && RVVVectorBitsMin < ZvlLen)
    report_fatal_error("ysx-v-vector-bits-min specified is lower "
                       "than the Zvl*b limitation");

  return RVVVectorBitsMin;
}

unsigned YSXSubtarget::getMaxLMULForFixedLengthVectors() const {
  assert(hasVInstructions() &&
         "Tried to get vector length without Zve or V extension support!");
  assert(RVVVectorLMULMax <= 8 &&
         llvm::has_single_bit<uint32_t>(RVVVectorLMULMax) &&
         "V extension requires a LMUL to be at most 8 and a power of 2!");
  return llvm::bit_floor(std::clamp<unsigned>(RVVVectorLMULMax, 1, 8));
}

bool YSXSubtarget::useRVVForFixedLengthVectors() const {
  return hasVInstructions() &&
         getMinRVVVectorSizeInBits() >= YSX::RVVBitsPerBlock;
}

bool YSXSubtarget::enableSubRegLiveness() const { return true; }

bool YSXSubtarget::enableMachinePipeliner() const {
  return getSchedModel().hasInstrSchedModel();
}

  /// Enable use of alias analysis during code generation (during MI
  /// scheduling, DAGCombine, etc.).
bool YSXSubtarget::useAA() const { return UseAA; }

unsigned YSXSubtarget::getMinimumJumpTableEntries() const {
  return YSXMinimumJumpTableEntries.getNumOccurrences() > 0
             ? YSXMinimumJumpTableEntries
             : TuneInfo->MinimumJumpTableEntries;
}

void YSXSubtarget::overrideSchedPolicy(MachineSchedPolicy &Policy,
                                         const SchedRegion &Region) const {
  // Do bidirectional scheduling since it provides a more balanced scheduling
  // leading to better performance. This will increase compile time.
  Policy.OnlyTopDown = false;
  Policy.OnlyBottomUp = false;

  // Disabling the latency heuristic can reduce the number of spills/reloads but
  // will cause some regressions on some cores.
  Policy.DisableLatencyHeuristic = DisableLatencySchedHeuristic;

  // Spilling is generally expensive on all RISC-V cores, so always enable
  // register-pressure tracking. This will increase compile time.
  Policy.ShouldTrackPressure = true;
}

void YSXSubtarget::overridePostRASchedPolicy(
    MachineSchedPolicy &Policy, const SchedRegion &Region) const {
  MISched::Direction PostRASchedDirection = getPostRASchedDirection();
  if (PostRASchedDirection == MISched::TopDown) {
    Policy.OnlyTopDown = true;
    Policy.OnlyBottomUp = false;
  } else if (PostRASchedDirection == MISched::BottomUp) {
    Policy.OnlyTopDown = false;
    Policy.OnlyBottomUp = true;
  } else if (PostRASchedDirection == MISched::Bidirectional) {
    Policy.OnlyTopDown = false;
    Policy.OnlyBottomUp = false;
  }
}

bool YSXSubtarget::useMIPSLoadStorePairs() const {
  return UseMIPSLoadStorePairsOpt && HasVendorXMIPSLSP;
}

bool YSXSubtarget::useMIPSCCMovInsn() const {
  return UseMIPSCCMovInsn && HasVendorXMIPSCMov;
}
