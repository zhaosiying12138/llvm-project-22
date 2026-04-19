//===-- YSXFeatures.td - YuShuXin Features and Extensions -*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// YuShuXin v1 intentionally exposes a single ISA profile: rv64ima.  This file
// therefore keeps only the architectural features required for I/M/A and the
// small set of LLVM-internal switches still used by generic codegen.
//
//===----------------------------------------------------------------------===//

class YSXExtension<int major, int minor, string desc,
                   list<SubtargetFeature> implies = [],
                   string fieldname = !subst("Feature", "Has", NAME),
                   string value = "true", bit IsExperimental = false>
    : SubtargetFeature<"", fieldname, value, "", implies> {
  defvar extName = !subst("FeatureStdExt", "", NAME);

  let Name = !if(IsExperimental, "experimental-", "") # !tolower(extName);
  let Desc = "'" # extName # "' (" # desc # ")";

  int MajorVersion = major;
  int MinorVersion = minor;
  bit Experimental = IsExperimental;
}

class YSXExtensionBitmask<bits<3> groupID, int bitPos> {
  int GroupID = groupID;
  int BitPos = bitPos;
}

//===----------------------------------------------------------------------===//
// Supported rv64ima ISA surface
//===----------------------------------------------------------------------===//

def FeatureStdExtI
    : YSXExtension<2, 1, "Base Integer Instruction Set">,
      YSXExtensionBitmask<0, 8>;

def FeatureStdExtZmmul
    : YSXExtension<1, 0, "Integer Multiplication">,
      YSXExtensionBitmask<0, 35>;
def HasStdExtZmmul : Predicate<"Subtarget->hasStdExtZmmul()">,
                     AssemblerPredicate<(all_of FeatureStdExtZmmul),
                                        "'Zmmul' (Integer Multiplication)">;

def FeatureStdExtM
    : YSXExtension<2, 0, "Integer Multiplication and Division",
                   [FeatureStdExtZmmul]>,
      YSXExtensionBitmask<0, 12>;
def HasStdExtM : Predicate<"Subtarget->hasStdExtM()">,
                 AssemblerPredicate<(all_of FeatureStdExtM),
                                    "'M' (Integer Multiplication and Division)">;

def FeatureStdExtZaamo
    : YSXExtension<1, 0, "'AMO' instructions">,
      YSXExtensionBitmask<0, 37>;
def HasStdExtZaamo
    : Predicate<"Subtarget->hasStdExtZaamo()">,
      AssemblerPredicate<(any_of FeatureStdExtZaamo),
                         "'Zaamo' (Atomic Memory Operations)">;
def NoStdExtZaamo : Predicate<"!Subtarget->hasStdExtZaamo()">;

def FeatureStdExtZalrsc
    : YSXExtension<1, 0, "'LR/SC' instructions">,
      YSXExtensionBitmask<0, 38>;
def HasStdExtZalrsc
    : Predicate<"Subtarget->hasStdExtZalrsc()">,
      AssemblerPredicate<(any_of FeatureStdExtZalrsc),
                         "'Zalrsc' (Load-Reserved/Store-Conditional)">;

def FeatureStdExtA
    : YSXExtension<2, 1, "Atomic Instructions",
                   [FeatureStdExtZaamo, FeatureStdExtZalrsc]>,
      YSXExtensionBitmask<0, 0>;
def HasStdExtA : Predicate<"Subtarget->hasStdExtA()">,
                 AssemblerPredicate<(all_of FeatureStdExtA),
                                    "'A' (Atomic Instructions)">;

//===----------------------------------------------------------------------===//
// Target width and generic assembler controls
//===----------------------------------------------------------------------===//

def Feature32Bit
    : SubtargetFeature<"32bit", "IsRV32", "true", "Implements RV32">;
def Feature64Bit
    : SubtargetFeature<"64bit", "IsRV64", "true", "Implements RV64">;
def IsRV64 : Predicate<"Subtarget->is64Bit()">,
             AssemblerPredicate<(all_of Feature64Bit),
                                "RV64I Base Instruction Set">;
def IsRV32 : Predicate<"!Subtarget->is64Bit()">,
             AssemblerPredicate<(all_of (not Feature64Bit)),
                                "RV32I Base Instruction Set">;

defvar RV32 = DefaultMode;
def RV64 : HwMode<[IsRV64]>;

def FeatureRelax
    : SubtargetFeature<"relax", "EnableLinkerRelax", "true",
                       "Enable linker relaxation">;

