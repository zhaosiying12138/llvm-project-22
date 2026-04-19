//===-- YSXMCTargetDesc.cpp - RISC-V Target Descriptions ----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This file provides RISC-V specific target descriptions.
///
//===----------------------------------------------------------------------===//

#include "YSXMCTargetDesc.h"
#include "YSXELFStreamer.h"
#include "YSXInstPrinter.h"
#include "YSXMCAsmInfo.h"
#include "YSXMCObjectFileInfo.h"
#include "YSXTargetStreamer.h"
#include "TargetInfo/YSXTargetInfo.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include <bitset>
#include <vector>

#define GET_INSTRINFO_MC_DESC
#define ENABLE_INSTR_PREDICATE_VERIFIER
#include "YSXGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "YSXGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "YSXGenSubtargetInfo.inc"

using namespace llvm;

static bool isKnownYSXFeature(StringRef Feature) {
  for (const SubtargetFeatureKV &KV :
       ArrayRef(YSXFeatureKV, YSX::NumSubtargetFeatures))
    if (Feature == StringRef(KV.Key))
      return true;
  return false;
}

static bool isRequiredYSXFeature(StringRef Feature) {
  return Feature == "64bit" || Feature == "i" || Feature == "m" ||
         Feature == "a" || Feature == "zmmul" || Feature == "zaamo" ||
         Feature == "zalrsc";
}

static bool isYSXReserveXFeature(StringRef Feature) {
  if (!Feature.consume_front("reserve-x"))
    return false;
  unsigned Reg = 0;
  return !Feature.empty() && !Feature.getAsInteger(10, Reg) && Reg >= 1 &&
         Reg <= 31;
}

static bool isRetainedYSXFeature(StringRef Feature) {
  return isRequiredYSXFeature(Feature) || Feature == "relax" ||
         Feature == "exact-asm" || isYSXReserveXFeature(Feature);
}

static std::string filterYSXFeatureString(StringRef FS) {
  SmallVector<StringRef, 8> Features;
  FS.split(Features, ",", /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  std::string FilteredFS;
  for (StringRef Feature : Features) {
    Feature = Feature.trim();
    bool Enabled = true;
    if (Feature.consume_front("+"))
      Enabled = true;
    else if (Feature.consume_front("-"))
      Enabled = false;

    if (Feature == "help" || Feature == "cpuhelp") {
      if (!FilteredFS.empty())
        FilteredFS += ",";
      FilteredFS += Enabled ? "+" : "-";
      FilteredFS += Feature;
      continue;
    }

    if (Feature == "32bit" || !isKnownYSXFeature(Feature) ||
        !isRetainedYSXFeature(Feature)) {
      if (!Enabled)
        continue;
      reportFatalUsageError("YSX only supports the rv64ima ISA");
    }

    if (!Enabled && isRequiredYSXFeature(Feature))
      reportFatalUsageError("YSX only supports the rv64ima ISA");

    if (!FilteredFS.empty())
      FilteredFS += ",";
    FilteredFS += Enabled ? "+" : "-";
    FilteredFS += Feature;
  }
  return FilteredFS;
}

static bool hasYSXHelpFeature(StringRef FS) {
  SmallVector<StringRef, 8> Features;
  FS.split(Features, ",", /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  for (StringRef Feature : Features) {
    Feature = Feature.trim();
    Feature.consume_front("+") || Feature.consume_front("-");
    if (Feature == "help" || Feature == "cpuhelp")
      return true;
  }
  return false;
}

static const SubtargetFeatureKV &findYSXFeature(StringRef Name) {
  auto Features = ArrayRef(YSXFeatureKV);
  auto It = llvm::lower_bound(Features, Name);
  assert(It != Features.end() && StringRef(It->Key) == Name &&
         "missing YSX feature help entry");
  return *It;
}

static const SubtargetSubTypeKV &findYSXCPU(StringRef Name) {
  auto CPUs = ArrayRef(YSXSubTypeKV);
  auto It = llvm::lower_bound(CPUs, Name);
  assert(It != CPUs.end() && StringRef(It->Key) == Name &&
         "missing YSX CPU help entry");
  return *It;
}

static ArrayRef<StringRef> getYSXHelpCPUNames() {
  static const StringRef Names[] = {"generic", "generic-rv64"};
  return Names;
}

static ArrayRef<SubtargetSubTypeKV> getYSXHelpCPUDescs() {
  static const std::vector<SubtargetSubTypeKV> CPUs = {
      findYSXCPU("generic"), findYSXCPU("generic-rv64")};
  return CPUs;
}

static ArrayRef<SubtargetFeatureKV> getYSXHelpFeatures() {
  static const std::vector<SubtargetFeatureKV> Features = {
      findYSXFeature("64bit"),    findYSXFeature("a"),
      findYSXFeature("exact-asm"), findYSXFeature("i"),
      findYSXFeature("m"),        findYSXFeature("relax"),
      findYSXFeature("zaamo"),    findYSXFeature("zalrsc"),
      findYSXFeature("zmmul")};
  return Features;
}

namespace {
class YSXHelpMCSubtargetInfo final : public YSXGenMCSubtargetInfo {
public:
  YSXHelpMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS)
      : YSXGenMCSubtargetInfo(TT, CPU, CPU, FS, getYSXHelpCPUNames(),
                              getYSXHelpFeatures(), getYSXHelpCPUDescs(),
                              YSXWriteProcResTable, YSXWriteLatencyTable,
                              YSXReadAdvanceTable, nullptr, nullptr, nullptr) {}
};
} // namespace

static MCInstrInfo *createYSXMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitYSXMCInstrInfo(X);
  return X;
}

