//===-- YSXTargetMachine.cpp - Define TargetMachine for RISC-V ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implements the info about RISC-V target spec.
//
//===----------------------------------------------------------------------===//

#include "YSXTargetMachine.h"
#include "MCTargetDesc/YSXBaseInfo.h"
#include "YSX.h"
#include "YSXMachineFunctionInfo.h"
#include "YSXTargetObjectFile.h"
#include "YSXTargetTransformInfo.h"
#include "TargetInfo/YSXTargetInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/MIRParser/MIParser.h"
#include "llvm/CodeGen/MIRYamlMapping.h"
#include "llvm/CodeGen/GlobalISel/CSEInfo.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/MacroFusion.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include <optional>
using namespace llvm;

static cl::opt<bool> EnableRedundantCopyElimination(
    "ysx-enable-copyelim",
    cl::desc("Enable the redundant copy elimination pass"), cl::init(true),
    cl::Hidden);

// FIXME: Unify control over GlobalMerge.
static cl::opt<cl::boolOrDefault>
    EnableGlobalMerge("ysx-enable-global-merge", cl::Hidden,
                      cl::desc("Enable the global merge pass"));

static cl::opt<bool>
    EnableMachineCombiner("ysx-enable-machine-combiner",
                          cl::desc("Enable the machine combiner pass"),
                          cl::init(true), cl::Hidden);

static cl::opt<unsigned> RVVVectorBitsMaxOpt(
    "ysx-v-vector-bits-max",
    cl::desc("Assume V extension vector registers are at most this big, "
             "with zero meaning no maximum size is assumed."),
    cl::init(0), cl::Hidden);

static cl::opt<int> RVVVectorBitsMinOpt(
    "ysx-v-vector-bits-min",
    cl::desc("Assume V extension vector registers are at least this big, "
             "with zero meaning no minimum size is assumed. A value of -1 "
             "means use Zvl*b extension. This is primarily used to enable "
             "autovectorization with fixed width vectors."),
    cl::init(-1), cl::Hidden);

static cl::opt<bool> EnableYSXCopyPropagation(
    "ysx-enable-copy-propagation",
    cl::desc("Enable the copy propagation with RISC-V copy instr"),
    cl::init(true), cl::Hidden);

static cl::opt<bool> EnableYSXDeadRegisterElimination(
    "ysx-enable-dead-defs", cl::Hidden,
    cl::desc("Enable the pass that removes dead"
             " definitions and replaces stores to"
             " them with stores to x0"),
    cl::init(true));

static cl::opt<bool>
    EnableSinkFold("ysx-enable-sink-fold",
                   cl::desc("Enable sinking and folding of instruction copies"),
                   cl::init(true), cl::Hidden);

static cl::opt<bool>
    EnableLoopDataPrefetch("ysx-enable-loop-data-prefetch", cl::Hidden,
                           cl::desc("Enable the loop data prefetch pass"),
                           cl::init(true));

static cl::opt<bool> DisableVectorMaskMutation(
    "ysx-disable-vector-mask-mutation",
    cl::desc("Disable the vector mask scheduling mutation"), cl::init(false),
    cl::Hidden);

static cl::opt<bool>
    EnableMachinePipeliner("ysx-enable-pipeliner",
                           cl::desc("Enable Machine Pipeliner for RISC-V"),
                           cl::init(false), cl::Hidden);

