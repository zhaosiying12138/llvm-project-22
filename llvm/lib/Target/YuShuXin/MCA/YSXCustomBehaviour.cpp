//===------------------- YSXCustomBehaviour.cpp ---------------*-C++ -* -===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
///
/// This file implements methods from the YSXCustomBehaviour class.
///
//===----------------------------------------------------------------------===//

#include "YSXCustomBehaviour.h"
#include "MCTargetDesc/YSXMCTargetDesc.h"
#include "YSX.h"
#include "TargetInfo/YSXTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DebugLog.h"

#define DEBUG_TYPE "llvm-mca-ysx-custombehaviour"

namespace llvm::YSX {
struct VXMemOpInfo {
  unsigned Log2IdxEEW : 3;
  unsigned IsOrdered : 1;
  unsigned IsStore : 1;
  unsigned NF : 4;
  unsigned BaseInstr;
};

#define GET_YSXBaseVXMemOpTable_IMPL
#include "YSXGenSearchableTables.inc"
} // namespace llvm::YSX

namespace llvm {
namespace mca {

const llvm::StringRef YSXLMULInstrument::DESC_NAME = "YSX-LMUL";

bool YSXLMULInstrument::isDataValid(llvm::StringRef Data) {
  // Return true if not one of the valid LMUL strings
  return StringSwitch<bool>(Data)
      .Cases({"M1", "M2", "M4", "M8", "MF2", "MF4", "MF8"}, true)
      .Default(false);
}

uint8_t YSXLMULInstrument::getLMUL() const {
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

const llvm::StringRef YSXSEWInstrument::DESC_NAME = "YSX-SEW";

bool YSXSEWInstrument::isDataValid(llvm::StringRef Data) {
  // Return true if not one of the valid SEW strings
  return StringSwitch<bool>(Data)
      .Cases({"E8", "E16", "E32", "E64"}, true)
      .Default(false);
}

uint8_t YSXSEWInstrument::getSEW() const {
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

bool YSXInstrumentManager::supportsInstrumentType(
    llvm::StringRef Type) const {
  return Type == YSXLMULInstrument::DESC_NAME ||
         Type == YSXSEWInstrument::DESC_NAME ||
         InstrumentManager::supportsInstrumentType(Type);
}

UniqueInstrument
YSXInstrumentManager::createInstrument(llvm::StringRef Desc,
                                         llvm::StringRef Data) {
  if (Desc == YSXLMULInstrument::DESC_NAME) {
    if (!YSXLMULInstrument::isDataValid(Data)) {
      LDBG() << "RVCB: Bad data for instrument kind " << Desc << ": " << Data
             << '\n';
      return nullptr;
    }
    return std::make_unique<YSXLMULInstrument>(Data);
  }

  if (Desc == YSXSEWInstrument::DESC_NAME) {
    if (!YSXSEWInstrument::isDataValid(Data)) {
      LDBG() << "RVCB: Bad data for instrument kind " << Desc << ": " << Data
             << '\n';
      return nullptr;
    }
    return std::make_unique<YSXSEWInstrument>(Data);
  }

  LDBG() << "RVCB: Creating default instrument for Desc: " << Desc << '\n';
  return InstrumentManager::createInstrument(Desc, Data);
}

SmallVector<UniqueInstrument>
YSXInstrumentManager::createInstruments(const MCInst &Inst) {
  if (Inst.getOpcode() == YSX::VSETVLI ||
      Inst.getOpcode() == YSX::VSETIVLI) {
    LDBG() << "RVCB: Found VSETVLI and creating instrument for it: " << Inst
           << "\n";
    unsigned VTypeI = Inst.getOperand(2).getImm();
    YSXVType::VLMUL VLMUL = YSXVType::getVLMUL(VTypeI);

    StringRef LMUL;
    switch (VLMUL) {
    case YSXVType::LMUL_1:
      LMUL = "M1";
      break;
    case YSXVType::LMUL_2:
      LMUL = "M2";
      break;
    case YSXVType::LMUL_4:
      LMUL = "M4";
      break;
    case YSXVType::LMUL_8:
      LMUL = "M8";
      break;
    case YSXVType::LMUL_F2:
      LMUL = "MF2";
      break;
    case YSXVType::LMUL_F4:
      LMUL = "MF4";
      break;
    case YSXVType::LMUL_F8:
      LMUL = "MF8";
      break;
    case YSXVType::LMUL_RESERVED:
      llvm_unreachable("Cannot create instrument for LMUL_RESERVED");
    }
    SmallVector<UniqueInstrument> Instruments;
    Instruments.emplace_back(
        createInstrument(YSXLMULInstrument::DESC_NAME, LMUL));

    unsigned SEW = YSXVType::getSEW(VTypeI);
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
        createInstrument(YSXSEWInstrument::DESC_NAME, SEWStr));

    return Instruments;
  }
  return SmallVector<UniqueInstrument>();
}

static std::pair<uint8_t, uint8_t>
getEEWAndEMUL(unsigned Opcode, YSXVType::VLMUL LMUL, uint8_t SEW) {
  uint8_t EEW;
  switch (Opcode) {
  case YSX::VLM_V:
  case YSX::VSM_V:
  case YSX::VLE8_V:
  case YSX::VSE8_V:
  case YSX::VLSE8_V:
  case YSX::VSSE8_V:
    EEW = 8;
    break;
  case YSX::VLE16_V:
  case YSX::VSE16_V:
  case YSX::VLSE16_V:
  case YSX::VSSE16_V:
    EEW = 16;
    break;
  case YSX::VLE32_V:
  case YSX::VSE32_V:
  case YSX::VLSE32_V:
  case YSX::VSSE32_V:
    EEW = 32;
    break;
  case YSX::VLE64_V:
  case YSX::VSE64_V:
  case YSX::VLSE64_V:
  case YSX::VSSE64_V:
    EEW = 64;
    break;
  default:
    llvm_unreachable("Could not determine EEW from Opcode");
  }

  auto EMUL =
      YSXVType::getSameRatioLMUL(YSXVType::getSEWLMULRatio(SEW, LMUL), EEW);
  if (!EEW)
    llvm_unreachable("Invalid SEW or LMUL for new ratio");
  return std::make_pair(EEW, *EMUL);
}

static bool opcodeHasEEWAndEMULInfo(unsigned short Opcode) {
  return Opcode == YSX::VLM_V || Opcode == YSX::VSM_V ||
         Opcode == YSX::VLE8_V || Opcode == YSX::VSE8_V ||
         Opcode == YSX::VLE16_V || Opcode == YSX::VSE16_V ||
         Opcode == YSX::VLE32_V || Opcode == YSX::VSE32_V ||
         Opcode == YSX::VLE64_V || Opcode == YSX::VSE64_V ||
         Opcode == YSX::VLSE8_V || Opcode == YSX::VSSE8_V ||
         Opcode == YSX::VLSE16_V || Opcode == YSX::VSSE16_V ||
         Opcode == YSX::VLSE32_V || Opcode == YSX::VSSE32_V ||
         Opcode == YSX::VLSE64_V || Opcode == YSX::VSSE64_V;
}

unsigned YSXInstrumentManager::getSchedClassID(
    const MCInstrInfo &MCII, const MCInst &MCI,
    const llvm::SmallVector<Instrument *> &IVec) const {
  unsigned short Opcode = MCI.getOpcode();
  unsigned SchedClassID = MCII.get(Opcode).getSchedClass();

  // Unpack all possible RISC-V instruments from IVec.
  YSXLMULInstrument *LI = nullptr;
  YSXSEWInstrument *SI = nullptr;
  for (auto &I : IVec) {
    if (I->getDesc() == YSXLMULInstrument::DESC_NAME)
      LI = static_cast<YSXLMULInstrument *>(I);
    else if (I->getDesc() == YSXSEWInstrument::DESC_NAME)
      SI = static_cast<YSXSEWInstrument *>(I);
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
  if (const auto *VXMO = YSX::getVXMemOpInfo(Opcode)) {
    // Calculate the expected index EMUL. For indexed operations,
    // the DataEEW and DataEMUL are equal to SEW and LMUL, respectively.
    unsigned IndexEMUL = ((1 << VXMO->Log2IdxEEW) * LMUL) / SEW;

    if (!VXMO->NF) {
      // Indexed Load / Store.
      if (VXMO->IsStore) {
        if (const auto *VXP = YSX::getVSXPseudo(
                /*Masked=*/0, VXMO->IsOrdered, VXMO->Log2IdxEEW, LMUL,
                IndexEMUL))
          VPOpcode = VXP->Pseudo;
      } else {
        if (const auto *VXP = YSX::getVLXPseudo(
                /*Masked=*/0, VXMO->IsOrdered, VXMO->Log2IdxEEW, LMUL,
                IndexEMUL))
          VPOpcode = VXP->Pseudo;
      }
    } else {
      // Segmented Indexed Load / Store.
      if (VXMO->IsStore) {
        if (const auto *VXP =
                YSX::getVSXSEGPseudo(VXMO->NF, /*Masked=*/0, VXMO->IsOrdered,
                                       VXMO->Log2IdxEEW, LMUL, IndexEMUL))
          VPOpcode = VXP->Pseudo;
      } else {
        if (const auto *VXP =
                YSX::getVLXSEGPseudo(VXMO->NF, /*Masked=*/0, VXMO->IsOrdered,
                                       VXMO->Log2IdxEEW, LMUL, IndexEMUL))
          VPOpcode = VXP->Pseudo;
      }
    }
  } else if (opcodeHasEEWAndEMULInfo(Opcode)) {
    YSXVType::VLMUL VLMUL = static_cast<YSXVType::VLMUL>(LMUL);
    auto [EEW, EMUL] = getEEWAndEMUL(Opcode, VLMUL, SEW);
    if (const auto *RVV =
            YSXVInversePseudosTable::getBaseInfo(Opcode, EMUL, EEW))
      VPOpcode = RVV->Pseudo;
  } else {
    // Check if it depends on LMUL and SEW
    const auto *RVV = YSXVInversePseudosTable::getBaseInfo(Opcode, LMUL, SEW);
    // Check if it depends only on LMUL
    if (!RVV)
      RVV = YSXVInversePseudosTable::getBaseInfo(Opcode, LMUL, 0);

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

static InstrumentManager *
createYSXInstrumentManager(const MCSubtargetInfo &STI,
                             const MCInstrInfo &MCII) {
  return new YSXInstrumentManager(STI, MCII);
}

/// Extern function to initialize the targets for the RISC-V backend
extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYSXTargetMCA() {
  TargetRegistry::RegisterInstrumentManager(getTheYSX64Target(),
                                            createYSXInstrumentManager);
}

extern "C" LLVM_ABI LLVM_EXTERNAL_VISIBILITY void
LLVMInitializeYuShuXinTargetMCA() {
  LLVMInitializeYSXTargetMCA();
}