static MCRegisterInfo *createYSXMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitYSXMCRegisterInfo(X, YSX::X1);
  return X;
}

static MCAsmInfo *createYSXMCAsmInfo(const MCRegisterInfo &MRI,
                                       const Triple &TT,
                                       const MCTargetOptions &Options) {
  MCAsmInfo *MAI = nullptr;
  if (TT.isOSBinFormatELF())
    MAI = new YSXMCAsmInfo(TT);
  else if (TT.isOSBinFormatMachO())
    MAI = new YSXMCAsmInfoDarwin();
  else
    reportFatalUsageError("unsupported object format");

  unsigned SP = MRI.getDwarfRegNum(YSX::X2, true);
  MCCFIInstruction Inst = MCCFIInstruction::cfiDefCfa(nullptr, SP, 0);
  MAI->addInitialFrameState(Inst);

  return MAI;
}

static MCObjectFileInfo *
createYSXMCObjectFileInfo(MCContext &Ctx, bool PIC,
                            bool LargeCodeModel = false) {
  MCObjectFileInfo *MOFI = new YSXMCObjectFileInfo();
  MOFI->initMCObjectFileInfo(Ctx, PIC, LargeCodeModel);
  return MOFI;
}

static MCSubtargetInfo *createYSXMCSubtargetInfo(const Triple &TT,
                                                   StringRef CPU, StringRef FS) {
  if (!TT.isYSX64())
    reportFatalUsageError("YSX only supports the ysx64 target");

  if (CPU == "help" || hasYSXHelpFeature(FS)) {
    StringRef HelpCPU = CPU;
    if (HelpCPU.empty() || HelpCPU == "generic")
      HelpCPU = "generic-rv64";
    else if (HelpCPU != "help" && HelpCPU != "generic-rv64")
      reportFatalUsageError("YSX only supports -mcpu=generic-rv64");

    MCSubtargetInfo *X = new YSXHelpMCSubtargetInfo(TT, HelpCPU, FS);
    llvm::FeatureBitset Features = X->getFeatureBits();
    Features.set(YSX::Feature64Bit);
    Features.set(YSX::FeatureStdExtI);
    Features.set(YSX::FeatureStdExtM);
    Features.set(YSX::FeatureStdExtA);
    X->setFeatureBits(Features);
    return X;
  }

  if (CPU.empty() || CPU == "generic")
    CPU = "generic-rv64";
  else if (CPU != "generic-rv64")
    reportFatalUsageError("YSX only supports -mcpu=generic-rv64");

  if (FS.empty())
    FS = "+m,+a";
  std::string FilteredFS = filterYSXFeatureString(FS);

  MCSubtargetInfo *X =
      createYSXMCSubtargetInfoImpl(TT, CPU, /*TuneCPU*/ CPU, FilteredFS);

  return X;
}

static MCInstPrinter *createYSXMCInstPrinter(const Triple &T,
                                               unsigned SyntaxVariant,
                                               const MCAsmInfo &MAI,
                                               const MCInstrInfo &MII,
                                               const MCRegisterInfo &MRI) {
  return new YSXInstPrinter(MAI, MII, MRI);
}

static MCTargetStreamer *
createYSXObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  const Triple &TT = STI.getTargetTriple();
  if (TT.isOSBinFormatELF())
    return new YSXTargetELFStreamer(S, STI);
  return new YSXTargetStreamer(S);
}

static MCStreamer *
createMachOStreamer(MCContext &Ctx, std::unique_ptr<MCAsmBackend> &&TAB,
                    std::unique_ptr<MCObjectWriter> &&OW,
                    std::unique_ptr<MCCodeEmitter> &&Emitter) {
  return createMachOStreamer(Ctx, std::move(TAB), std::move(OW),
                             std::move(Emitter),
                             /*DWARFMustBeAtTheEnd*/ false,
                             /*LabelSections*/ true);
}