static cl::opt<bool> EnableCFIInstrInserter(
    "ysx-enable-cfi-instr-inserter",
    cl::desc("Enable CFI Instruction Inserter for RISC-V"), cl::init(false),
    cl::Hidden);

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void LLVMInitializeYSXTarget() {
  RegisterTargetMachine<YSXTargetMachine> Y(getTheYSX64Target());
  auto *PR = PassRegistry::getPassRegistry();
  initializeKCFIPass(*PR);
  initializeYSXDeadRegisterDefinitionsPass(*PR);
  initializeYSXLateBranchOptPass(*PR);
  initializeYSXCodeGenPrepareLegacyPassPass(*PR);
  initializeYSXPostRAExpandPseudoPass(*PR);
  initializeYSXMergeBaseOffsetOptPass(*PR);
  initializeYSXOptWInstrsPass(*PR);
  initializeYSXFoldMemOffsetPass(*PR);
  initializeYSXPreRAExpandPseudoPass(*PR);
  initializeYSXExpandPseudoPass(*PR);
  initializeYSXInsertReadWriteCSRPass(*PR);
  initializeYSXDAGToDAGISelLegacyPass(*PR);
  initializeYSXMoveMergePass(*PR);
  initializeYSXPushPopOptPass(*PR);
  initializeYSXLoadStoreOptPass(*PR);
  initializeYSXExpandAtomicPseudoPass(*PR);
  initializeYSXRedundantCopyEliminationPass(*PR);
  initializeYSXAsmPrinterPass(*PR);
  initializeYSXPromoteConstantPass(*PR);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYuShuXinTarget() {
  LLVMInitializeYSXTarget();
}

static Reloc::Model getEffectiveRelocModel(std::optional<Reloc::Model> RM) {
  return RM.value_or(Reloc::Static);
}

static std::unique_ptr<TargetLoweringObjectFile> createTLOF(const Triple &TT) {
  if (TT.isOSBinFormatMachO())
    return std::make_unique<YSXMachOTargetObjectFile>();
  return std::make_unique<YSXELFTargetObjectFile>();
}

YSXTargetMachine::YSXTargetMachine(const Target &T, const Triple &TT,
                                       StringRef CPU, StringRef FS,
                                       const TargetOptions &Options,
                                       std::optional<Reloc::Model> RM,
                                       std::optional<CodeModel::Model> CM,
                                       CodeGenOptLevel OL, bool JIT)
    : CodeGenTargetMachineImpl(
          T, TT.computeDataLayout(Options.MCOptions.getABIName()), TT, CPU, FS,
          Options, getEffectiveRelocModel(RM),
          getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(createTLOF(TT)) {
  initAsmInfo();

  // RISC-V supports the MachineOutliner.
  setMachineOutliner(true);
  setSupportsDefaultOutlining(true);

  // RISC-V supports the debug entry values.
  setSupportsDebugEntryValues(true);

  if (TT.isOSFuchsia() && !TT.isArch64Bit())
    report_fatal_error("Fuchsia is only supported for 64-bit");

  setCFIFixup(!EnableCFIInstrInserter);
}

const YSXSubtarget *
YSXTargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute TuneAttr = F.getFnAttribute("tune-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  std::string CPU =
      CPUAttr.isValid() ? CPUAttr.getValueAsString().str() : TargetCPU;
  std::string TuneCPU =
      TuneAttr.isValid() ? TuneAttr.getValueAsString().str() : CPU;
  std::string FS =
      FSAttr.isValid() ? FSAttr.getValueAsString().str() : TargetFS;

  unsigned RVVBitsMin = RVVVectorBitsMinOpt;
  unsigned RVVBitsMax = RVVVectorBitsMaxOpt;

  if (TargetTriple.isYSX64()) {
    RVVBitsMin = 0;
    RVVBitsMax = 0;
  } else {
    Attribute VScaleRangeAttr = F.getFnAttribute(Attribute::VScaleRange);
    if (VScaleRangeAttr.isValid()) {
      if (!RVVVectorBitsMinOpt.getNumOccurrences())
        RVVBitsMin = VScaleRangeAttr.getVScaleRangeMin() * YSX::RVVBitsPerBlock;
      std::optional<unsigned> VScaleMax = VScaleRangeAttr.getVScaleRangeMax();
      if (VScaleMax.has_value() && !RVVVectorBitsMaxOpt.getNumOccurrences())
        RVVBitsMax = *VScaleMax * YSX::RVVBitsPerBlock;
    }
  }

  if (RVVBitsMin != -1U) {
    // FIXME: Change to >= 32 when VLEN = 32 is supported.
    assert((RVVBitsMin == 0 || (RVVBitsMin >= 64 && RVVBitsMin <= 65536 &&
                                isPowerOf2_32(RVVBitsMin))) &&
           "V or Zve* extension requires vector length to be in the range of "
           "64 to 65536 and a power 2!");
    assert((RVVBitsMax >= RVVBitsMin || RVVBitsMax == 0) &&
           "Minimum V extension vector length should not be larger than its "
           "maximum!");
  }
  assert((RVVBitsMax == 0 || (RVVBitsMax >= 64 && RVVBitsMax <= 65536 &&
                              isPowerOf2_32(RVVBitsMax))) &&
         "V or Zve* extension requires vector length to be in the range of "
         "64 to 65536 and a power 2!");

  if (RVVBitsMin != -1U) {
    if (RVVBitsMax != 0) {
      RVVBitsMin = std::min(RVVBitsMin, RVVBitsMax);
      RVVBitsMax = std::max(RVVBitsMin, RVVBitsMax);
    }

    RVVBitsMin = llvm::bit_floor(
        (RVVBitsMin < 64 || RVVBitsMin > 65536) ? 0 : RVVBitsMin);
  }
  RVVBitsMax =
      llvm::bit_floor((RVVBitsMax < 64 || RVVBitsMax > 65536) ? 0 : RVVBitsMax);

  SmallString<512> Key;
  raw_svector_ostream(Key) << "RVVMin" << RVVBitsMin << "RVVMax" << RVVBitsMax
                           << CPU << TuneCPU << FS;
  auto &I = SubtargetMap[Key];
  if (!I) {
    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    resetTargetOptions(F);
    auto ABIName = Options.MCOptions.getABIName();
    if (const MDString *ModuleTargetABI = dyn_cast_or_null<MDString>(
            F.getParent()->getModuleFlag("target-abi"))) {
      auto TargetABI = YSXABI::getTargetABI(ABIName);
      if (TargetABI != YSXABI::ABI_Unknown &&
          ModuleTargetABI->getString() != ABIName) {
        report_fatal_error("-target-abi option != target-abi module flag");
      }
      ABIName = ModuleTargetABI->getString();
    }
    I = std::make_unique<YSXSubtarget>(
        TargetTriple, CPU, TuneCPU, FS, ABIName, RVVBitsMin, RVVBitsMax, *this);
  }
  return I.get();
}

MachineFunctionInfo *YSXTargetMachine::createMachineFunctionInfo(
    BumpPtrAllocator &Allocator, const Function &F,
    const TargetSubtargetInfo *STI) const {
  return YSXMachineFunctionInfo::create<YSXMachineFunctionInfo>(
      Allocator, F, static_cast<const YSXSubtarget *>(STI));
}

TargetTransformInfo
YSXTargetMachine::getTargetTransformInfo(const Function &F) const {
  return TargetTransformInfo(std::make_unique<YSXTTIImpl>(this, F));
}

// A RISC-V hart has a single byte-addressable address space of 2^XLEN bytes
// for all memory accesses, so it is reasonable to assume that an
// implementation has no-op address space casts. If an implementation makes a
// change to this, they can override it here.
bool YSXTargetMachine::isNoopAddrSpaceCast(unsigned SrcAS,
                                             unsigned DstAS) const {
  return true;
}

ScheduleDAGInstrs *
YSXTargetMachine::createMachineScheduler(MachineSchedContext *C) const {
  const YSXSubtarget &ST = C->MF->getSubtarget<YSXSubtarget>();
  ScheduleDAGMILive *DAG = createSchedLive<GenericScheduler>(C);

  if (ST.enableMISchedLoadClustering())
    DAG->addMutation(createLoadClusterDAGMutation(
        DAG->TII, DAG->TRI, /*ReorderWhileClustering=*/true));

  if (ST.enableMISchedStoreClustering())
    DAG->addMutation(createStoreClusterDAGMutation(
        DAG->TII, DAG->TRI, /*ReorderWhileClustering=*/true));

  return DAG;
}

ScheduleDAGInstrs *
YSXTargetMachine::createPostMachineScheduler(MachineSchedContext *C) const {
  const YSXSubtarget &ST = C->MF->getSubtarget<YSXSubtarget>();
  ScheduleDAGMI *DAG = createSchedPostRA(C);

  if (ST.enablePostMISchedLoadClustering())
    DAG->addMutation(createLoadClusterDAGMutation(
        DAG->TII, DAG->TRI, /*ReorderWhileClustering=*/true));

  if (ST.enablePostMISchedStoreClustering())
    DAG->addMutation(createStoreClusterDAGMutation(
        DAG->TII, DAG->TRI, /*ReorderWhileClustering=*/true));

  return DAG;
}

namespace {

class RVVRegisterRegAlloc : public RegisterRegAllocBase<RVVRegisterRegAlloc> {
public:
  RVVRegisterRegAlloc(const char *N, const char *D, FunctionPassCtor C)
      : RegisterRegAllocBase(N, D, C) {}
};

static bool onlyAllocateRVVReg(const TargetRegisterInfo &TRI,
                               const MachineRegisterInfo &MRI,
                               const Register Reg) {
  const TargetRegisterClass *RC = MRI.getRegClass(Reg);
  return YSXRegisterInfo::isRVVRegClass(RC);
}

static FunctionPass *useDefaultRegisterAllocator() { return nullptr; }

static llvm::once_flag InitializeDefaultRVVRegisterAllocatorFlag;

/// -ysx-rvv-regalloc=<fast|basic|greedy> command line option.
/// This option could designate the rvv register allocator only.
/// For example: -ysx-rvv-regalloc=basic
static cl::opt<RVVRegisterRegAlloc::FunctionPassCtor, false,
               RegisterPassParser<RVVRegisterRegAlloc>>
    RVVRegAlloc("ysx-rvv-regalloc", cl::Hidden,
                cl::init(&useDefaultRegisterAllocator),
                cl::desc("Register allocator to use for RVV register."));

static void initializeDefaultRVVRegisterAllocatorOnce() {
  RegisterRegAlloc::FunctionPassCtor Ctor = RVVRegisterRegAlloc::getDefault();

  if (!Ctor) {
    Ctor = RVVRegAlloc;
    RVVRegisterRegAlloc::setDefault(RVVRegAlloc);
  }
}

static FunctionPass *createBasicRVVRegisterAllocator() {
  return createBasicRegisterAllocator(onlyAllocateRVVReg);
}

static FunctionPass *createGreedyRVVRegisterAllocator() {
  return createGreedyRegisterAllocator(onlyAllocateRVVReg);
}

static FunctionPass *createFastRVVRegisterAllocator() {
  return createFastRegisterAllocator(onlyAllocateRVVReg, false);
}

static RVVRegisterRegAlloc basicRegAllocRVVReg("basic",
                                               "basic register allocator",
                                               createBasicRVVRegisterAllocator);
static RVVRegisterRegAlloc
    greedyRegAllocRVVReg("greedy", "greedy register allocator",
                         createGreedyRVVRegisterAllocator);

static RVVRegisterRegAlloc fastRegAllocRVVReg("fast", "fast register allocator",
                                              createFastRVVRegisterAllocator);

class YSXPassConfig : public TargetPassConfig {
public:
  YSXPassConfig(YSXTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {
    if (TM.getOptLevel() != CodeGenOptLevel::None)
      substitutePass(&PostRASchedulerID, &PostMachineSchedulerID);
    setEnableSinkAndFold(EnableSinkFold);
    EnableLoopTermFold = true;
  }

  YSXTargetMachine &getYSXTargetMachine() const {
    return getTM<YSXTargetMachine>();
  }

  void addIRPasses() override;
  bool addPreISel() override;
  void addCodeGenPrepare() override;
  bool addInstSelector() override;
  void addPreEmitPass() override;
  void addPreEmitPass2() override;
  void addPreSched2() override;
  void addMachineSSAOptimization() override;
  FunctionPass *createRVVRegAllocPass(bool Optimized);
  bool addRegAssignAndRewriteFast() override;
  bool addRegAssignAndRewriteOptimized() override;
  void addPreRegAlloc() override;
  void addPostRegAlloc() override;
  void addFastRegAlloc() override;
  bool addILPOpts() override;

  std::unique_ptr<CSEConfigBase> getCSEConfig() const override;
};
} // namespace

TargetPassConfig *YSXTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new YSXPassConfig(*this, PM);
}

std::unique_ptr<CSEConfigBase> YSXPassConfig::getCSEConfig() const {
  return getStandardCSEConfigForOpt(TM->getOptLevel());
}

FunctionPass *YSXPassConfig::createRVVRegAllocPass(bool Optimized) {
  // Initialize the global default.
  llvm::call_once(InitializeDefaultRVVRegisterAllocatorFlag,
                  initializeDefaultRVVRegisterAllocatorOnce);

  RegisterRegAlloc::FunctionPassCtor Ctor = RVVRegisterRegAlloc::getDefault();
  if (Ctor != useDefaultRegisterAllocator)
    return Ctor();

  if (Optimized)
    return createGreedyRVVRegisterAllocator();

  return createFastRVVRegisterAllocator();
}

bool YSXPassConfig::addRegAssignAndRewriteFast() {
  bool Added = TargetPassConfig::addRegAssignAndRewriteFast();
  if (TM->getOptLevel() != CodeGenOptLevel::None &&
      EnableYSXDeadRegisterElimination)
    addPass(createYSXDeadRegisterDefinitionsPass());
  return Added;
}

bool YSXPassConfig::addRegAssignAndRewriteOptimized() {
  bool Added = TargetPassConfig::addRegAssignAndRewriteOptimized();
  if (TM->getOptLevel() != CodeGenOptLevel::None &&
      EnableYSXDeadRegisterElimination)
    addPass(createYSXDeadRegisterDefinitionsPass());
  return Added;
}

void YSXPassConfig::addIRPasses() {
  addPass(createAtomicExpandLegacyPass());

  if (getOptLevel() != CodeGenOptLevel::None) {
    if (EnableLoopDataPrefetch)
      addPass(createLoopDataPrefetchPass());

    addPass(createYSXCodeGenPrepareLegacyPass());
  }

  TargetPassConfig::addIRPasses();
}

bool YSXPassConfig::addPreISel() {
  if (TM->getOptLevel() != CodeGenOptLevel::None)
    addPass(createYSXPromoteConstantPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    // Add a barrier before instruction selection so that we will not get
    // deleted block address after enabling default outlining. See D99707 for
    // more details.
    addPass(createBarrierNoopPass());
  }

  if ((TM->getOptLevel() != CodeGenOptLevel::None &&
       EnableGlobalMerge == cl::BOU_UNSET) ||
      EnableGlobalMerge == cl::BOU_TRUE) {
    // FIXME: Like AArch64, we disable extern global merging by default due to
    // concerns it might regress some workloads. Unlike AArch64, we don't
    // currently support enabling the pass in an "OnlyOptimizeForSize" mode.
    // Investigating and addressing both items are TODO.
    addPass(createGlobalMergePass(TM, /* MaxOffset */ 2047,
                                  /* OnlyOptimizeForSize */ false,
                                  /* MergeExternalByDefault */ true));
  }

  return false;
}

void YSXPassConfig::addCodeGenPrepare() {
  if (getOptLevel() != CodeGenOptLevel::None)
    addPass(createTypePromotionLegacyPass());
  TargetPassConfig::addCodeGenPrepare();
}

bool YSXPassConfig::addInstSelector() {
  addPass(createYSXISelDag(getYSXTargetMachine(), getOptLevel()));

  return false;
}

void YSXPassConfig::addPreSched2() {
  addPass(createYSXPostRAExpandPseudoPass());

  // Emit KCFI checks for indirect calls.
  addPass(createKCFIPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None)
    addPass(createYSXLoadStoreOptPass());
}

void YSXPassConfig::addPreEmitPass() {
  // TODO: It would potentially be better to schedule copy propagation after
  // expanding pseudos (in addPreEmitPass2). However, performing copy
  // propagation after the machine outliner (which runs after addPreEmitPass)
  // currently leads to incorrect code-gen, where copies to registers within
  // outlined functions are removed erroneously.
  if (TM->getOptLevel() >= CodeGenOptLevel::Default &&
      EnableYSXCopyPropagation)
    addPass(createMachineCopyPropagationPass(true));
  if (TM->getOptLevel() >= CodeGenOptLevel::Default)
    addPass(createYSXLateBranchOptPass());
  addPass(&BranchRelaxationPassID);
}

void YSXPassConfig::addPreEmitPass2() {
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    addPass(createYSXMoveMergePass());
    // Schedule PushPop Optimization before expansion of Pseudo instruction,
    // ensuring return instruction is detected correctly.
    addPass(createYSXPushPopOptimizationPass());
  }
  addPass(createYSXExpandPseudoPass());

  // Schedule the expansion of AMOs at the last possible moment, avoiding the
  // possibility for other passes to break the requirements for forward
  // progress in the LR/SC block.
  addPass(createYSXExpandAtomicPseudoPass());

  // KCFI indirect call checks are lowered to a bundle.
  addPass(createUnpackMachineBundles([&](const MachineFunction &MF) {
    return MF.getFunction().getParent()->getModuleFlag("kcfi");
  }));

  if (EnableCFIInstrInserter)
    addPass(createCFIInstrInserter());
}

void YSXPassConfig::addMachineSSAOptimization() {
  addPass(createYSXFoldMemOffsetPass());

  TargetPassConfig::addMachineSSAOptimization();

  if (TM->getTargetTriple().isYSX64()) {
    addPass(createYSXOptWInstrsPass());
  }
}

void YSXPassConfig::addPreRegAlloc() {
  addPass(createYSXPreRAExpandPseudoPass());
  if (TM->getOptLevel() != CodeGenOptLevel::None) {
    addPass(createYSXMergeBaseOffsetOptPass());
  }

  addPass(createYSXInsertReadWriteCSRPass());
  addPass(createYSXLandingPadSetupPass());

  if (TM->getOptLevel() != CodeGenOptLevel::None && EnableMachinePipeliner)
    addPass(&MachinePipelinerID);
}

void YSXPassConfig::addFastRegAlloc() {
  addPass(&InitUndefID);
  TargetPassConfig::addFastRegAlloc();
}


void YSXPassConfig::addPostRegAlloc() {
  if (TM->getOptLevel() != CodeGenOptLevel::None &&
      EnableRedundantCopyElimination)
    addPass(createYSXRedundantCopyEliminationPass());
}

bool YSXPassConfig::addILPOpts() {
  if (EnableMachineCombiner)
    addPass(&MachineCombinerID);

  return true;
}

void YSXTargetMachine::registerPassBuilderCallbacks(PassBuilder &PB) {
#define GET_PASS_REGISTRY "YSXPassRegistry.def"
#include "llvm/Passes/TargetPassRegistry.inc"
}

yaml::MachineFunctionInfo *
YSXTargetMachine::createDefaultFuncInfoYAML() const {
  return new yaml::YSXMachineFunctionInfo();
}

yaml::MachineFunctionInfo *
YSXTargetMachine::convertFuncInfoToYAML(const MachineFunction &MF) const {
  const auto *MFI = MF.getInfo<YSXMachineFunctionInfo>();
  return new yaml::YSXMachineFunctionInfo(*MFI);
}

bool YSXTargetMachine::parseMachineFunctionInfo(
    const yaml::MachineFunctionInfo &MFI, PerFunctionMIParsingState &PFS,
    SMDiagnostic &Error, SMRange &SourceRange) const {
  const auto &YamlMFI =
      static_cast<const yaml::YSXMachineFunctionInfo &>(MFI);
  PFS.MF.getInfo<YSXMachineFunctionInfo>()->initializeBaseYamlFields(YamlMFI);
  return false;
}
