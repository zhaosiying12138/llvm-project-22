//===------------------- RISCVCustomBehaviour.cpp ---------------*-C++ -* -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file implements methods from the RISCVCustomBehaviour class.
///
//===----------------------------------------------------------------------===//

#include "RISCVCustomBehaviour.h"
#include "MCTargetDesc/RISCVMCTargetDesc.h"
#include "RISCV.h"
#include "TargetInfo/RISCVTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/MCA/Instruction.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DebugLog.h"
#include <string>

#define DEBUG_TYPE "llvm-mca-riscv-custombehaviour"

static llvm::cl::opt<bool> RISCVRVVWarHazardModel(
    "riscv-rvv-war-hazard-model", llvm::cl::Hidden, llvm::cl::init(false),
    llvm::cl::desc("Model RVV write-after-read issue hazards in llvm-mca"));

namespace llvm::RISCV {
struct VXMemOpInfo {
  unsigned Log2IdxEEW : 3;
  unsigned IsOrdered : 1;
  unsigned IsStore : 1;
  unsigned NF : 4;
  unsigned BaseInstr;
};

#define GET_RISCVBaseVXMemOpTable_IMPL
#include "RISCVGenSearchableTables.inc"
} // namespace llvm::RISCV

namespace llvm {
namespace mca {

static bool isRVVPhysReg(MCPhysReg Reg) {
  return Reg >= RISCV::V0 && Reg <= RISCV::V31;
}

static bool rvvRegsOverlap(MCPhysReg LHS, MCPhysReg RHS) {
  return isRVVPhysReg(LHS) && LHS == RHS;
}

static std::string getRVVRegName(MCPhysReg Reg) {
  if (!isRVVPhysReg(Reg))
    return "unknown";
  return "v" + std::to_string(Reg - RISCV::V0);
}

static uint64_t getRVVWarHazardKey(unsigned WriterIndex, unsigned ReaderIndex,
                                   MCPhysReg Reg) {
  return (static_cast<uint64_t>(WriterIndex) << 32) |
         (static_cast<uint64_t>(ReaderIndex) << 16) |
         static_cast<uint64_t>(Reg);
}

bool RISCVCustomBehaviour::hasRVVWarHazard(const InstRef &Writer,
                                           const InstRef &Reader,
                                           MCPhysReg &Reg) const {
  const Instruction *WriterIS = Writer.getInstruction();
  const Instruction *ReaderIS = Reader.getInstruction();
  if (!WriterIS || !ReaderIS)
    return false;

  for (const WriteState &Def : WriterIS->getDefs()) {
    MCPhysReg DefReg = Def.getRegisterID();
    if (!isRVVPhysReg(DefReg))
      continue;
    for (const ReadState &Use : ReaderIS->getUses()) {
      MCPhysReg UseReg = Use.getRegisterID();
      if (rvvRegsOverlap(DefReg, UseReg)) {
        Reg = DefReg;
        return true;
      }
    }
  }
  return false;
}

void RISCVCustomBehaviour::recordRVVWarHazard(const InstRef &Writer,
                                              const InstRef &Reader,
                                              MCPhysReg Reg) {
  ++BlockedIssueEvents;
  uint64_t Key =
      getRVVWarHazardKey(Writer.getSourceIndex(), Reader.getSourceIndex(), Reg);
  if (!SeenHazards.insert(Key).second)
    return;

  HazardRegisters.insert(Reg);
  if (Samples.size() < 8)
    Samples.push_back({Writer.getSourceIndex(), Reader.getSourceIndex(), Reg});
}

bool RISCVCustomBehaviour::checkCustomIssueHazard(const InstRef &IR,
                                                  ArrayRef<InstRef> WaitSet,
                                                  ArrayRef<InstRef> PendingSet,
                                                  ArrayRef<InstRef> ReadySet) {
  if (!RISCVRVVWarHazardModel)
    return false;

  auto CheckSet = [&](ArrayRef<InstRef> Set) {
    for (const InstRef &Other : Set) {
      if (!Other || Other == IR || Other.getSourceIndex() >= IR.getSourceIndex())
        continue;
      MCPhysReg Reg = 0;
      if (hasRVVWarHazard(IR, Other, Reg)) {
        recordRVVWarHazard(IR, Other, Reg);
        return true;
      }
    }
    return false;
  };

  return CheckSet(WaitSet) || CheckSet(PendingSet) || CheckSet(ReadySet);
}

void RISCVCustomBehaviour::noteCustomIssueBlockedCycle() {
  if (RISCVRVVWarHazardModel)
    ++BlockedIssueCycles;
}

namespace {
class RISCVRVVWarHazardView : public View {
  const RISCVCustomBehaviour &CB;

public:
  RISCVRVVWarHazardView(const RISCVCustomBehaviour &CB) : CB(CB) {}