static MCTargetStreamer *
createYSXAsmTargetStreamer(MCStreamer &S, formatted_raw_ostream &OS,
                             MCInstPrinter *InstPrint) {
  return new YSXTargetAsmStreamer(S, OS);
}

static MCTargetStreamer *createYSXNullTargetStreamer(MCStreamer &S) {
  return new YSXTargetStreamer(S);
}

namespace {

class YSXMCInstrAnalysis : public MCInstrAnalysis {
  int64_t GPRState[31] = {};
  std::bitset<31> GPRValidMask;

  static bool isGPR(MCRegister Reg) {
    return Reg >= YSX::X0 && Reg <= YSX::X31;
  }

  static unsigned getRegIndex(MCRegister Reg) {
    assert(isGPR(Reg) && Reg != YSX::X0 && "Invalid GPR reg");
    return Reg - YSX::X1;
  }

  void setGPRState(MCRegister Reg, std::optional<int64_t> Value) {
    if (Reg == YSX::X0)
      return;

    auto Index = getRegIndex(Reg);

    if (Value) {
      GPRState[Index] = *Value;
      GPRValidMask.set(Index);
    } else {
      GPRValidMask.reset(Index);
    }
  }

  std::optional<int64_t> getGPRState(MCRegister Reg) const {
    if (Reg == YSX::X0)
      return 0;

    auto Index = getRegIndex(Reg);

    if (GPRValidMask.test(Index))
      return GPRState[Index];
    return std::nullopt;
  }

public:
  explicit YSXMCInstrAnalysis(const MCInstrInfo *Info)
      : MCInstrAnalysis(Info) {}

  void resetState() override { GPRValidMask.reset(); }

  void updateState(const MCInst &Inst, uint64_t Addr) override {
    // Terminators mark the end of a basic block which means the sequentially
    // next instruction will be the first of another basic block and the current
    // state will typically not be valid anymore. For calls, we assume all
    // registers may be clobbered by the callee (TODO: should we take the
    // calling convention into account?).
    if (isTerminator(Inst) || isCall(Inst)) {
      resetState();
      return;
    }

    switch (Inst.getOpcode()) {
    default: {
      // Clear the state of all defined registers for instructions that we don't
      // explicitly support.
      auto NumDefs = Info->get(Inst.getOpcode()).getNumDefs();
      for (unsigned I = 0; I < NumDefs; ++I) {
        auto DefReg = Inst.getOperand(I).getReg();
        if (isGPR(DefReg))
          setGPRState(DefReg, std::nullopt);
      }
      break;
    }
    case YSX::AUIPC:
      setGPRState(Inst.getOperand(0).getReg(),
                  Addr + SignExtend64<32>(Inst.getOperand(1).getImm() << 12));
      break;
    }
  }

  bool evaluateBranch(const MCInst &Inst, uint64_t Addr, uint64_t Size,
                      uint64_t &Target) const override {
    if (isConditionalBranch(Inst)) {
      int64_t Imm;
      if (Size == 2)
        Imm = Inst.getOperand(1).getImm();
      else
        Imm = Inst.getOperand(2).getImm();
      Target = Addr + Imm;
      return true;
    }

    switch (Inst.getOpcode()) {
    case YSX::JAL:
      Target = Addr + Inst.getOperand(1).getImm();
      return true;
    case YSX::JALR: {
      if (auto TargetRegState = getGPRState(Inst.getOperand(1).getReg())) {
        Target = *TargetRegState + Inst.getOperand(2).getImm();
        return true;
      }
      return false;
    }
    }

    return false;
  }

  bool isTerminator(const MCInst &Inst) const override {
    if (MCInstrAnalysis::isTerminator(Inst))
      return true;

    switch (Inst.getOpcode()) {
    default:
      return false;
    case YSX::JAL:
    case YSX::JALR:
      return Inst.getOperand(0).getReg() == YSX::X0;
    }
  }

  bool isCall(const MCInst &Inst) const override {
    if (MCInstrAnalysis::isCall(Inst))
      return true;

    switch (Inst.getOpcode()) {
    default:
      return false;
    case YSX::JAL:
    case YSX::JALR:
      return Inst.getOperand(0).getReg() != YSX::X0;
    }
  }

  bool isReturn(const MCInst &Inst) const override {
    if (MCInstrAnalysis::isReturn(Inst))
      return true;

    switch (Inst.getOpcode()) {
    default:
      return false;
    case YSX::JALR:
      return Inst.getOperand(0).getReg() == YSX::X0 &&
             maybeReturnAddress(Inst.getOperand(1).getReg());
    }
  }

  bool isBranch(const MCInst &Inst) const override {
    if (MCInstrAnalysis::isBranch(Inst))
      return true;

    return isBranchImpl(Inst);
  }