def FeatureExactAssembly
    : SubtargetFeature<"exact-asm", "EnableExactAssembly", "true",
                       "Enable exact assembly">;

foreach i = {1-31} in
  def FeatureReserveX#i :
      SubtargetFeature<"reserve-x"#i, "UserReservedRegister[YSX::X"#i#"]",
                       "true", "Reserve X"#i>;

def FeatureSaveRestore : SubtargetFeature<"save-restore", "EnableSaveRestore",
                                          "true", "Enable save/restore">;

def FeatureNoTrailingSeqCstFence
    : SubtargetFeature<"no-trailing-seq-cst-fence",
                       "EnableTrailingSeqCstFence", "false",
                       "Disable trailing fence for seq-cst store">;

def FeatureUnalignedScalarMem
    : SubtargetFeature<"unaligned-scalar-mem", "EnableUnalignedScalarMem",
                       "true", "Has performant unaligned scalar memory">;

def FeatureForcedAtomics
    : SubtargetFeature<"forced-atomics", "HasForcedAtomics", "true",
                       "Assume lock-free native-width atomics are available">;

def HasAtomicLdSt
    : Predicate<"Subtarget->hasStdExtZalrsc() || Subtarget->hasForcedAtomics()">;

def FeaturePermissiveZalrsc
    : SubtargetFeature<"permissive-zalrsc", "HasPermissiveZalrsc", "true",
                       "Implementation permits non-base instructions between LR/SC pairs">;

def FeatureTaggedGlobals
    : SubtargetFeature<"tagged-globals", "AllowTaggedGlobals", "true",
                       "Allow tagged global-address materialization">;

def Experimental
    : SubtargetFeature<"experimental", "HasExperimental", "true",
                       "Experimental intrinsics">;

//===----------------------------------------------------------------------===//
// Unsupported extension predicates retained only for surviving generic TD code
//===----------------------------------------------------------------------===//

def HasStdExtZicfilp : Predicate<"false">;
def NoStdExtZicfilp : Predicate<"true">;
def HasStdExtZihintpause : Predicate<"false">;
def HasStdExtZihintntl : Predicate<"false">;
def HasStdExtZtso : Predicate<"false">;
def NoStdExtZtso : Predicate<"true">;
def NoStdExtZba : Predicate<"true">;
def NoStdExtZacas : Predicate<"true">;

//===----------------------------------------------------------------------===//
// Tuning controls still consumed by generic passes
//===----------------------------------------------------------------------===//

def TuneNLogNVRGather
    : SubtargetFeature<"log-vrgather", "YSXVRGatherCostModel", "NLog2N",
                       "Use N log N vector gather cost model">;

def TunePostRAScheduler
    : SubtargetFeature<"use-postra-scheduler", "UsePostRAScheduler", "true",
                       "Schedule again after register allocation">;

def TuneDisableMISchedLoadClustering
    : SubtargetFeature<"disable-misched-load-clustering",
                       "EnableMISchedLoadClustering", "false",
                       "Disable load clustering in the machine scheduler">;

def TuneDisableMISchedStoreClustering
    : SubtargetFeature<"disable-misched-store-clustering",
                       "EnableMISchedStoreClustering", "false",
                       "Disable store clustering in the machine scheduler">;

def TuneDisablePostMISchedLoadClustering
    : SubtargetFeature<"disable-postmisched-load-clustering",
                       "EnablePostMISchedLoadClustering", "false",
                       "Disable PostRA load clustering in the machine scheduler">;

def TuneDisablePostMISchedStoreClustering
    : SubtargetFeature<"disable-postmisched-store-clustering",
                       "EnablePostMISchedStoreClustering", "false",
                       "Disable PostRA store clustering in the machine scheduler">;

def TuneDisableLatencySchedHeuristic
    : SubtargetFeature<"disable-latency-sched-heuristic",
                       "DisableLatencySchedHeuristic", "true",
                       "Disable latency scheduling heuristic">;

def TuneEnableVsetvliSchedHeuristic
    : SubtargetFeature<"enable-vsetvli-sched-heuristic",
                       "EnableVsetvliSchedHeuristic", "true",
                       "Enable vsetvli-based scheduling heuristic">;

def TunePredictableSelectIsExpensive
    : SubtargetFeature<"predictable-select-expensive",
                       "PredictableSelectIsExpensive", "true",
                       "Prefer likely predicted branches over selects">;

def TuneOptimizedZeroStrideLoad
    : SubtargetFeature<"optimized-zero-stride-load",
                       "HasOptimizedZeroStrideLoad", "true",
                       "Optimized zero-stride vector load">;