  void printView(raw_ostream &OS) const override {
    OS << "\n\nRVV WAR Hazard\n";
    OS << "Total hazards: " << CB.getTotalRVVWarHazards() << '\n';
    OS << "Blocked issue events: " << CB.getBlockedIssueEvents() << '\n';
    OS << "Blocked issue cycles: " << CB.getBlockedIssueCycles() << '\n';
    OS << "Registers:";
    if (CB.getHazardRegisters().empty()) {
      OS << " none\n";
    } else {
      for (MCPhysReg Reg : CB.getHazardRegisters())
        OS << ' ' << getRVVRegName(Reg);
      OS << '\n';
    }

    OS << "Samples:\n";
    if (CB.getRVVWarSamples().empty()) {
      OS << "  none\n";
      return;
    }
    for (const RISCVCustomBehaviour::RVVWarSample &Sample :
         CB.getRVVWarSamples()) {
      OS << "  #" << Sample.WriterIndex << " waits for #"
         << Sample.ReaderIndex << " on " << getRVVRegName(Sample.Reg) << '\n';
    }
  }

  StringRef getNameAsString() const override {
    return "RISCVRVVWarHazardView";
  }
};
} // namespace

std::vector<std::unique_ptr<View>>
RISCVCustomBehaviour::getEndViews(llvm::MCInstPrinter &IP,
                                  llvm::ArrayRef<llvm::MCInst> Insts) {
  if (!RISCVRVVWarHazardModel)
    return {};
  std::vector<std::unique_ptr<View>> Views;
  Views.push_back(std::make_unique<RISCVRVVWarHazardView>(*this));
  return Views;
}

const llvm::StringRef RISCVLMULInstrument::DESC_NAME = "RISCV-LMUL";

bool RISCVLMULInstrument::isDataValid(llvm::StringRef Data) {
  // Return true if not one of the valid LMUL strings
  return StringSwitch<bool>(Data)
      .Cases({"M1", "M2", "M4", "M8", "MF2", "MF4", "MF8"}, true)
      .Default(false);
}

uint8_t RISCVLMULInstrument::getLMUL() const {
  // assertion prevents us from needing llvm_unreachable in the StringSwitch
  // below
  assert(isDataValid(getData()) &&
         "Cannot get LMUL because invalid Data value");
  // These are the LMUL values that are used in RISC-V tablegen
  return StringSwitch<uint8_t>(getData())
      .Case("M1", 0b000)
      .Case("M2", 0b001)
      .Case("M4", 0b010)
      .Case("M8", 0b011)
      .Case("MF2", 0b111)
      .Case("MF4", 0b110)
      .Case("MF8", 0b101);
}

const llvm::StringRef RISCVSEWInstrument::DESC_NAME = "RISCV-SEW";

bool RISCVSEWInstrument::isDataValid(llvm::StringRef Data) {
  // Return true if not one of the valid SEW strings
  return StringSwitch<bool>(Data)
      .Cases({"E8", "E16", "E32", "E64"}, true)
      .Default(false);
}

uint8_t RISCVSEWInstrument::getSEW() const {
  // assertion prevents us from needing llvm_unreachable in the StringSwitch
  // below
  assert(isDataValid(getData()) && "Cannot get SEW because invalid Data value");
  // These are the LMUL values that are used in RISC-V tablegen
  return StringSwitch<uint8_t>(getData())
      .Case("E8", 8)
      .Case("E16", 16)
      .Case("E32", 32)
      .Case("E64", 64);
}

bool RISCVInstrumentManager::supportsInstrumentType(
    llvm::StringRef Type) const {
  return Type == RISCVLMULInstrument::DESC_NAME ||
         Type == RISCVSEWInstrument::DESC_NAME ||
         InstrumentManager::supportsInstrumentType(Type);
}

UniqueInstrument
RISCVInstrumentManager::createInstrument(llvm::StringRef Desc,
                                         llvm::StringRef Data) {
  if (Desc == RISCVLMULInstrument::DESC_NAME) {
    if (!RISCVLMULInstrument::isDataValid(Data)) {
      LDBG() << "RVCB: Bad data for instrument kind " << Desc << ": " << Data
             << '\n';
      return nullptr;
    }
    return std::make_unique<RISCVLMULInstrument>(Data);
  }

  if (Desc == RISCVSEWInstrument::DESC_NAME) {
    if (!RISCVSEWInstrument::isDataValid(Data)) {
      LDBG() << "RVCB: Bad data for instrument kind " << Desc << ": " << Data
             << '\n';
      return nullptr;
    }
    return std::make_unique<RISCVSEWInstrument>(Data);
  }

  LDBG() << "RVCB: Creating default instrument for Desc: " << Desc << '\n';
  return InstrumentManager::createInstrument(Desc, Data);
}

SmallVector<UniqueInstrument>
RISCVInstrumentManager::createInstruments(const MCInst &Inst) {
  if (Inst.getOpcode() == RISCV::VSETVLI ||
      Inst.getOpcode() == RISCV::VSETIVLI) {
    LDBG() << "RVCB: Found VSETVLI and creating instrument for it: " << Inst
           << "\n";
    unsigned VTypeI = Inst.getOperand(2).getImm();
    RISCVVType::VLMUL VLMUL = RISCVVType::getVLMUL(VTypeI);

    StringRef LMUL;
    switch (VLMUL) {
    case RISCVVType::LMUL_1:
      LMUL = "M1";
      break;
    case RISCVVType::LMUL_2:
      LMUL = "M2";
      break;
    case RISCVVType::LMUL_4:
      LMUL = "M4";
      break;
    case RISCVVType::LMUL_8:
      LMUL = "M8";
      break;
    case RISCVVType::LMUL_F2:
      LMUL = "MF2";
      break;
    case RISCVVType::LMUL_F4:
      LMUL = "MF4";
      break;
    case RISCVVType::LMUL_F8:
      LMUL = "MF8";
      break;
    case RISCVVType::LMUL_RESERVED:
      llvm_unreachable("Cannot create instrument for LMUL_RESERVED");
    }
    SmallVector<UniqueInstrument> Instruments;
    Instruments.emplace_back(
        createInstrument(RISCVLMULInstrument::DESC_NAME, LMUL));

    unsigned SEW = RISCVVType::getSEW(VTypeI);
    StringRef SEWStr;
    switch (SEW) {
    case 8:
      SEWStr = "E8";
      break;
    case 16:
      SEWStr = "E16";
      break;
    case 32:
      SEWStr = "E32";
      break;
    case 64:
      SEWStr = "E64";
      break;
    default:
      llvm_unreachable("Cannot create instrument for SEW");
    }
    Instruments.emplace_back(
        createInstrument(RISCVSEWInstrument::DESC_NAME, SEWStr));

    return Instruments;
  }
  return SmallVector<UniqueInstrument>();
}

static std::pair<uint8_t, uint8_t>
getEEWAndEMUL(unsigned Opcode, RISCVVType::VLMUL LMUL, uint8_t SEW) {
  uint8_t EEW;
  switch (Opcode) {
  case RISCV::VLM_V:
  case RISCV::VSM_V:
  case RISCV::VLE8_V:
  case RISCV::VSE8_V:
  case RISCV::VLSE8_V:
  case RISCV::VSSE8_V:
    EEW = 8;
    break;
  case RISCV::VLE16_V:
  case RISCV::VSE16_V:
  case RISCV::VLSE16_V:
  case RISCV::VSSE16_V:
    EEW = 16;
    break;
  case RISCV::VLE32_V:
  case RISCV::VSE32_V:
  case RISCV::VLSE32_V:
  case RISCV::VSSE32_V:
    EEW = 32;
    break;
  case RISCV::VLE64_V:
  case RISCV::VSE64_V:
  case RISCV::VLSE64_V:
  case RISCV::VSSE64_V:
    EEW = 64;
    break;
  default:
    llvm_unreachable("Could not determine EEW from Opcode");
  }

  auto EMUL =
      RISCVVType::getSameRatioLMUL(RISCVVType::getSEWLMULRatio(SEW, LMUL), EEW);
  if (!EEW)
    llvm_unreachable("Invalid SEW or LMUL for new ratio");
  return std::make_pair(EEW, *EMUL);
}

static bool opcodeHasEEWAndEMULInfo(unsigned short Opcode) {
  return Opcode == RISCV::VLM_V || Opcode == RISCV::VSM_V ||
         Opcode == RISCV::VLE8_V || Opcode == RISCV::VSE8_V ||
         Opcode == RISCV::VLE16_V || Opcode == RISCV::VSE16_V ||
         Opcode == RISCV::VLE32_V || Opcode == RISCV::VSE32_V ||
         Opcode == RISCV::VLE64_V || Opcode == RISCV::VSE64_V ||
         Opcode == RISCV::VLSE8_V || Opcode == RISCV::VSSE8_V ||
         Opcode == RISCV::VLSE16_V || Opcode == RISCV::VSSE16_V ||
         Opcode == RISCV::VLSE32_V || Opcode == RISCV::VSSE32_V ||
         Opcode == RISCV::VLSE64_V || Opcode == RISCV::VSSE64_V;
}

unsigned RISCVInstrumentManager::getSchedClassID(
    const MCInstrInfo &MCII, const MCInst &MCI,
    const llvm::SmallVector<Instrument *> &IVec) const {
  unsigned short Opcode = MCI.getOpcode();
  unsigned SchedClassID = MCII.get(Opcode).getSchedClass();

  // Unpack all possible RISC-V instruments from IVec.
  RISCVLMULInstrument *LI = nullptr;
  RISCVSEWInstrument *SI = nullptr;
  for (auto &I : IVec) {
    if (I->getDesc() == RISCVLMULInstrument::DESC_NAME)
      LI = static_cast<RISCVLMULInstrument *>(I);
    else if (I->getDesc() == RISCVSEWInstrument::DESC_NAME)
      SI = static_cast<RISCVSEWInstrument *>(I);
  }

  // Need LMUL or LMUL, SEW in order to override opcode. If no LMUL is provided,
  // then no option to override.
  if (!LI) {
    LDBG() << "RVCB: Did not use instrumentation to override Opcode.\n";
    return SchedClassID;
  }
  uint8_t LMUL = LI->getLMUL();

  // getBaseInfo works with (Opcode, LMUL, 0) if no SEW instrument,
  // or (Opcode, LMUL, SEW) if SEW instrument is active, and depends on LMUL
  // and SEW, or (Opcode, LMUL, 0) if does not depend on SEW.
  uint8_t SEW = SI ? SI->getSEW() : 0;

  std::optional<unsigned> VPOpcode;
  if (const auto *VXMO = RISCV::getVXMemOpInfo(Opcode)) {
    // Calculate the expected index EMUL. For indexed operations,
    // the DataEEW and DataEMUL are equal to SEW and LMUL, respectively.
    unsigned IndexEMUL = ((1 << VXMO->Log2IdxEEW) * LMUL) / SEW;

    if (!VXMO->NF) {
      // Indexed Load / Store.
      if (VXMO->IsStore) {
        if (const auto *VXP = RISCV::getVSXPseudo(
                /*Masked=*/0, VXMO->IsOrdered, VXMO->Log2IdxEEW, LMUL,
                IndexEMUL))
          VPOpcode = VXP->Pseudo;
      } else {
        if (const auto *VXP = RISCV::getVLXPseudo(
                /*Masked=*/0, VXMO->IsOrdered, VXMO->Log2IdxEEW, LMUL,
                IndexEMUL))
          VPOpcode = VXP->Pseudo;
      }
    } else {
      // Segmented Indexed Load / Store.
      if (VXMO->IsStore) {
        if (const auto *VXP =
                RISCV::getVSXSEGPseudo(VXMO->NF, /*Masked=*/0, VXMO->IsOrdered,
                                       VXMO->Log2IdxEEW, LMUL, IndexEMUL))
          VPOpcode = VXP->Pseudo;
      } else {
        if (const auto *VXP =
                RISCV::getVLXSEGPseudo(VXMO->NF, /*Masked=*/0, VXMO->IsOrdered,
                                       VXMO->Log2IdxEEW, LMUL, IndexEMUL))
          VPOpcode = VXP->Pseudo;
      }
    }
  } else if (opcodeHasEEWAndEMULInfo(Opcode)) {
    RISCVVType::VLMUL VLMUL = static_cast<RISCVVType::VLMUL>(LMUL);
    auto [EEW, EMUL] = getEEWAndEMUL(Opcode, VLMUL, SEW);
    if (const auto *RVV =
            RISCVVInversePseudosTable::getBaseInfo(Opcode, EMUL, EEW))
      VPOpcode = RVV->Pseudo;
  } else {
    // Check if it depends on LMUL and SEW
    const auto *RVV = RISCVVInversePseudosTable::getBaseInfo(Opcode, LMUL, SEW);
    // Check if it depends only on LMUL
    if (!RVV)
      RVV = RISCVVInversePseudosTable::getBaseInfo(Opcode, LMUL, 0);

    if (RVV)
      VPOpcode = RVV->Pseudo;
  }

  // Not a RVV instr
  if (!VPOpcode) {
    LDBG() << "RVCB: Could not find PseudoInstruction for Opcode "
           << MCII.getName(Opcode)
           << ", LMUL=" << (LI ? LI->getData() : "Unspecified")
           << ", SEW=" << (SI ? SI->getData() : "Unspecified")
           << ". Ignoring instrumentation and using original SchedClassID="
           << SchedClassID << '\n';
    return SchedClassID;
  }

  // Override using pseudo
  LDBG() << "RVCB: Found Pseudo Instruction for Opcode " << MCII.getName(Opcode)
         << ", LMUL=" << LI->getData()
         << ", SEW=" << (SI ? SI->getData() : "Unspecified")
         << ". Overriding original SchedClassID=" << SchedClassID << " with "
         << MCII.getName(*VPOpcode) << '\n';
  return MCII.get(*VPOpcode).getSchedClass();
}

} // namespace mca
} // namespace llvm

using namespace llvm;
using namespace mca;

static CustomBehaviour *
createRISCVCustomBehaviour(const MCSubtargetInfo &STI,
                           const mca::SourceMgr &SrcMgr,
                           const MCInstrInfo &MCII) {
  return new RISCVCustomBehaviour(STI, SrcMgr, MCII);
}

static InstrumentManager *
createRISCVInstrumentManager(const MCSubtargetInfo &STI,
                             const MCInstrInfo &MCII) {
  return new RISCVInstrumentManager(STI, MCII);
}

/// Extern function to initialize the targets for the RISC-V backend
extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeRISCVTargetMCA() {
  TargetRegistry::RegisterCustomBehaviour(getTheRISCV32Target(),
                                          createRISCVCustomBehaviour);
  TargetRegistry::RegisterCustomBehaviour(getTheRISCV64Target(),
                                          createRISCVCustomBehaviour);
  TargetRegistry::RegisterInstrumentManager(getTheRISCV32Target(),
                                            createRISCVInstrumentManager);
  TargetRegistry::RegisterInstrumentManager(getTheRISCV64Target(),
                                            createRISCVInstrumentManager);
}