  bool isUnconditionalBranch(const MCInst &Inst) const override {
    if (MCInstrAnalysis::isUnconditionalBranch(Inst))
      return true;

    return isBranchImpl(Inst);
  }

  bool isIndirectBranch(const MCInst &Inst) const override {
    if (MCInstrAnalysis::isIndirectBranch(Inst))
      return true;

    switch (Inst.getOpcode()) {
    default:
      return false;
    case YSX::JALR:
      return Inst.getOperand(0).getReg() == YSX::X0 &&
             !maybeReturnAddress(Inst.getOperand(1).getReg());
    }
  }

  /// Returns (PLT virtual address, GOT virtual address) pairs for PLT entries.
  std::vector<std::pair<uint64_t, uint64_t>>
  findPltEntries(uint64_t PltSectionVA, ArrayRef<uint8_t> PltContents,
                 const MCSubtargetInfo &STI) const override {
    if (!STI.getTargetTriple().isYSX64())
      return {};
    uint32_t LoadInsnOpCode = 0x3003; // ld

    constexpr uint64_t FirstEntryAt = 32, EntrySize = 16;
    if (PltContents.size() < FirstEntryAt + EntrySize)
      return {};

    std::vector<std::pair<uint64_t, uint64_t>> Results;
    for (uint64_t EntryStart = FirstEntryAt,
                  EntryStartEnd = PltContents.size() - EntrySize;
         EntryStart <= EntryStartEnd; EntryStart += EntrySize) {
      const uint32_t AuipcInsn =
          support::endian::read32le(PltContents.data() + EntryStart);
      const bool IsAuipc = (AuipcInsn & 0x7F) == 0x17;
      if (!IsAuipc)
        continue;

      const uint32_t LoadInsn =
          support::endian::read32le(PltContents.data() + EntryStart + 4);
      const bool IsLoad = (LoadInsn & 0x707F) == LoadInsnOpCode;
      if (!IsLoad)
        continue;

      const uint64_t GotPltSlotVA = PltSectionVA + EntryStart +
                                    (AuipcInsn & 0xFFFFF000) +
                                    SignExtend64<12>(LoadInsn >> 20);
      Results.emplace_back(PltSectionVA + EntryStart, GotPltSlotVA);
    }

    return Results;
  }

private:
  static bool maybeReturnAddress(MCRegister Reg) {
    // X1 is used for normal returns, X5 for returns from outlined functions.
    return Reg == YSX::X1 || Reg == YSX::X5;
  }

  static bool isBranchImpl(const MCInst &Inst) {
    switch (Inst.getOpcode()) {
    default:
      return false;
    case YSX::JAL:
      return Inst.getOperand(0).getReg() == YSX::X0;
    case YSX::JALR:
      return Inst.getOperand(0).getReg() == YSX::X0 &&
             !maybeReturnAddress(Inst.getOperand(1).getReg());
    }
  }
};

} // end anonymous namespace

static MCInstrAnalysis *createYSXInstrAnalysis(const MCInstrInfo *Info) {
  return new YSXMCInstrAnalysis(Info);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYSXTargetMC() {
  for (Target *T : {&getTheYSX64Target()}) {
    TargetRegistry::RegisterMCAsmInfo(*T, createYSXMCAsmInfo);
    TargetRegistry::RegisterMCObjectFileInfo(*T, createYSXMCObjectFileInfo);
    TargetRegistry::RegisterMCInstrInfo(*T, createYSXMCInstrInfo);
    TargetRegistry::RegisterMCRegInfo(*T, createYSXMCRegisterInfo);
    TargetRegistry::RegisterMCAsmBackend(*T, createYSXAsmBackend);
    TargetRegistry::RegisterMCCodeEmitter(*T, createYSXMCCodeEmitter);
    TargetRegistry::RegisterMCInstPrinter(*T, createYSXMCInstPrinter);
    TargetRegistry::RegisterMCSubtargetInfo(*T, createYSXMCSubtargetInfo);
    TargetRegistry::RegisterELFStreamer(*T, createYSXELFStreamer);
    TargetRegistry::RegisterMachOStreamer(*T, createMachOStreamer);
    TargetRegistry::RegisterObjectTargetStreamer(
        *T, createYSXObjectTargetStreamer);
    TargetRegistry::RegisterMCInstrAnalysis(*T, createYSXInstrAnalysis);

    // Register the asm target streamer.
    TargetRegistry::RegisterAsmTargetStreamer(*T, createYSXAsmTargetStreamer);
    // Register the null target streamer.
    TargetRegistry::RegisterNullTargetStreamer(*T,
                                               createYSXNullTargetStreamer);
  }
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYuShuXinTargetMC() {
  LLVMInitializeYSXTargetMC();
}