foreach nf = {2-8} in
  def TuneOptimizedNF#nf#SegmentLoadStore :
      SubtargetFeature<"optimized-nf"#nf#"-segment-load-store",
                       "HasOptimizedNF"#nf#"SegmentLoadStore",
                       "true", "Optimized segment load/store">;

def TuneVLDependentLatency
    : SubtargetFeature<"vl-dependent-latency", "HasVLDependentLatency", "true",
                       "Vector instruction latency depends on vl">;

def TuneDLenFactor2
    : SubtargetFeature<"dlen-factor-2", "DLenFactor2", "true",
                       "Vector unit DLEN is half of VLEN">;

def TuneNoDefaultUnroll
    : SubtargetFeature<"no-default-unroll", "EnableDefaultUnroll", "false",
                       "Disable default unroll preference">;

def TuneShortForwardBranchIALU
    : SubtargetFeature<"short-forward-branch-ialu",
                       "HasShortForwardBranchIALU", "true",
                       "Enable short forward branch optimization for IALU">;
def HasShortForwardBranchIALU : Predicate<"Subtarget->hasShortForwardBranchIALU()">;
def NoShortForwardBranch : Predicate<"!Subtarget->hasShortForwardBranchIALU()">;

def TuneShortForwardBranchIMinMax
    : SubtargetFeature<"short-forward-branch-iminmax",
                       "HasShortForwardBranchIMinMax", "true",
                       "Enable short forward branch optimization for min/max",
                       [TuneShortForwardBranchIALU]>;
def HasShortForwardBranchIMinMax : Predicate<"Subtarget->hasShortForwardBranchIMinMax()">;

def TuneShortForwardBranchIMul
    : SubtargetFeature<"short-forward-branch-imul",
                       "HasShortForwardBranchIMul", "true",
                       "Enable short forward branch optimization for mul",
                       [TuneShortForwardBranchIALU]>;
def HasShortForwardBranchIMul : Predicate<"Subtarget->hasShortForwardBranchIMul()">;

def TuneShortForwardBranchILoad
    : SubtargetFeature<"short-forward-branch-iload",
                       "HasShortForwardBranchILoad", "true",
                       "Enable short forward branch optimization for loads",
                       [TuneShortForwardBranchIALU]>;
def HasShortForwardBranchILoad : Predicate<"Subtarget->hasShortForwardBranchILoad()">;

def TuneNoSinkSplatOperands
    : SubtargetFeature<"no-sink-splat-operands", "SinkSplatOperands",
                       "false", "Disable sink splat operands">;

def TunePreferWInst
    : SubtargetFeature<"prefer-w-inst", "PreferWInst", "true",
                       "Prefer W-suffixed instructions">;

def TuneConditionalCompressedMoveFusion
    : SubtargetFeature<"conditional-cmv-fusion",
                       "HasConditionalCompressedMoveFusion", "true",
                       "Enable branch+c.mv fusion">;
def HasConditionalMoveFusion : Predicate<"Subtarget->hasConditionalMoveFusion()">;
def NoConditionalMoveFusion  : Predicate<"!Subtarget->hasConditionalMoveFusion()">;

def TuneHasSingleElementVecFP64
    : SubtargetFeature<"single-element-vec-fp64",
                       "HasSingleElementVectorFP64", "true",
                       "Single-element vector FP64 throughput model">;

def TuneVXRMPipelineFlush
    : SubtargetFeature<"vxrm-pipeline-flush", "HasVXRMPipelineFlush",
                       "true", "VXRM writes cause pipeline flush">;

def TunePreferVsetvliOverReadVLENB
    : SubtargetFeature<"prefer-vsetvli-over-read-vlenb",
                       "PreferVsetvliOverReadVLENB", "true",
                       "Prefer vsetvli over read vlenb">;

def TuneAndes45 : SubtargetFeature<"andes45", "YSXProcFamily", "Andes45",
                                   "Andes 45-Series processors">;
def TuneMIPSP8700 : SubtargetFeature<"mips-p8700", "YSXProcFamily", "MIPSP8700",
                                     "MIPS p8700 processor">;
def TuneSiFive7 : SubtargetFeature<"sifive7", "YSXProcFamily", "SiFive7",
                                   "SiFive 7-Series processors">;
def TuneVentanaVeyron
    : SubtargetFeature<"ventana-veyron", "YSXProcFamily", "VentanaVeyron",
                       "Ventana Veyron-Series processors">;
