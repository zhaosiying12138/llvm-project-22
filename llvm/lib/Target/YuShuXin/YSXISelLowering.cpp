//===-- YSXISelLowering.cpp - RISC-V DAG Lowering Implementation  -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that RISC-V uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#include "YSXISelLowering.h"
#include "MCTargetDesc/YSXMatInt.h"
#include "YSX.h"
#include "YSXConstantPoolValue.h"
#include "YSXMachineFunctionInfo.h"
#include "YSXRegisterInfo.h"
#include "YSXSelectionDAGInfo.h"
#include "YSXSubtarget.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/VectorUtils.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SDPatternMatch.h"
#include "llvm/CodeGen/SelectionDAGAddressAnalysis.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/InstructionCost.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>

using namespace llvm;

#define DEBUG_TYPE "ysx-lower"

STATISTIC(NumTailCalls, "Number of tail calls");

static cl::opt<unsigned> ExtensionMaxWebSize(
    DEBUG_TYPE "-ext-max-web-size", cl::Hidden,
    cl::desc("Give the maximum size (in number of nodes) of the web of "
             "instructions that we will consider for VW expansion"),
    cl::init(18));

static cl::opt<bool>
    AllowSplatInVW_W(DEBUG_TYPE "-form-vw-w-with-splat", cl::Hidden,
                     cl::desc("Allow the formation of VW_W operations (e.g., "
                              "VWADD_W) with splat constants"),
                     cl::init(false));

static cl::opt<unsigned> NumRepeatedDivisors(
    DEBUG_TYPE "-fp-repeated-divisors", cl::Hidden,
    cl::desc("Set the minimum number of repetitions of a divisor to allow "
             "transformation to multiplications by the reciprocal"),
    cl::init(2));

static cl::opt<int>
    FPImmCost(DEBUG_TYPE "-fpimm-cost", cl::Hidden,
              cl::desc("Give the maximum number of instructions that we will "
                       "use for creating a floating-point immediate value"),
              cl::init(3));

static cl::opt<bool>
    ReassocShlAddiAdd("ysx-reassoc-shl-addi-add", cl::Hidden,
                      cl::desc("Swap add and addi in cases where the add may "
                               "be combined with a shift"),
                      cl::init(true));

YSXTargetLowering::YSXTargetLowering(const TargetMachine &TM,
                                         const YSXSubtarget &STI)
    : TargetLowering(TM, STI), Subtarget(STI) {

  YSXABI::ABI ABI = Subtarget.getTargetABI();
  assert(ABI != YSXABI::ABI_Unknown && "Improperly initialised target ABI");

  if ((ABI == YSXABI::ABI_ILP32F || ABI == YSXABI::ABI_LP64F) &&
      !Subtarget.hasStdExtF()) {
    errs() << "Hard-float 'f' ABI can't be used for a target that "
                "doesn't support the F instruction set extension (ignoring "
                          "target-abi)\n";
    ABI = Subtarget.is64Bit() ? YSXABI::ABI_LP64 : YSXABI::ABI_ILP32;
  } else if ((ABI == YSXABI::ABI_ILP32D || ABI == YSXABI::ABI_LP64D) &&
             !Subtarget.hasStdExtD()) {
    errs() << "Hard-float 'd' ABI can't be used for a target that "
              "doesn't support the D instruction set extension (ignoring "
              "target-abi)\n";
    ABI = Subtarget.is64Bit() ? YSXABI::ABI_LP64 : YSXABI::ABI_ILP32;
  }

  switch (ABI) {
  default:
    reportFatalUsageError("Don't know how to lower this ABI");
  case YSXABI::ABI_ILP32:
  case YSXABI::ABI_ILP32E:
  case YSXABI::ABI_LP64E:
  case YSXABI::ABI_ILP32F:
  case YSXABI::ABI_ILP32D:
  case YSXABI::ABI_LP64:
  case YSXABI::ABI_LP64F:
  case YSXABI::ABI_LP64D:
    break;
  }

  MVT XLenVT = Subtarget.getXLenVT();

  // Set up the register classes.
  addRegisterClass(XLenVT, &YSX::GPRRegClass);

  // Compute derived properties from the register classes.
  computeRegisterProperties(STI.getRegisterInfo());

  setStackPointerRegisterToSaveRestore(YSX::X2);

  setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}, XLenVT,
                   MVT::i1, Promote);
  // DAGCombiner can call isLoadExtLegal for types that aren't legal.
  setLoadExtAction({ISD::EXTLOAD, ISD::SEXTLOAD, ISD::ZEXTLOAD}, MVT::i32,
                   MVT::i1, Promote);

  // TODO: add all necessary setOperationAction calls.
  setOperationAction(ISD::DYNAMIC_STACKALLOC, XLenVT, Custom);

  setOperationAction(ISD::BR_JT, MVT::Other, Expand);
  setOperationAction(ISD::BR_CC, XLenVT, Expand);
  setOperationAction(ISD::BRCOND, MVT::Other, Custom);
  setOperationAction(ISD::SELECT_CC, XLenVT, Expand);

  setCondCodeAction(ISD::SETGT, XLenVT, Custom);
  setCondCodeAction(ISD::SETGE, XLenVT, Expand);
  setCondCodeAction(ISD::SETUGT, XLenVT, Custom);
  setCondCodeAction(ISD::SETUGE, XLenVT, Expand);
  if (!(Subtarget.hasVendorXCValu() && !Subtarget.is64Bit())) {
    setCondCodeAction(ISD::SETULE, XLenVT, Expand);
    setCondCodeAction(ISD::SETLE, XLenVT, Expand);
  }

  setOperationAction({ISD::STACKSAVE, ISD::STACKRESTORE}, MVT::Other, Expand);

  setOperationAction(ISD::VASTART, MVT::Other, Custom);
  setOperationAction({ISD::VAARG, ISD::VACOPY, ISD::VAEND}, MVT::Other, Expand);

  if (!Subtarget.hasVendorXRemovedTHeadBb() && !Subtarget.hasVendorXRemovedQcibm() &&
      !Subtarget.hasVendorXAndesPerf())
    setOperationAction(ISD::SIGN_EXTEND_INREG, MVT::i1, Expand);

  setOperationAction(ISD::EH_DWARF_CFA, MVT::i32, Custom);

  if (!Subtarget.hasStdExtZbb() && !Subtarget.hasVendorXRemovedTHeadBb() &&
      !Subtarget.hasVendorXRemovedQcibm() && !Subtarget.hasVendorXAndesPerf() &&
      !(Subtarget.hasVendorXCValu() && !Subtarget.is64Bit()))
    setOperationAction(ISD::SIGN_EXTEND_INREG, {MVT::i8, MVT::i16}, Expand);

  if (Subtarget.hasStdExtZilsd() && !Subtarget.is64Bit()) {
    setOperationAction(ISD::LOAD, MVT::i64, Custom);
    setOperationAction(ISD::STORE, MVT::i64, Custom);
  }

  if (Subtarget.is64Bit()) {
    setOperationAction(ISD::EH_DWARF_CFA, MVT::i64, Custom);

    setOperationAction(ISD::LOAD, MVT::i32, Custom);
    setOperationAction({ISD::ADD, ISD::SUB, ISD::SHL, ISD::SRA, ISD::SRL},
                       MVT::i32, Custom);
    setOperationAction({ISD::UADDO, ISD::USUBO}, MVT::i32, Custom);
    setOperationAction({ISD::SADDO, ISD::SSUBO}, MVT::i32, Custom);
  }
  if (!Subtarget.hasStdExtZmmul()) {
    setOperationAction({ISD::MUL, ISD::MULHS, ISD::MULHU}, XLenVT, Expand);
  } else if (Subtarget.is64Bit()) {
    setOperationAction(ISD::MUL, MVT::i128, Custom);
    setOperationAction(ISD::MUL, MVT::i32, Custom);
  } else {
    setOperationAction(ISD::MUL, MVT::i64, Custom);
  }

  if (!Subtarget.hasStdExtM()) {
    setOperationAction({ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM}, XLenVT,
                       Expand);
  } else if (Subtarget.is64Bit()) {
    setOperationAction({ISD::SDIV, ISD::UDIV, ISD::UREM},
                       {MVT::i8, MVT::i16, MVT::i32}, Custom);
  }

  setOperationAction(
      {ISD::SDIVREM, ISD::UDIVREM, ISD::SMUL_LOHI, ISD::UMUL_LOHI}, XLenVT,
      Expand);

  setOperationAction({ISD::SHL_PARTS, ISD::SRL_PARTS, ISD::SRA_PARTS}, XLenVT,
                     Custom);

  if (Subtarget.hasStdExtZbb() || Subtarget.hasStdExtZbkb()) {
    if (Subtarget.is64Bit())
      setOperationAction({ISD::ROTL, ISD::ROTR}, MVT::i32, Custom);
  } else if (Subtarget.hasVendorXRemovedTHeadBb()) {
    if (Subtarget.is64Bit())
      setOperationAction({ISD::ROTL, ISD::ROTR}, MVT::i32, Custom);
    setOperationAction({ISD::ROTL, ISD::ROTR}, XLenVT, Custom);
  } else if (Subtarget.hasVendorXCVbitmanip() && !Subtarget.is64Bit()) {
    setOperationAction(ISD::ROTL, XLenVT, Expand);
  } else {
    setOperationAction({ISD::ROTL, ISD::ROTR}, XLenVT, Expand);
  }

  if (Subtarget.hasStdExtP())
    setOperationAction({ISD::FSHL, ISD::FSHR}, XLenVT, Legal);

  setOperationAction(ISD::BSWAP, XLenVT,
                     Subtarget.hasREV8Like() ? Legal : Expand);

  if ((Subtarget.hasVendorXCVbitmanip() || Subtarget.hasVendorXRemovedQcibm()) &&
      !Subtarget.is64Bit()) {
    setOperationAction(ISD::BITREVERSE, XLenVT, Legal);
  } else {
    // Zbkb can use rev8+brev8 to implement bitreverse.
    setOperationAction(ISD::BITREVERSE, XLenVT,
                       Subtarget.hasStdExtZbkb() ? Custom : Expand);
    if (Subtarget.hasStdExtZbkb())
      setOperationAction(ISD::BITREVERSE, MVT::i8, Custom);
  }

  if (Subtarget.hasStdExtZbb() ||
      (Subtarget.hasVendorXCValu() && !Subtarget.is64Bit())) {
    setOperationAction({ISD::SMIN, ISD::SMAX, ISD::UMIN, ISD::UMAX}, XLenVT,
                       Legal);
  }

  if (Subtarget.hasCTZLike()) {
    if (Subtarget.is64Bit())
      setOperationAction({ISD::CTTZ, ISD::CTTZ_ZERO_UNDEF}, MVT::i32, Custom);
  } else {
    setOperationAction(ISD::CTTZ, XLenVT, Expand);
  }

  if (!Subtarget.hasCPOPLike()) {
    // TODO: These should be set to LibCall, but this currently breaks
    //   the Linux kernel build. See #101786. Lacks i128 tests, too.
    if (Subtarget.is64Bit())
      setOperationAction(ISD::CTPOP, MVT::i128, Expand);
    else
      setOperationAction(ISD::CTPOP, MVT::i32, Expand);
    setOperationAction(ISD::CTPOP, MVT::i64, Expand);
  }

  if (Subtarget.hasCLZLike()) {
    // We need the custom lowering to make sure that the resulting sequence
    // for the 32bit case is efficient on 64bit targets.
    // Use default promotion for i32 without Zbb.
    if (Subtarget.is64Bit() &&
        (Subtarget.hasStdExtZbb() || Subtarget.hasStdExtP()))
      setOperationAction({ISD::CTLZ, ISD::CTLZ_ZERO_UNDEF}, MVT::i32, Custom);
  } else {
    setOperationAction(ISD::CTLZ, XLenVT, Expand);
  }

  if (Subtarget.hasStdExtP()) {
    setOperationAction(ISD::CTLS, XLenVT, Legal);
    if (Subtarget.is64Bit())
      setOperationAction(ISD::CTLS, MVT::i32, Custom);
  }

  if (Subtarget.hasStdExtP() ||
      (Subtarget.hasVendorXCValu() && !Subtarget.is64Bit())) {
    setOperationAction(ISD::ABS, XLenVT, Legal);
    if (Subtarget.is64Bit())
      setOperationAction(ISD::ABS, MVT::i32, Custom);
  } else if (Subtarget.hasShortForwardBranchIALU()) {
    // We can use PseudoCCSUB to implement ABS.
    setOperationAction(ISD::ABS, XLenVT, Legal);
  } else if (Subtarget.is64Bit()) {
    setOperationAction(ISD::ABS, MVT::i32, Custom);
  }

  if (!Subtarget.useMIPSCCMovInsn() && !Subtarget.hasVendorXRemovedTHeadCondMov())
    setOperationAction(ISD::SELECT, XLenVT, Custom);

  if ((Subtarget.hasStdExtP() || Subtarget.hasVendorXRemovedQcia()) &&
      !Subtarget.is64Bit()) {
    // FIXME: Support i32 on RV64+P by inserting into a v2i32 vector, doing
    // the vector operation and extracting.
    setOperationAction({ISD::SADDSAT, ISD::SSUBSAT, ISD::UADDSAT, ISD::USUBSAT},
                       MVT::i32, Legal);
  } else if (!Subtarget.hasStdExtZbb() && Subtarget.is64Bit()) {
    setOperationAction({ISD::SADDSAT, ISD::SSUBSAT, ISD::UADDSAT, ISD::USUBSAT},
                       MVT::i32, Custom);
  }

  if (Subtarget.hasVendorXRemovedQcia() && !Subtarget.is64Bit()) {
    setOperationAction(ISD::USHLSAT, MVT::i32, Legal);
  }

  if ((Subtarget.hasStdExtP() || Subtarget.hasVendorXRemovedQcia()) &&
      !Subtarget.is64Bit()) {
    // FIXME: Support i32 on RV64+P by inserting into a v2i32 vector, doing
    // pssha.w and extracting.
    setOperationAction(ISD::SSHLSAT, MVT::i32, Legal);
  }

  static const unsigned FPLegalNodeTypes[] = {
      ISD::FMINNUM,       ISD::FMAXNUM,        ISD::FMINIMUMNUM,
      ISD::FMAXIMUMNUM,   ISD::LRINT,          ISD::LLRINT,
      ISD::LROUND,        ISD::LLROUND,        ISD::STRICT_LRINT,
      ISD::STRICT_LLRINT, ISD::STRICT_LROUND,  ISD::STRICT_LLROUND,
      ISD::STRICT_FMA,    ISD::STRICT_FADD,    ISD::STRICT_FSUB,
      ISD::STRICT_FMUL,   ISD::STRICT_FDIV,    ISD::STRICT_FSQRT,
      ISD::STRICT_FSETCC, ISD::STRICT_FSETCCS, ISD::FCANONICALIZE};

  static const ISD::CondCode FPCCToExpand[] = {
      ISD::SETOGT, ISD::SETOGE, ISD::SETONE, ISD::SETUEQ, ISD::SETUGT,
      ISD::SETUGE, ISD::SETULT, ISD::SETULE, ISD::SETUNE, ISD::SETGT,
      ISD::SETGE,  ISD::SETNE,  ISD::SETO,   ISD::SETUO};

  static const unsigned FPOpToExpand[] = {ISD::FSIN, ISD::FCOS, ISD::FSINCOS,
                                          ISD::FPOW};
  static const unsigned FPOpToLibCall[] = {ISD::FREM};

  static const unsigned FPRndMode[] = {
      ISD::FCEIL, ISD::FFLOOR, ISD::FTRUNC, ISD::FRINT, ISD::FROUND,
      ISD::FROUNDEVEN};

  static const unsigned ZfhminZfbfminPromoteOps[] = {
      ISD::FMINNUM,      ISD::FMAXNUM,       ISD::FMAXIMUMNUM,
      ISD::FMINIMUMNUM,  ISD::FADD,          ISD::FSUB,
      ISD::FMUL,         ISD::FMA,           ISD::FDIV,
      ISD::FSQRT,        ISD::STRICT_FMA,    ISD::STRICT_FADD,
      ISD::STRICT_FSUB,  ISD::STRICT_FMUL,   ISD::STRICT_FDIV,
      ISD::STRICT_FSQRT, ISD::STRICT_FSETCC, ISD::STRICT_FSETCCS,
      ISD::SETCC,        ISD::FCEIL,         ISD::FFLOOR,
      ISD::FTRUNC,       ISD::FRINT,         ISD::FROUND,
      ISD::FROUNDEVEN,   ISD::FCANONICALIZE};

  if (Subtarget.enablePExtSIMDCodeGen()) {
    setTargetDAGCombine(ISD::TRUNCATE);
    setTruncStoreAction(MVT::v2i32, MVT::v2i16, Expand);
    setTruncStoreAction(MVT::v4i16, MVT::v4i8, Expand);
    static const MVT RV32VTs[] = {MVT::v2i16, MVT::v4i8};
    static const MVT RV64VTs[] = {MVT::v2i32, MVT::v4i16, MVT::v8i8};
    ArrayRef<MVT> VTs;
    if (Subtarget.is64Bit()) {
      VTs = RV64VTs;
      setTruncStoreAction(MVT::v2i64, MVT::v2i32, Expand);
      setTruncStoreAction(MVT::v4i32, MVT::v4i16, Expand);
      setTruncStoreAction(MVT::v8i16, MVT::v8i8, Expand);
      setTruncStoreAction(MVT::v2i32, MVT::v2i16, Expand);
      setTruncStoreAction(MVT::v4i16, MVT::v4i8, Expand);
    } else {
      VTs = RV32VTs;
      setOperationAction(ISD::BUILD_VECTOR, MVT::v4i8, Custom);
    }
    setOperationAction(ISD::UADDSAT, VTs, Legal);
    setOperationAction(ISD::SADDSAT, VTs, Legal);
    setOperationAction(ISD::USUBSAT, VTs, Legal);
    setOperationAction(ISD::SSUBSAT, VTs, Legal);
    setOperationAction(ISD::SSHLSAT, VTs, Legal);
    setOperationAction({ISD::AVGFLOORS, ISD::AVGFLOORU}, VTs, Legal);
    setOperationAction({ISD::ABDS, ISD::ABDU}, VTs, Legal);
    setOperationAction(ISD::SPLAT_VECTOR, VTs, Legal);
    setOperationAction({ISD::SHL, ISD::SRL, ISD::SRA}, VTs, Custom);
    setOperationAction(ISD::BITCAST, VTs, Custom);
    setOperationAction(ISD::EXTRACT_VECTOR_ELT, VTs, Custom);
    setOperationAction({ISD::SDIV, ISD::UDIV, ISD::SREM, ISD::UREM,
                        ISD::SDIVREM, ISD::UDIVREM},
                       VTs, Expand);
    setOperationAction({ISD::SMIN, ISD::UMIN, ISD::SMAX, ISD::UMAX}, VTs,
                       Legal);
    setOperationAction(ISD::SETCC, VTs, Legal);
    setCondCodeAction({ISD::SETNE, ISD::SETGT, ISD::SETGE, ISD::SETUGT,
                       ISD::SETUGE, ISD::SETULE, ISD::SETLE},
                      VTs, Expand);
    // P extension vector comparisons produce all 1s for true, all 0s for false
    setBooleanVectorContents(ZeroOrNegativeOneBooleanContent);
  }

  if (Subtarget.hasStdExtZfbfmin()) {
    setOperationAction(ISD::BITCAST, MVT::i16, Custom);
    setOperationAction(ISD::ConstantFP, MVT::bf16, Expand);
    setOperationAction(ISD::SELECT_CC, MVT::bf16, Expand);
    setOperationAction(ISD::SELECT, MVT::bf16, Custom);
    setOperationAction(ISD::BR_CC, MVT::bf16, Expand);
    setOperationAction(ZfhminZfbfminPromoteOps, MVT::bf16, Promote);
    setOperationAction(ISD::FREM, MVT::bf16, Promote);
    setOperationAction(ISD::FABS, MVT::bf16, Custom);
    setOperationAction(ISD::FNEG, MVT::bf16, Custom);
    setOperationAction(ISD::FCOPYSIGN, MVT::bf16, Custom);
    setOperationAction({ISD::FP_TO_SINT, ISD::FP_TO_UINT}, XLenVT, Custom);
    setOperationAction({ISD::SINT_TO_FP, ISD::UINT_TO_FP}, XLenVT, Custom);
  }

  if (Subtarget.hasStdExtZfhminOrZhinxmin()) {
    if (Subtarget.hasStdExtZfhOrZhinx()) {
      setOperationAction(FPLegalNodeTypes, MVT::f16, Legal);
      setOperationAction(FPRndMode, MVT::f16,
                         Subtarget.hasStdExtZfa() ? Legal : Custom);
      setOperationAction(ISD::IS_FPCLASS, MVT::f16, Custom);
      setOperationAction({ISD::FMAXIMUM, ISD::FMINIMUM}, MVT::f16,
                         Subtarget.hasStdExtZfa() ? Legal : Custom);
      if (Subtarget.hasStdExtZfa())
        setOperationAction(ISD::ConstantFP, MVT::f16, Custom);
    } else {
      setOperationAction(ZfhminZfbfminPromoteOps, MVT::f16, Promote);
      setOperationAction({ISD::FMAXIMUM, ISD::FMINIMUM}, MVT::f16, Promote);
      for (auto Op : {ISD::LROUND, ISD::LLROUND, ISD::LRINT, ISD::LLRINT,
                      ISD::STRICT_LROUND, ISD::STRICT_LLROUND,
                      ISD::STRICT_LRINT, ISD::STRICT_LLRINT})
        setOperationAction(Op, MVT::f16, Custom);
      setOperationAction(ISD::FABS, MVT::f16, Custom);
      setOperationAction(ISD::FNEG, MVT::f16, Custom);
      setOperationAction(ISD::FCOPYSIGN, MVT::f16, Custom);
      setOperationAction({ISD::FP_TO_SINT, ISD::FP_TO_UINT}, XLenVT, Custom);
      setOperationAction({ISD::SINT_TO_FP, ISD::UINT_TO_FP}, XLenVT, Custom);
    }

    if (!Subtarget.hasStdExtD()) {
      // FIXME: handle f16 fma when f64 is not legal. Using an f32 fma
      // instruction runs into double rounding issues, so this is wrong.
      // Normally we'd use an f64 fma, but without the D extension the f64 type
      // is not legal. This should probably be a libcall.
      AddPromotedToType(ISD::FMA, MVT::f16, MVT::f32);
      AddPromotedToType(ISD::STRICT_FMA, MVT::f16, MVT::f32);
    }

    setOperationAction(ISD::BITCAST, MVT::i16, Custom);

    setOperationAction(ISD::STRICT_FP_ROUND, MVT::f16, Legal);
    setOperationAction(ISD::STRICT_FP_EXTEND, MVT::f32, Legal);
    setCondCodeAction(FPCCToExpand, MVT::f16, Expand);
    setOperationAction(ISD::SELECT_CC, MVT::f16, Expand);
    setOperationAction(ISD::SELECT, MVT::f16, Custom);
    setOperationAction(ISD::BR_CC, MVT::f16, Expand);

    setOperationAction(
        ISD::FNEARBYINT, MVT::f16,
        Subtarget.hasStdExtZfh() && Subtarget.hasStdExtZfa() ? Legal : Promote);
    setOperationAction({ISD::FREM, ISD::FPOW, ISD::FPOWI,
                        ISD::FCOS, ISD::FSIN, ISD::FSINCOS, ISD::FEXP,
                        ISD::FEXP2, ISD::FEXP10, ISD::FLOG, ISD::FLOG2,
                        ISD::FLOG10, ISD::FLDEXP, ISD::FFREXP, ISD::FMODF},
                       MVT::f16, Promote);

    // FIXME: Need to promote f16 STRICT_* to f32 libcalls, but we don't have
    // complete support for all operations in LegalizeDAG.
    setOperationAction({ISD::STRICT_FCEIL, ISD::STRICT_FFLOOR,
                        ISD::STRICT_FNEARBYINT, ISD::STRICT_FRINT,
                        ISD::STRICT_FROUND, ISD::STRICT_FROUNDEVEN,
                        ISD::STRICT_FTRUNC, ISD::STRICT_FLDEXP},
                       MVT::f16, Promote);

    // We need to custom promote this.
    if (Subtarget.is64Bit())
      setOperationAction(ISD::FPOWI, MVT::i32, Custom);
  }

  if (Subtarget.hasStdExtFOrZfinx()) {
    setOperationAction(FPLegalNodeTypes, MVT::f32, Legal);
    setOperationAction(FPRndMode, MVT::f32,
                       Subtarget.hasStdExtZfa() ? Legal : Custom);
    setCondCodeAction(FPCCToExpand, MVT::f32, Expand);
    setOperationAction(ISD::SELECT_CC, MVT::f32, Expand);
    setOperationAction(ISD::SELECT, MVT::f32, Custom);
    setOperationAction(ISD::BR_CC, MVT::f32, Expand);
    setOperationAction(FPOpToExpand, MVT::f32, Expand);
    setOperationAction(FPOpToLibCall, MVT::f32, LibCall);
    setLoadExtAction(ISD::EXTLOAD, MVT::f32, MVT::f16, Expand);
    setTruncStoreAction(MVT::f32, MVT::f16, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::f32, MVT::bf16, Expand);
    setTruncStoreAction(MVT::f32, MVT::bf16, Expand);
    setOperationAction(ISD::IS_FPCLASS, MVT::f32, Custom);
    setOperationAction(ISD::BF16_TO_FP, MVT::f32, Custom);
    setOperationAction(ISD::FP_TO_BF16, MVT::f32,
                       Subtarget.isSoftFPABI() ? LibCall : Custom);
    setOperationAction(ISD::FP_TO_FP16, MVT::f32, Custom);
    setOperationAction(ISD::FP16_TO_FP, MVT::f32, Custom);
    setOperationAction(ISD::STRICT_FP_TO_FP16, MVT::f32, Custom);
    setOperationAction(ISD::STRICT_FP16_TO_FP, MVT::f32, Custom);

    if (Subtarget.hasStdExtZfa()) {
      setOperationAction(ISD::ConstantFP, MVT::f32, Custom);
      setOperationAction(ISD::FNEARBYINT, MVT::f32, Legal);
      setOperationAction({ISD::FMAXIMUM, ISD::FMINIMUM}, MVT::f32, Legal);
    } else {
      setOperationAction({ISD::FMAXIMUM, ISD::FMINIMUM}, MVT::f32, Custom);
    }
  }

  if (Subtarget.hasStdExtFOrZfinx() && Subtarget.is64Bit())
    setOperationAction(ISD::BITCAST, MVT::i32, Custom);

  if (Subtarget.hasStdExtDOrZdinx()) {
    setOperationAction(FPLegalNodeTypes, MVT::f64, Legal);

    if (!Subtarget.is64Bit())
      setOperationAction(ISD::BITCAST, MVT::i64, Custom);

    if (Subtarget.hasStdExtZdinx() && !Subtarget.hasStdExtZilsd() &&
        !Subtarget.is64Bit()) {
      setOperationAction(ISD::LOAD, MVT::f64, Custom);
      setOperationAction(ISD::STORE, MVT::f64, Custom);
    }

    if (Subtarget.hasStdExtZfa()) {
      setOperationAction(ISD::ConstantFP, MVT::f64, Custom);
      setOperationAction(FPRndMode, MVT::f64, Legal);
      setOperationAction(ISD::FNEARBYINT, MVT::f64, Legal);
      setOperationAction({ISD::FMAXIMUM, ISD::FMINIMUM}, MVT::f64, Legal);
    } else {
      if (Subtarget.is64Bit())
        setOperationAction(FPRndMode, MVT::f64, Custom);

      setOperationAction({ISD::FMAXIMUM, ISD::FMINIMUM}, MVT::f64, Custom);
    }

    setOperationAction(ISD::STRICT_FP_ROUND, MVT::f32, Legal);
    setOperationAction(ISD::STRICT_FP_EXTEND, MVT::f64, Legal);
    setCondCodeAction(FPCCToExpand, MVT::f64, Expand);
    setOperationAction(ISD::SELECT_CC, MVT::f64, Expand);
    setOperationAction(ISD::SELECT, MVT::f64, Custom);
    setOperationAction(ISD::BR_CC, MVT::f64, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::f64, MVT::f32, Expand);
    setTruncStoreAction(MVT::f64, MVT::f32, Expand);
    setOperationAction(FPOpToExpand, MVT::f64, Expand);
    setOperationAction(FPOpToLibCall, MVT::f64, LibCall);
    setLoadExtAction(ISD::EXTLOAD, MVT::f64, MVT::f16, Expand);
    setTruncStoreAction(MVT::f64, MVT::f16, Expand);
    setLoadExtAction(ISD::EXTLOAD, MVT::f64, MVT::bf16, Expand);
    setTruncStoreAction(MVT::f64, MVT::bf16, Expand);
    setOperationAction(ISD::IS_FPCLASS, MVT::f64, Custom);
    setOperationAction(ISD::BF16_TO_FP, MVT::f64, Custom);
    setOperationAction(ISD::FP_TO_BF16, MVT::f64,
                       Subtarget.isSoftFPABI() ? LibCall : Custom);
    setOperationAction(ISD::FP_TO_FP16, MVT::f64, Custom);
    setOperationAction(ISD::FP16_TO_FP, MVT::f64, Expand);
    setOperationAction(ISD::STRICT_FP_TO_FP16, MVT::f64, Custom);
    setOperationAction(ISD::STRICT_FP16_TO_FP, MVT::f64, Expand);
  }

  if (Subtarget.is64Bit()) {
    setOperationAction({ISD::FP_TO_UINT, ISD::FP_TO_SINT,
                        ISD::STRICT_FP_TO_UINT, ISD::STRICT_FP_TO_SINT},
                       MVT::i32, Custom);
    setOperationAction(ISD::LROUND, MVT::i32, Custom);
  }

  if (Subtarget.hasStdExtFOrZfinx()) {
    setOperationAction({ISD::FP_TO_UINT_SAT, ISD::FP_TO_SINT_SAT}, XLenVT,
                       Custom);

    // f16/bf16 require custom handling.
    setOperationAction({ISD::STRICT_FP_TO_UINT, ISD::STRICT_FP_TO_SINT}, XLenVT,
                       Custom);
    setOperationAction({ISD::STRICT_UINT_TO_FP, ISD::STRICT_SINT_TO_FP}, XLenVT,
                       Custom);

    setOperationAction(ISD::GET_ROUNDING, XLenVT, Custom);
    setOperationAction(ISD::SET_ROUNDING, MVT::Other, Custom);
    setOperationAction(ISD::GET_FPENV, XLenVT, Custom);
    setOperationAction(ISD::SET_FPENV, XLenVT, Custom);
    setOperationAction(ISD::RESET_FPENV, MVT::Other, Custom);
    setOperationAction(ISD::GET_FPMODE, XLenVT, Custom);
    setOperationAction(ISD::SET_FPMODE, XLenVT, Custom);
    setOperationAction(ISD::RESET_FPMODE, MVT::Other, Custom);
  }

  setOperationAction({ISD::GlobalAddress, ISD::BlockAddress, ISD::ConstantPool,
                      ISD::JumpTable},
                     XLenVT, Custom);

  setOperationAction(ISD::GlobalTLSAddress, XLenVT, Custom);

  if (Subtarget.is64Bit())
    setOperationAction(ISD::Constant, MVT::i64, Custom);

  setOperationAction(ISD::READCYCLECOUNTER, MVT::i64, Expand);
  setOperationAction(ISD::READSTEADYCOUNTER, MVT::i64, Expand);

  if (Subtarget.is64Bit()) {
    setOperationAction(ISD::INIT_TRAMPOLINE, MVT::Other, Custom);
    setOperationAction(ISD::ADJUST_TRAMPOLINE, MVT::Other, Custom);
  }

  setOperationAction({ISD::TRAP, ISD::DEBUGTRAP}, MVT::Other, Legal);
  setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::Other, Custom);
  if (Subtarget.is64Bit())
    setOperationAction(ISD::INTRINSIC_WO_CHAIN, MVT::i32, Custom);

  if (Subtarget.hasVendorXMIPSCBOP())
    setOperationAction(ISD::PREFETCH, MVT::Other, Custom);
  else if (Subtarget.hasStdExtZicbop())
    setOperationAction(ISD::PREFETCH, MVT::Other, Legal);

  if (Subtarget.hasStdExtZalrsc()) {
    setMaxAtomicSizeInBitsSupported(Subtarget.getXLen());
    if (Subtarget.hasStdExtZabha() && Subtarget.hasStdExtZacas())
      setMinCmpXchgSizeInBits(8);
    else
      setMinCmpXchgSizeInBits(32);
  } else if (Subtarget.hasForcedAtomics()) {
    setMaxAtomicSizeInBitsSupported(Subtarget.getXLen());
  } else {
    setMaxAtomicSizeInBitsSupported(0);
  }

  setOperationAction(ISD::ATOMIC_FENCE, MVT::Other, Custom);

  setBooleanContents(ZeroOrOneBooleanContent);

  if (getTargetMachine().getTargetTriple().isOSLinux()) {
    // Custom lowering of llvm.clear_cache.
    setOperationAction(ISD::CLEAR_CACHE, MVT::Other, Custom);
  }


  if (Subtarget.hasStdExtZaamo())
    setOperationAction(ISD::ATOMIC_LOAD_SUB, XLenVT, Expand);

  if (Subtarget.hasForcedAtomics()) {
    // Force __sync libcalls to be emitted for atomic rmw/cas operations.
    setOperationAction(
        {ISD::ATOMIC_CMP_SWAP, ISD::ATOMIC_SWAP, ISD::ATOMIC_LOAD_ADD,
         ISD::ATOMIC_LOAD_SUB, ISD::ATOMIC_LOAD_AND, ISD::ATOMIC_LOAD_OR,
         ISD::ATOMIC_LOAD_XOR, ISD::ATOMIC_LOAD_NAND, ISD::ATOMIC_LOAD_MIN,
         ISD::ATOMIC_LOAD_MAX, ISD::ATOMIC_LOAD_UMIN, ISD::ATOMIC_LOAD_UMAX},
        XLenVT, LibCall);
  }

  if (Subtarget.hasVendorXRemovedTHeadMemIdx()) {
    for (unsigned im : {ISD::PRE_INC, ISD::POST_INC}) {
      setIndexedLoadAction(im, MVT::i8, Legal);
      setIndexedStoreAction(im, MVT::i8, Legal);
      setIndexedLoadAction(im, MVT::i16, Legal);
      setIndexedStoreAction(im, MVT::i16, Legal);
      setIndexedLoadAction(im, MVT::i32, Legal);
      setIndexedStoreAction(im, MVT::i32, Legal);

      if (Subtarget.is64Bit()) {
        setIndexedLoadAction(im, MVT::i64, Legal);
        setIndexedStoreAction(im, MVT::i64, Legal);
      }
    }
  }

  if (Subtarget.hasVendorXCVmem() && !Subtarget.is64Bit()) {
    setIndexedLoadAction(ISD::POST_INC, MVT::i8, Legal);
    setIndexedLoadAction(ISD::POST_INC, MVT::i16, Legal);
    setIndexedLoadAction(ISD::POST_INC, MVT::i32, Legal);

    setIndexedStoreAction(ISD::POST_INC, MVT::i8, Legal);
    setIndexedStoreAction(ISD::POST_INC, MVT::i16, Legal);
    setIndexedStoreAction(ISD::POST_INC, MVT::i32, Legal);
  }

  // Function alignments.
  const Align FunctionAlignment(Subtarget.hasStdExtZca() ? 2 : 4);
  setMinFunctionAlignment(FunctionAlignment);
  // Set preferred alignments.
  setPrefFunctionAlignment(Subtarget.getPrefFunctionAlignment());
  setPrefLoopAlignment(Subtarget.getPrefLoopAlignment());

  setTargetDAGCombine({ISD::INTRINSIC_VOID, ISD::INTRINSIC_W_CHAIN,
                       ISD::INTRINSIC_WO_CHAIN, ISD::ADD, ISD::SUB, ISD::MUL,
                       ISD::AND, ISD::OR, ISD::XOR, ISD::SETCC, ISD::SELECT});
  setTargetDAGCombine(ISD::SRA);
  setTargetDAGCombine(ISD::SIGN_EXTEND_INREG);

  if (Subtarget.hasStdExtFOrZfinx())
    setTargetDAGCombine({ISD::FADD, ISD::FMAXNUM, ISD::FMINNUM, ISD::FMUL});

  if (Subtarget.hasStdExtZbb())
    setTargetDAGCombine({ISD::UMAX, ISD::UMIN, ISD::SMAX, ISD::SMIN});

  if ((Subtarget.hasStdExtZbs() && Subtarget.is64Bit()) ||
      Subtarget.hasVInstructions())
    setTargetDAGCombine(ISD::TRUNCATE);

  if (Subtarget.hasStdExtZbkb())
    setTargetDAGCombine(ISD::BITREVERSE);

  if (Subtarget.hasStdExtFOrZfinx())
    setTargetDAGCombine({ISD::ZERO_EXTEND, ISD::FP_TO_SINT, ISD::FP_TO_UINT,
                         ISD::FP_TO_SINT_SAT, ISD::FP_TO_UINT_SAT});
  if (Subtarget.hasVInstructions())
    setTargetDAGCombine(
        {ISD::FCOPYSIGN,    ISD::MGATHER,      ISD::MSCATTER,
         ISD::VP_GATHER,    ISD::VP_SCATTER,   ISD::SRA,
         ISD::SRL,          ISD::SHL,          ISD::STORE,
         ISD::SPLAT_VECTOR, ISD::BUILD_VECTOR, ISD::CONCAT_VECTORS,
         ISD::VP_STORE,     ISD::VP_TRUNCATE,  ISD::EXPERIMENTAL_VP_REVERSE,
         ISD::MUL,          ISD::SDIV,         ISD::UDIV,
         ISD::SREM,         ISD::UREM,         ISD::INSERT_VECTOR_ELT,
         ISD::ABS,          ISD::CTPOP,        ISD::VECTOR_SHUFFLE,
         ISD::FMA,          ISD::VSELECT,      ISD::VECREDUCE_ADD});

  if (Subtarget.hasVendorXRemovedTHeadMemPair())
    setTargetDAGCombine({ISD::LOAD, ISD::STORE});
  if (Subtarget.useYSXVecForFixedLengthVectors())
    setTargetDAGCombine(ISD::BITCAST);

  setMaxDivRemBitWidthSupported(Subtarget.is64Bit() ? 128 : 64);

  // Disable strict node mutation.
  IsStrictFPEnabled = true;
  EnableExtLdPromotion = true;

  // Let the subtarget decide if a predictable select is more expensive than the
  // corresponding branch. This information is used in CGP/SelectOpt to decide
  // when to convert selects into branches.
  PredictableSelectIsExpensive = Subtarget.predictableSelectIsExpensive();

  MaxStoresPerMemsetOptSize = Subtarget.getMaxStoresPerMemset(/*OptSize=*/true);
  MaxStoresPerMemset = Subtarget.getMaxStoresPerMemset(/*OptSize=*/false);

  MaxGluedStoresPerMemcpy = Subtarget.getMaxGluedStoresPerMemcpy();
  MaxStoresPerMemcpyOptSize = Subtarget.getMaxStoresPerMemcpy(/*OptSize=*/true);
  MaxStoresPerMemcpy = Subtarget.getMaxStoresPerMemcpy(/*OptSize=*/false);

  MaxStoresPerMemmoveOptSize =
      Subtarget.getMaxStoresPerMemmove(/*OptSize=*/true);
  MaxStoresPerMemmove = Subtarget.getMaxStoresPerMemmove(/*OptSize=*/false);

  MaxLoadsPerMemcmpOptSize = Subtarget.getMaxLoadsPerMemcmp(/*OptSize=*/true);
  MaxLoadsPerMemcmp = Subtarget.getMaxLoadsPerMemcmp(/*OptSize=*/false);
}

TargetLoweringBase::LegalizeTypeAction
YSXTargetLowering::getPreferredVectorAction(MVT VT) const {
  if (Subtarget.is64Bit() && Subtarget.enablePExtSIMDCodeGen())
    if (VT == MVT::v2i16 || VT == MVT::v4i8)
      return TypeWidenVector;

  return TargetLoweringBase::getPreferredVectorAction(VT);
}

EVT YSXTargetLowering::getSetCCResultType(const DataLayout &DL,
                                            LLVMContext &Context,
                                            EVT VT) const {
  if (!VT.isVector())
    return getPointerTy(DL);
  if (Subtarget.hasVInstructions() &&
      (VT.isScalableVector() || Subtarget.useYSXVecForFixedLengthVectors()))
    return EVT::getVectorVT(Context, MVT::i1, VT.getVectorElementCount());
  return VT.changeVectorElementTypeToInteger();
}

MVT YSXTargetLowering::getVPExplicitVectorLengthTy() const {
  return Subtarget.getXLenVT();
}

bool YSXTargetLowering::shouldExpandGetVectorLength(EVT TripCountVT,
                                                      unsigned VF,
                                                      bool IsScalable) const {
  return true;
}

bool YSXTargetLowering::shouldExpandCttzElements(EVT VT) const {
  return true;
}

bool YSXTargetLowering::getTgtMemIntrinsic(IntrinsicInfo &Info,
                                             const CallBase &I,
                                             MachineFunction &MF,
                                             unsigned Intrinsic) const {
  if (I.hasMetadata(LLVMContext::MD_nontemporal))
    Info.flags |= MachineMemOperand::MONonTemporal;

  Info.flags |= YSXTargetLowering::getTargetMMOFlags(I);
  switch (Intrinsic) {
  default:
    return false;
  case Intrinsic::riscv_masked_atomicrmw_xchg:
  case Intrinsic::riscv_masked_atomicrmw_add:
  case Intrinsic::riscv_masked_atomicrmw_sub:
  case Intrinsic::riscv_masked_atomicrmw_nand:
  case Intrinsic::riscv_masked_atomicrmw_max:
  case Intrinsic::riscv_masked_atomicrmw_min:
  case Intrinsic::riscv_masked_atomicrmw_umax:
  case Intrinsic::riscv_masked_atomicrmw_umin:
  case Intrinsic::riscv_masked_cmpxchg:
    // ysx_masked_{atomicrmw_*,cmpxchg} intrinsics represent an emulated
    // narrow atomic operation. These will be expanded to an LR/SC loop that
    // reads/writes to/from an aligned 4 byte location. And, or, shift, etc.
    // will be used to modify the appropriate part of the 4 byte data and
    // preserve the rest.
    Info.opc = ISD::INTRINSIC_W_CHAIN;
    Info.memVT = MVT::i32;
    Info.ptrVal = I.getArgOperand(0);
    Info.offset = 0;
    Info.align = Align(4);
    Info.flags = MachineMemOperand::MOLoad | MachineMemOperand::MOStore |
                 MachineMemOperand::MOVolatile;
    return true;
  }
}

bool YSXTargetLowering::isLegalAddressingMode(const DataLayout &DL,
                                                const AddrMode &AM, Type *Ty,
                                                unsigned AS,
                                                Instruction *I) const {
  // No global is ever allowed as a base.
  if (AM.BaseGV)
    return false;

  // None of our addressing modes allows a scalable offset
  if (AM.ScalableOffset)
    return false;

  // YSXVec instructions only support register addressing.
  if (Subtarget.hasVInstructions() && isa<VectorType>(Ty))
    return AM.HasBaseReg && AM.Scale == 0 && !AM.BaseOffs;

  // Require a 12-bit signed offset.
  if (!isInt<12>(AM.BaseOffs))
    return false;

  switch (AM.Scale) {
  case 0: // "r+i" or just "i", depending on HasBaseReg.
    break;
  case 1:
    if (!AM.HasBaseReg) // allow "r+i".
      break;
    return false; // disallow "r+r" or "r+r+i".
  default:
    return false;
  }

  return true;
}

bool YSXTargetLowering::isLegalICmpImmediate(int64_t Imm) const {
  return isInt<12>(Imm);
}

bool YSXTargetLowering::isLegalAddImmediate(int64_t Imm) const {
  return isInt<12>(Imm);
}

// On RV32, 64-bit integers are split into their high and low parts and held
// in two different registers, so the trunc is free since the low register can
// just be used.
// FIXME: Should we consider i64->i32 free on RV64 to match the EVT version of
// isTruncateFree?
bool YSXTargetLowering::isTruncateFree(Type *SrcTy, Type *DstTy) const {
  if (Subtarget.is64Bit() || !SrcTy->isIntegerTy() || !DstTy->isIntegerTy())
    return false;
  unsigned SrcBits = SrcTy->getPrimitiveSizeInBits();
  unsigned DestBits = DstTy->getPrimitiveSizeInBits();
  return (SrcBits == 64 && DestBits == 32);
}

bool YSXTargetLowering::isTruncateFree(EVT SrcVT, EVT DstVT) const {
  // We consider i64->i32 free on RV64 since we have good selection of W
  // instructions that make promoting operations back to i64 free in many cases.
  if (SrcVT.isVector() || DstVT.isVector() || !SrcVT.isInteger() ||
      !DstVT.isInteger())
    return false;
  unsigned SrcBits = SrcVT.getSizeInBits();
  unsigned DestBits = DstVT.getSizeInBits();
  return (SrcBits == 64 && DestBits == 32);
}

bool YSXTargetLowering::isTruncateFree(SDValue Val, EVT VT2) const {
  EVT SrcVT = Val.getValueType();
  // free truncate from vnsrl and vnsra
  if (Subtarget.hasVInstructions() &&
      (Val.getOpcode() == ISD::SRL || Val.getOpcode() == ISD::SRA) &&
      SrcVT.isVector() && VT2.isVector()) {
    unsigned SrcBits = SrcVT.getVectorElementType().getSizeInBits();
    unsigned DestBits = VT2.getVectorElementType().getSizeInBits();
    if (SrcBits == DestBits * 2) {
      return true;
    }
  }
  return TargetLowering::isTruncateFree(Val, VT2);
}

bool YSXTargetLowering::isZExtFree(SDValue Val, EVT VT2) const {
  // Zexts are free if they can be combined with a load.
  // Don't advertise i32->i64 zextload as being free for RV64. It interacts
  // poorly with type legalization of compares preferring sext.
  if (auto *LD = dyn_cast<LoadSDNode>(Val)) {
    EVT MemVT = LD->getMemoryVT();
    if ((MemVT == MVT::i8 || MemVT == MVT::i16) &&
        (LD->getExtensionType() == ISD::NON_EXTLOAD ||
         LD->getExtensionType() == ISD::ZEXTLOAD))
      return true;
  }

  return TargetLowering::isZExtFree(Val, VT2);
}

bool YSXTargetLowering::isSExtCheaperThanZExt(EVT SrcVT, EVT DstVT) const {
  return Subtarget.is64Bit() && SrcVT == MVT::i32 && DstVT == MVT::i64;
}

bool YSXTargetLowering::signExtendConstant(const ConstantInt *CI) const {
  return Subtarget.is64Bit() && CI->getType()->isIntegerTy(32);
}

bool YSXTargetLowering::isCheapToSpeculateCttz(Type *Ty) const {
  return Subtarget.hasCTZLike();
}

bool YSXTargetLowering::isCheapToSpeculateCtlz(Type *Ty) const {
  return Subtarget.hasCLZLike();
}

bool YSXTargetLowering::isMaskAndCmp0FoldingBeneficial(
    const Instruction &AndI) const {
  // We expect to be able to match a bit extraction instruction if the Zbs
  // extension is supported and the mask is a power of two. However, we
  // conservatively return false if the mask would fit in an ANDI instruction,
  // on the basis that it's possible the sinking+duplication of the AND in
  // CodeGenPrepare triggered by this hook wouldn't decrease the instruction
  // count and would increase code size (e.g. ANDI+BNEZ => BEXTI+BNEZ).
  if (!Subtarget.hasBEXTILike())
    return false;
  ConstantInt *Mask = dyn_cast<ConstantInt>(AndI.getOperand(1));
  if (!Mask)
    return false;
  return !Mask->getValue().isSignedIntN(12) && Mask->getValue().isPowerOf2();
}

bool YSXTargetLowering::hasAndNotCompare(SDValue Y) const {
  EVT VT = Y.getValueType();

  if (VT.isVector())
    return false;

  return (Subtarget.hasStdExtZbb() || Subtarget.hasStdExtZbkb()) &&
         (!isa<ConstantSDNode>(Y) || cast<ConstantSDNode>(Y)->isOpaque());
}

bool YSXTargetLowering::hasAndNot(SDValue Y) const {
  EVT VT = Y.getValueType();

  if (!VT.isVector())
    return hasAndNotCompare(Y);

  return Subtarget.hasStdExtZvkb();
}

bool YSXTargetLowering::hasBitTest(SDValue X, SDValue Y) const {
  // Zbs provides BEXT[_I], which can be used with SEQZ/SNEZ as a bit test.
  if (Subtarget.hasStdExtZbs())
    return X.getValueType().isScalarInteger();
  auto *C = dyn_cast<ConstantSDNode>(Y);
  // XTheadBs provides th.tst (similar to bexti), if Y is a constant
  if (Subtarget.hasVendorXRemovedTHeadBs())
    return C != nullptr;
  // We can use ANDI+SEQZ/SNEZ as a bit test. Y contains the bit position.
  return C && C->getAPIntValue().ule(10);
}

bool YSXTargetLowering::shouldFoldSelectWithIdentityConstant(
    unsigned BinOpcode, EVT VT, unsigned SelectOpcode, SDValue X,
    SDValue Y) const {
  if (SelectOpcode != ISD::VSELECT)
    return false;

  // Only enable for rvv.
  if (!VT.isVector() || !Subtarget.hasVInstructions())
    return false;

  if (VT.isFixedLengthVector() && !isTypeLegal(VT))
    return false;

  return true;
}

bool YSXTargetLowering::shouldConvertConstantLoadToIntImm(const APInt &Imm,
                                                            Type *Ty) const {
  assert(Ty->isIntegerTy());

  unsigned BitSize = Ty->getIntegerBitWidth();
  if (BitSize > Subtarget.getXLen())
    return false;

  // Fast path, assume 32-bit immediates are cheap.
  int64_t Val = Imm.getSExtValue();
  if (isInt<32>(Val))
    return true;

  // A constant pool entry may be more aligned than the load we're trying to
  // replace. If we don't support unaligned scalar mem, prefer the constant
  // pool.
  // TODO: Can the caller pass down the alignment?
  if (!Subtarget.enableUnalignedScalarMem())
    return true;

  // Prefer to keep the load if it would require many instructions.
  // This uses the same threshold we use for constant pools but doesn't
  // check useConstantPoolForLargeInts.
  // TODO: Should we keep the load only when we're definitely going to emit a
  // constant pool?

  YSXMatInt::InstSeq Seq = YSXMatInt::generateInstSeq(Val, Subtarget);
  return Seq.size() <= Subtarget.getMaxBuildIntsCost();
}

bool YSXTargetLowering::
    shouldProduceAndByConstByHoistingConstFromShiftsLHSOfAnd(
        SDValue X, ConstantSDNode *XC, ConstantSDNode *CC, SDValue Y,
        unsigned OldShiftOpcode, unsigned NewShiftOpcode,
        SelectionDAG &DAG) const {
  // One interesting pattern that we'd want to form is 'bit extract':
  //   ((1 >> Y) & 1) ==/!= 0
  // But we also need to be careful not to try to reverse that fold.

  // Is this '((1 >> Y) & 1)'?
  if (XC && OldShiftOpcode == ISD::SRL && XC->isOne())
    return false; // Keep the 'bit extract' pattern.

  // Will this be '((1 >> Y) & 1)' after the transform?
  if (NewShiftOpcode == ISD::SRL && CC->isOne())
    return true; // Do form the 'bit extract' pattern.

  // If 'X' is a constant, and we transform, then we will immediately
  // try to undo the fold, thus causing endless combine loop.
  // So only do the transform if X is not a constant. This matches the default
  // implementation of this function.
  return !XC;
}

bool YSXTargetLowering::shouldScalarizeBinop(SDValue VecOp) const {
  unsigned Opc = VecOp.getOpcode();

  // Assume target opcodes can't be scalarized.
  // TODO - do we have any exceptions?
  if (Opc >= ISD::BUILTIN_OP_END || !isBinOp(Opc))
    return false;

  // If the vector op is not supported, try to convert to scalar.
  EVT VecVT = VecOp.getValueType();
  if (!isOperationLegalOrCustomOrPromote(Opc, VecVT))
    return true;

  // If the vector op is supported, but the scalar op is not, the transform may
  // not be worthwhile.
  // Permit a vector binary operation can be converted to scalar binary
  // operation which is custom lowered with illegal type.
  EVT ScalarVT = VecVT.getScalarType();
  return isOperationLegalOrCustomOrPromote(Opc, ScalarVT) ||
         isOperationCustom(Opc, ScalarVT);
}

bool YSXTargetLowering::isOffsetFoldingLegal(
    const GlobalAddressSDNode *GA) const {
  // In order to maximise the opportunity for common subexpression elimination,
  // keep a separate ADD node for the global address offset instead of folding
  // it in the global address node. Later peephole optimisations may choose to
  // fold it back in when profitable.
  return false;
}

// Returns 0-31 if the fli instruction is available for the type and this is
// legal FP immediate for the type. Returns -1 otherwise.
int YSXTargetLowering::getLegalZfaFPImm(const APFloat &Imm, EVT VT) const {
  if (!Subtarget.hasStdExtZfa())
    return -1;

  bool IsSupportedVT = false;
  if (VT == MVT::f16) {
    IsSupportedVT = Subtarget.hasStdExtZfh() || Subtarget.hasStdExtZvfh();
  } else if (VT == MVT::f32) {
    IsSupportedVT = true;
  } else if (VT == MVT::f64) {
    assert(Subtarget.hasStdExtD() && "Expect D extension");
    IsSupportedVT = true;
  }

  if (!IsSupportedVT)
    return -1;

  return YSXLoadFPImm::getLoadFPImm(Imm);
}

bool YSXTargetLowering::isFPImmLegal(const APFloat &Imm, EVT VT,
                                       bool ForCodeSize) const {
  bool IsLegalVT = false;
  if (VT == MVT::f16)
    IsLegalVT = Subtarget.hasStdExtZfhminOrZhinxmin();
  else if (VT == MVT::f32)
    IsLegalVT = Subtarget.hasStdExtFOrZfinx();
  else if (VT == MVT::f64)
    IsLegalVT = Subtarget.hasStdExtDOrZdinx();
  else if (VT == MVT::bf16)
    IsLegalVT = Subtarget.hasStdExtZfbfmin();

  if (!IsLegalVT)
    return false;

  if (getLegalZfaFPImm(Imm, VT) >= 0)
    return true;

  // Some constants can be produced by fli+fneg.
  if (Imm.isNegative() && getLegalZfaFPImm(-Imm, VT) >= 0)
    return true;

  // Cannot create a 64 bit floating-point immediate value for rv32.
  if (Subtarget.getXLen() < VT.getScalarSizeInBits()) {
    // td can handle +0.0 or -0.0 already.
    // -0.0 can be created by fmv + fneg.
    return Imm.isZero();
  }

  // Special case: fmv + fneg
  if (Imm.isNegZero())
    return true;

  // Building an integer and then converting requires a fmv at the end of
  // the integer sequence. The fmv is not required for Zfinx.
  const int FmvCost = Subtarget.hasStdExtZfinx() ? 0 : 1;
  const int Cost =
      FmvCost + YSXMatInt::getIntMatCost(Imm.bitcastToAPInt(),
                                           Subtarget.getXLen(), Subtarget);
  return Cost <= FPImmCost;
}

// TODO: This is very conservative.
bool YSXTargetLowering::isExtractSubvectorCheap(EVT ResVT, EVT SrcVT,
                                                  unsigned Index) const {
  if (!isOperationLegalOrCustom(ISD::EXTRACT_SUBVECTOR, ResVT))
    return false;

  // Extracts from index 0 are just subreg extracts.
  if (Index == 0)
    return true;

  // Only support extracting a fixed from a fixed vector for now.
  if (ResVT.isScalableVector() || SrcVT.isScalableVector())
    return false;

  EVT EltVT = ResVT.getVectorElementType();
  assert(EltVT == SrcVT.getVectorElementType() && "Should hold for node");

  // The smallest type we can slide is i8.
  // TODO: We can extract index 0 from a mask vector without a slide.
  if (EltVT == MVT::i1)
    return false;

  unsigned ResElts = ResVT.getVectorNumElements();
  unsigned SrcElts = SrcVT.getVectorNumElements();

  unsigned MinVLen = Subtarget.getRealMinVLen();
  unsigned MinVLMAX = MinVLen / EltVT.getSizeInBits();

  // If we're extracting only data from the first VLEN bits of the source
  // then we can always do this with an m1 vslidedown.vx.  Restricting the
  // Index ensures we can use a vslidedown.vi.
  // TODO: We can generalize this when the exact VLEN is known.
  if (Index + ResElts <= MinVLMAX && Index < 31)
    return true;

  // Convervatively only handle extracting half of a vector.
  // TODO: We can do arbitrary slidedowns, but for now only support extracting
  // the upper half of a vector until we have more test coverage.
  // TODO: For sizes which aren't multiples of VLEN sizes, this may not be
  // a cheap extract.  However, this case is important in practice for
  // shuffled extracts of longer vectors.  How resolve?
  return (ResElts * 2) == SrcElts && Index == ResElts;
}

MVT YSXTargetLowering::getRegisterTypeForCallingConv(LLVMContext &Context,
                                                      CallingConv::ID CC,
                                                      EVT VT) const {
  // Use f32 to pass f16 if it is legal and Zfh/Zfhmin is not enabled.
  // We might still end up using a GPR but that will be decided based on ABI.
  if (VT == MVT::f16 && Subtarget.hasStdExtFOrZfinx() &&
      !Subtarget.hasStdExtZfhminOrZhinxmin())
    return MVT::f32;

  return TargetLowering::getRegisterTypeForCallingConv(Context, CC, VT);
}

unsigned
YSXTargetLowering::getNumRegisters(LLVMContext &Context, EVT VT,
                                     std::optional<MVT> RegisterVT) const {
  // Pair inline assembly operand
  if (VT == (Subtarget.is64Bit() ? MVT::i128 : MVT::i64) && RegisterVT &&
      *RegisterVT == MVT::Untyped)
    return 1;

  return TargetLowering::getNumRegisters(Context, VT, RegisterVT);
}

unsigned YSXTargetLowering::getNumRegistersForCallingConv(LLVMContext &Context,
                                                           CallingConv::ID CC,
                                                           EVT VT) const {
  // Use f32 to pass f16 if it is legal and Zfh/Zfhmin is not enabled.
  // We might still end up using a GPR but that will be decided based on ABI.
  if (VT == MVT::f16 && Subtarget.hasStdExtFOrZfinx() &&
      !Subtarget.hasStdExtZfhminOrZhinxmin())
    return 1;

  return TargetLowering::getNumRegistersForCallingConv(Context, CC, VT);
}

// Changes the condition code and swaps operands if necessary, so the SetCC
// operation matches one of the comparisons supported directly by branches
// in the RISC-V ISA. May adjust compares to favor compare with 0 over compare
// with 1/-1.
static void translateSetCCForBranch(const SDLoc &DL, SDValue &LHS, SDValue &RHS,
                                    ISD::CondCode &CC, SelectionDAG &DAG,
                                    const YSXSubtarget &Subtarget) {
  // If this is a single bit test that can't be handled by ANDI, shift the
  // bit to be tested to the MSB and perform a signed compare with 0.
  if (isIntEqualitySetCC(CC) && isNullConstant(RHS) &&
      LHS.getOpcode() == ISD::AND && LHS.hasOneUse() &&
      isa<ConstantSDNode>(LHS.getOperand(1)) &&
      // XAndesPerf supports branch on test bit.
      !Subtarget.hasVendorXAndesPerf()) {
    uint64_t Mask = LHS.getConstantOperandVal(1);
    if ((isPowerOf2_64(Mask) || isMask_64(Mask)) && !isInt<12>(Mask)) {
      unsigned ShAmt = 0;
      if (isPowerOf2_64(Mask)) {
        CC = CC == ISD::SETEQ ? ISD::SETGE : ISD::SETLT;
        ShAmt = LHS.getValueSizeInBits() - 1 - Log2_64(Mask);
      } else {
        ShAmt = LHS.getValueSizeInBits() - llvm::bit_width(Mask);
      }

      LHS = LHS.getOperand(0);
      if (ShAmt != 0)
        LHS = DAG.getNode(ISD::SHL, DL, LHS.getValueType(), LHS,
                          DAG.getConstant(ShAmt, DL, LHS.getValueType()));
      return;
    }
  }

  if (auto *RHSC = dyn_cast<ConstantSDNode>(RHS)) {
    int64_t C = RHSC->getSExtValue();
    switch (CC) {
    default: break;
    case ISD::SETGT:
      // Convert X > -1 to X >= 0.
      if (C == -1) {
        RHS = DAG.getConstant(0, DL, RHS.getValueType());
        CC = ISD::SETGE;
        return;
      }
      if ((Subtarget.hasVendorXRemovedQcicm() || Subtarget.hasVendorXRemovedQcicli()) &&
          C != INT64_MAX && isInt<5>(C + 1)) {
        // We have a conditional move instruction for SETGE but not SETGT.
        // Convert X > C to X >= C + 1, if (C + 1) is a 5-bit signed immediate.
        RHS = DAG.getSignedConstant(C + 1, DL, RHS.getValueType());
        CC = ISD::SETGE;
        return;
      }
      if (Subtarget.hasVendorXRemovedQcibi() && C != INT64_MAX && isInt<16>(C + 1)) {
        // We have a branch immediate instruction for SETGE but not SETGT.
        // Convert X > C to X >= C + 1, if (C + 1) is a 16-bit signed immediate.
        RHS = DAG.getSignedConstant(C + 1, DL, RHS.getValueType());
        CC = ISD::SETGE;
        return;
      }
      break;
    case ISD::SETLT:
      // Convert X < 1 to 0 >= X.
      if (C == 1) {
        RHS = LHS;
        LHS = DAG.getConstant(0, DL, RHS.getValueType());
        CC = ISD::SETGE;
        return;
      }
      break;
    case ISD::SETUGT:
      if ((Subtarget.hasVendorXRemovedQcicm() || Subtarget.hasVendorXRemovedQcicli()) &&
          C != INT64_MAX && isUInt<5>(C + 1)) {
        // We have a conditional move instruction for SETUGE but not SETUGT.
        // Convert X > C to X >= C + 1, if (C + 1) is a 5-bit signed immediate.
        RHS = DAG.getConstant(C + 1, DL, RHS.getValueType());
        CC = ISD::SETUGE;
        return;
      }
      if (Subtarget.hasVendorXRemovedQcibi() && C != INT64_MAX && isUInt<16>(C + 1)) {
        // We have a branch immediate instruction for SETUGE but not SETUGT.
        // Convert X > C to X >= C + 1, if (C + 1) is a 16-bit unsigned
        // immediate.
        RHS = DAG.getConstant(C + 1, DL, RHS.getValueType());
        CC = ISD::SETUGE;
        return;
      }
      break;
    }
  }

  switch (CC) {
  default:
    break;
  case ISD::SETGT:
  case ISD::SETLE:
  case ISD::SETUGT:
  case ISD::SETULE:
    CC = ISD::getSetCCSwappedOperands(CC);
    std::swap(LHS, RHS);
    break;
  }
}

bool YSXTargetLowering::mergeStoresAfterLegalization(EVT VT) const {
  return true;
}


unsigned YSXTargetLowering::combineRepeatedFPDivisors() const {
  return NumRepeatedDivisors;
}

bool YSXTargetLowering::shouldExpandBuildVectorWithShuffles(
    EVT VT, unsigned DefinedValues) const {
  return false;
}

bool YSXTargetLowering::isShuffleMaskLegal(ArrayRef<int> M, EVT VT) const {
  return false;
}

static SDValue lowerConstant(SDValue Op, SelectionDAG &DAG,
                             const YSXSubtarget &Subtarget) {
  assert(Op.getValueType() == MVT::i64 && "Unexpected VT");

  int64_t Imm = cast<ConstantSDNode>(Op)->getSExtValue();

  // All simm32 constants should be handled by isel.
  // NOTE: The getMaxBuildIntsCost call below should return a value >= 2 making
  // this check redundant, but small immediates are common so this check
  // should have better compile time.
  if (isInt<32>(Imm))
    return Op;

  // We only need to cost the immediate, if constant pool lowering is enabled.
  if (!Subtarget.useConstantPoolForLargeInts())
    return Op;

  YSXMatInt::InstSeq Seq = YSXMatInt::generateInstSeq(Imm, Subtarget);
  if (Seq.size() <= Subtarget.getMaxBuildIntsCost())
    return Op;

  // Optimizations below are disabled for opt size. If we're optimizing for
  // size, use a constant pool.
  if (DAG.shouldOptForSize())
    return SDValue();

  // Special case. See if we can build the constant as (ADD (SLLI X, C), X) do
  // that if it will avoid a constant pool.
  // It will require an extra temporary register though.
  // If we have Zba we can use (ADD_UW X, (SLLI X, 32)) to handle cases where
  // low and high 32 bits are the same and bit 31 and 63 are set.
  unsigned ShiftAmt, AddOpc;
  YSXMatInt::InstSeq SeqLo =
      YSXMatInt::generateTwoRegInstSeq(Imm, Subtarget, ShiftAmt, AddOpc);
  if (!SeqLo.empty() && (SeqLo.size() + 2) <= Subtarget.getMaxBuildIntsCost())
    return Op;

  return SDValue();
}

SDValue YSXTargetLowering::lowerConstantFP(SDValue Op,
                                                 SelectionDAG &DAG) const {
  return SDValue();
}

static SDValue LowerPREFETCH(SDValue Op, const YSXSubtarget &Subtarget,
                             SelectionDAG &DAG) {

  unsigned IsData = Op.getConstantOperandVal(4);

  // mips-p8700  we support data prefetch for now.
  if (Subtarget.hasVendorXMIPSCBOP() && !IsData)
    return Op.getOperand(0);
  return Op;
}

static SDValue LowerATOMIC_FENCE(SDValue Op, SelectionDAG &DAG,
                                 const YSXSubtarget &Subtarget) {
  SDLoc dl(Op);
  AtomicOrdering FenceOrdering =
      static_cast<AtomicOrdering>(Op.getConstantOperandVal(1));
  SyncScope::ID FenceSSID =
      static_cast<SyncScope::ID>(Op.getConstantOperandVal(2));

  if (Subtarget.hasStdExtZtso()) {
    // The only fence that needs an instruction is a sequentially-consistent
    // cross-thread fence.
    if (FenceOrdering == AtomicOrdering::SequentiallyConsistent &&
        FenceSSID == SyncScope::System)
      return Op;

    // MEMBARRIER is a compiler barrier; it codegens to a no-op.
    return DAG.getNode(ISD::MEMBARRIER, dl, MVT::Other, Op.getOperand(0));
  }

  // singlethread fences only synchronize with signal handlers on the same
  // thread and thus only need to preserve instruction order, not actually
  // enforce memory ordering.
  if (FenceSSID == SyncScope::SingleThread)
    // MEMBARRIER is a compiler barrier; it codegens to a no-op.
    return DAG.getNode(ISD::MEMBARRIER, dl, MVT::Other, Op.getOperand(0));

  return Op;
}

SDValue YSXTargetLowering::LowerOperation(SDValue Op,
                                                SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  default:
    return SDValue();
  case ISD::PREFETCH:
    return LowerPREFETCH(Op, Subtarget, DAG);
  case ISD::ATOMIC_FENCE:
    return LowerATOMIC_FENCE(Op, DAG, Subtarget);
  case ISD::GlobalAddress:
    return lowerGlobalAddress(Op, DAG);
  case ISD::BlockAddress:
    return lowerBlockAddress(Op, DAG);
  case ISD::ConstantPool:
    return lowerConstantPool(Op, DAG);
  case ISD::JumpTable:
    return lowerJumpTable(Op, DAG);
  case ISD::GlobalTLSAddress:
    return lowerGlobalTLSAddress(Op, DAG);
  case ISD::Constant:
    return lowerConstant(Op, DAG, Subtarget);
  case ISD::SELECT:
    return lowerSELECT(Op, DAG);
  case ISD::BRCOND:
    return lowerBRCOND(Op, DAG);
  case ISD::VASTART:
    return lowerVASTART(Op, DAG);
  case ISD::FRAMEADDR:
    return lowerFRAMEADDR(Op, DAG);
  case ISD::RETURNADDR:
    return lowerRETURNADDR(Op, DAG);
  case ISD::SHL_PARTS:
    return lowerShiftLeftParts(Op, DAG);
  case ISD::SRA_PARTS:
    return lowerShiftRightParts(Op, DAG, true);
  case ISD::SRL_PARTS:
    return lowerShiftRightParts(Op, DAG, false);
  case ISD::INTRINSIC_WO_CHAIN:
    return LowerINTRINSIC_WO_CHAIN(Op, DAG);
  case ISD::INTRINSIC_W_CHAIN:
    return LowerINTRINSIC_W_CHAIN(Op, DAG);
  case ISD::INTRINSIC_VOID:
    return LowerINTRINSIC_VOID(Op, DAG);
  case ISD::LOAD:
    return Op;
  case ISD::STORE:
    return Op;
  case ISD::SELECT_CC: {
    SDValue Tmp1 = Op.getOperand(0);
    SDValue Tmp2 = Op.getOperand(1);
    SDValue True = Op.getOperand(2);
    SDValue False = Op.getOperand(3);
    EVT VT = Op.getValueType();
    SDValue CC = Op.getOperand(4);
    EVT CmpVT = Tmp1.getValueType();
    EVT CCVT =
        getSetCCResultType(DAG.getDataLayout(), *DAG.getContext(), CmpVT);
    SDLoc DL(Op);
    SDValue Cond =
        DAG.getNode(ISD::SETCC, DL, CCVT, Tmp1, Tmp2, CC, Op->getFlags());
    return DAG.getSelect(DL, VT, Cond, True, False);
  }
  case ISD::SETCC: {
    MVT OpVT = Op.getOperand(0).getSimpleValueType();
    if (!OpVT.isScalarInteger())
      return SDValue();
    MVT VT = Op.getSimpleValueType();
    SDValue LHS = Op.getOperand(0);
    SDValue RHS = Op.getOperand(1);
    ISD::CondCode CCVal = cast<CondCodeSDNode>(Op.getOperand(2))->get();
    if (CCVal != ISD::SETGT && CCVal != ISD::SETUGT)
      return SDValue();

    SDLoc DL(Op);
    if (isa<ConstantSDNode>(RHS)) {
      int64_t Imm = cast<ConstantSDNode>(RHS)->getSExtValue();
      if (Imm != 0 && isInt<12>((uint64_t)Imm + 1)) {
        if (CCVal == ISD::SETUGT && Imm == -1)
          return DAG.getConstant(0, DL, VT);
        CCVal = ISD::getSetCCSwappedOperands(CCVal);
        SDValue SetCC = DAG.getSetCC(
            DL, VT, LHS, DAG.getSignedConstant(Imm + 1, DL, OpVT), CCVal);
        return DAG.getLogicalNOT(DL, SetCC, VT);
      }
      if (CCVal == ISD::SETUGT && Imm == 2047) {
        SDValue Shift = DAG.getNode(ISD::SRL, DL, OpVT, LHS,
                                    DAG.getShiftAmountConstant(11, OpVT, DL));
        return DAG.getSetCC(DL, VT, Shift, DAG.getConstant(0, DL, OpVT),
                            ISD::SETNE);
      }
    }

    CCVal = ISD::getSetCCSwappedOperands(CCVal);
    return DAG.getSetCC(DL, VT, RHS, LHS, CCVal);
  }
  case ISD::SHL:
  case ISD::SRL:
  case ISD::SRA:
    return SDValue();
  case ISD::EH_DWARF_CFA:
    return lowerEH_DWARF_CFA(Op, DAG);
  case ISD::CLEAR_CACHE: {
    if (!getTargetMachine().getTargetTriple().isOSLinux())
      return SDValue();
    SDLoc DL(Op);
    SDValue Flags = DAG.getConstant(0, DL, Subtarget.getXLenVT());
    return emitFlushICache(DAG, Op.getOperand(0), Op.getOperand(1),
                           Op.getOperand(2), Flags, DL);
  }
  case ISD::DYNAMIC_STACKALLOC:
    return lowerDYNAMIC_STACKALLOC(Op, DAG);
  case ISD::INIT_TRAMPOLINE:
    return lowerINIT_TRAMPOLINE(Op, DAG);
  case ISD::ADJUST_TRAMPOLINE:
    return lowerADJUST_TRAMPOLINE(Op, DAG);
  }
}

SDValue YSXTargetLowering::emitFlushICache(SelectionDAG &DAG, SDValue InChain,
                                             SDValue Start, SDValue End,
                                             SDValue Flags, SDLoc DL) const {
  MakeLibCallOptions CallOptions;
  std::pair<SDValue, SDValue> CallResult =
      makeLibCall(DAG, RTLIB::RISCV_FLUSH_ICACHE, MVT::isVoid,
                  {Start, End, Flags}, CallOptions, DL, InChain);

  // This function returns void so only the out chain matters.
  return CallResult.second;
}

SDValue YSXTargetLowering::lowerINIT_TRAMPOLINE(SDValue Op,
                                                  SelectionDAG &DAG) const {
  if (!Subtarget.is64Bit())
    llvm::reportFatalUsageError("Trampolines only implemented for RV64");

  // Create an MCCodeEmitter to encode instructions.
  TargetLoweringObjectFile *TLO = getTargetMachine().getObjFileLowering();
  assert(TLO);
  MCContext &MCCtx = TLO->getContext();

  std::unique_ptr<MCCodeEmitter> CodeEmitter(
      createYSXMCCodeEmitter(*getTargetMachine().getMCInstrInfo(), MCCtx));

  SDValue Root = Op.getOperand(0);
  SDValue Trmp = Op.getOperand(1); // trampoline
  SDLoc dl(Op);

  const Value *TrmpAddr = cast<SrcValueSDNode>(Op.getOperand(4))->getValue();

  // We store in the trampoline buffer the following instructions and data.
  // Offset:
  //      0: auipc   t2, 0
  //      4: ld      t0, 24(t2)
  //      8: ld      t2, 16(t2)
  //     12: jalr    t0
  //     16: <StaticChainOffset>
  //     24: <FunctionAddressOffset>
  //     32:
  // Offset with branch control flow protection enabled:
  //      0: lpad    <imm20>
  //      4: auipc   t3, 0
  //      8: ld      t2, 28(t3)
  //     12: ld      t3, 20(t3)
  //     16: jalr    t2
  //     20: <StaticChainOffset>
  //     28: <FunctionAddressOffset>
  //     36:

  const bool HasCFBranch =
      Subtarget.hasStdExtZicfilp() &&
      DAG.getMachineFunction().getFunction().getParent()->getModuleFlag(
          "cf-protection-branch");
  const unsigned StaticChainIdx = HasCFBranch ? 5 : 4;
  const unsigned StaticChainOffset = StaticChainIdx * 4;
  const unsigned FunctionAddressOffset = StaticChainOffset + 8;

  const MCSubtargetInfo *STI = getTargetMachine().getMCSubtargetInfo();
  assert(STI);
  auto GetEncoding = [&](const MCInst &MC) {
    SmallVector<char, 4> CB;
    SmallVector<MCFixup> Fixups;
    CodeEmitter->encodeInstruction(MC, CB, Fixups, *STI);
    uint32_t Encoding = support::endian::read32le(CB.data());
    return Encoding;
  };

  SmallVector<SDValue> OutChains;

  SmallVector<uint32_t> Encodings;
  if (!HasCFBranch) {
    Encodings.append(
        {// auipc t2, 0
         // Loads the current PC into t2.
         GetEncoding(MCInstBuilder(YSX::AUIPC).addReg(YSX::X7).addImm(0)),
         // ld t0, 24(t2)
         // Loads the function address into t0. Note that we are using offsets
         // pc-relative to the first instruction of the trampoline.
         GetEncoding(MCInstBuilder(YSX::LD)
                         .addReg(YSX::X5)
                         .addReg(YSX::X7)
                         .addImm(FunctionAddressOffset)),
         // ld t2, 16(t2)
         // Load the value of the static chain.
         GetEncoding(MCInstBuilder(YSX::LD)
                         .addReg(YSX::X7)
                         .addReg(YSX::X7)
                         .addImm(StaticChainOffset)),
         // jalr t0
         // Jump to the function.
         GetEncoding(MCInstBuilder(YSX::JALR)
                         .addReg(YSX::X0)
                         .addReg(YSX::X5)
                         .addImm(0))});
  } else {
    Encodings.append(
        {// auipc x0, <imm20> (lpad <imm20>)
         // Landing pad.
         GetEncoding(MCInstBuilder(YSX::AUIPC).addReg(YSX::X0).addImm(0)),
         // auipc t3, 0
         // Loads the current PC into t3.
         GetEncoding(MCInstBuilder(YSX::AUIPC).addReg(YSX::X28).addImm(0)),
         // ld t2, (FunctionAddressOffset - 4)(t3)
         // Loads the function address into t2. Note that we are using offsets
         // pc-relative to the SECOND instruction of the trampoline.
         GetEncoding(MCInstBuilder(YSX::LD)
                         .addReg(YSX::X7)
                         .addReg(YSX::X28)
                         .addImm(FunctionAddressOffset - 4)),
         // ld t3, (StaticChainOffset - 4)(t3)
         // Load the value of the static chain.
         GetEncoding(MCInstBuilder(YSX::LD)
                         .addReg(YSX::X28)
                         .addReg(YSX::X28)
                         .addImm(StaticChainOffset - 4)),
         // jalr t2
         // Software-guarded jump to the function.
         GetEncoding(MCInstBuilder(YSX::JALR)
                         .addReg(YSX::X0)
                         .addReg(YSX::X7)
                         .addImm(0))});
  }

  // Store encoded instructions.
  for (auto [Idx, Encoding] : llvm::enumerate(Encodings)) {
    SDValue Addr = Idx > 0 ? DAG.getNode(ISD::ADD, dl, MVT::i64, Trmp,
                                         DAG.getConstant(Idx * 4, dl, MVT::i64))
                           : Trmp;
    OutChains.push_back(DAG.getTruncStore(
        Root, dl, DAG.getConstant(Encoding, dl, MVT::i64), Addr,
        MachinePointerInfo(TrmpAddr, Idx * 4), MVT::i32));
  }

  // Now store the variable part of the trampoline.
  SDValue FunctionAddress = Op.getOperand(2);
  SDValue StaticChain = Op.getOperand(3);

  // Store the given static chain and function pointer in the trampoline buffer.
  struct OffsetValuePair {
    const unsigned Offset;
    const SDValue Value;
    SDValue Addr = SDValue(); // Used to cache the address.
  } OffsetValues[] = {
      {StaticChainOffset, StaticChain},
      {FunctionAddressOffset, FunctionAddress},
  };
  for (auto &OffsetValue : OffsetValues) {
    SDValue Addr =
        DAG.getNode(ISD::ADD, dl, MVT::i64, Trmp,
                    DAG.getConstant(OffsetValue.Offset, dl, MVT::i64));
    OffsetValue.Addr = Addr;
    OutChains.push_back(
        DAG.getStore(Root, dl, OffsetValue.Value, Addr,
                     MachinePointerInfo(TrmpAddr, OffsetValue.Offset)));
  }

  assert(OutChains.size() == StaticChainIdx + 2 &&
         "Size of OutChains mismatch");
  SDValue StoreToken = DAG.getNode(ISD::TokenFactor, dl, MVT::Other, OutChains);

  // The end of instructions of trampoline is the same as the static chain
  // address that we computed earlier.
  SDValue EndOfTrmp = OffsetValues[0].Addr;

  // Call clear cache on the trampoline instructions.
  SDValue Chain = DAG.getNode(ISD::CLEAR_CACHE, dl, MVT::Other, StoreToken,
                              Trmp, EndOfTrmp);

  return Chain;
}

SDValue YSXTargetLowering::lowerADJUST_TRAMPOLINE(SDValue Op,
                                                    SelectionDAG &DAG) const {
  if (!Subtarget.is64Bit())
    llvm::reportFatalUsageError("Trampolines only implemented for RV64");

  return Op.getOperand(0);
}

static SDValue getTargetNode(GlobalAddressSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetGlobalAddress(N->getGlobal(), DL, Ty, 0, Flags);
}

static SDValue getTargetNode(BlockAddressSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetBlockAddress(N->getBlockAddress(), Ty, N->getOffset(),
                                   Flags);
}

static SDValue getTargetNode(ConstantPoolSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetConstantPool(N->getConstVal(), Ty, N->getAlign(),
                                   N->getOffset(), Flags);
}

static SDValue getTargetNode(JumpTableSDNode *N, const SDLoc &DL, EVT Ty,
                             SelectionDAG &DAG, unsigned Flags) {
  return DAG.getTargetJumpTable(N->getIndex(), Ty, Flags);
}

static SDValue getLargeGlobalAddress(GlobalAddressSDNode *N, const SDLoc &DL,
                                     EVT Ty, SelectionDAG &DAG) {
  YSXConstantPoolValue *CPV = YSXConstantPoolValue::Create(N->getGlobal());
  SDValue CPAddr = DAG.getTargetConstantPool(CPV, Ty, Align(8));
  SDValue LC = DAG.getNode(YSXISD::LLA, DL, Ty, CPAddr);
  return DAG.getLoad(
      Ty, DL, DAG.getEntryNode(), LC,
      MachinePointerInfo::getConstantPool(DAG.getMachineFunction()));
}

static SDValue getLargeExternalSymbol(ExternalSymbolSDNode *N, const SDLoc &DL,
                                      EVT Ty, SelectionDAG &DAG) {
  YSXConstantPoolValue *CPV =
      YSXConstantPoolValue::Create(*DAG.getContext(), N->getSymbol());
  SDValue CPAddr = DAG.getTargetConstantPool(CPV, Ty, Align(8));
  SDValue LC = DAG.getNode(YSXISD::LLA, DL, Ty, CPAddr);
  return DAG.getLoad(
      Ty, DL, DAG.getEntryNode(), LC,
      MachinePointerInfo::getConstantPool(DAG.getMachineFunction()));
}

template <class NodeTy>
SDValue YSXTargetLowering::getAddr(NodeTy *N, SelectionDAG &DAG,
                                     bool IsLocal, bool IsExternWeak) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());

  // When HWASAN is used and tagging of global variables is enabled
  // they should be accessed via the GOT, since the tagged address of a global
  // is incompatible with existing code models. This also applies to non-pic
  // mode.
  if (isPositionIndependent() || Subtarget.allowTaggedGlobals()) {
    SDValue Addr = getTargetNode(N, DL, Ty, DAG, 0);
    if (IsLocal && !Subtarget.allowTaggedGlobals())
      // Use PC-relative addressing to access the symbol. This generates the
      // pattern (PseudoLLA sym), which expands to (addi (auipc %pcrel_hi(sym))
      // %pcrel_lo(auipc)).
      return DAG.getNode(YSXISD::LLA, DL, Ty, Addr);

    // Use PC-relative addressing to access the GOT for this symbol, then load
    // the address from the GOT. This generates the pattern (PseudoLGA sym),
    // which expands to (ld (addi (auipc %got_pcrel_hi(sym)) %pcrel_lo(auipc))).
    SDValue Load =
        SDValue(DAG.getMachineNode(YSX::PseudoLGA, DL, Ty, Addr), 0);
    MachineFunction &MF = DAG.getMachineFunction();
    MachineMemOperand *MemOp = MF.getMachineMemOperand(
        MachinePointerInfo::getGOT(MF),
        MachineMemOperand::MOLoad | MachineMemOperand::MODereferenceable |
            MachineMemOperand::MOInvariant,
        LLT(Ty.getSimpleVT()), Align(Ty.getFixedSizeInBits() / 8));
    DAG.setNodeMemRefs(cast<MachineSDNode>(Load.getNode()), {MemOp});
    return Load;
  }

  switch (getTargetMachine().getCodeModel()) {
  default:
    reportFatalUsageError("Unsupported code model for lowering");
  case CodeModel::Small: {
    // Generate a sequence for accessing addresses within the first 2 GiB of
    // address space.
    // This generates the pattern (addi (lui %hi(sym)) %lo(sym)).
    SDValue AddrHi = getTargetNode(N, DL, Ty, DAG, YSXII::MO_HI);
    SDValue AddrLo = getTargetNode(N, DL, Ty, DAG, YSXII::MO_LO);
    SDValue MNHi = DAG.getNode(YSXISD::HI, DL, Ty, AddrHi);
    return DAG.getNode(YSXISD::ADD_LO, DL, Ty, MNHi, AddrLo);
  }
  case CodeModel::Medium: {
    SDValue Addr = getTargetNode(N, DL, Ty, DAG, 0);
    if (IsExternWeak) {
      // An extern weak symbol may be undefined, i.e. have value 0, which may
      // not be within 2GiB of PC, so use GOT-indirect addressing to access the
      // symbol. This generates the pattern (PseudoLGA sym), which expands to
      // (ld (addi (auipc %got_pcrel_hi(sym)) %pcrel_lo(auipc))).
      SDValue Load =
          SDValue(DAG.getMachineNode(YSX::PseudoLGA, DL, Ty, Addr), 0);
      MachineFunction &MF = DAG.getMachineFunction();
      MachineMemOperand *MemOp = MF.getMachineMemOperand(
          MachinePointerInfo::getGOT(MF),
          MachineMemOperand::MOLoad | MachineMemOperand::MODereferenceable |
              MachineMemOperand::MOInvariant,
          LLT(Ty.getSimpleVT()), Align(Ty.getFixedSizeInBits() / 8));
      DAG.setNodeMemRefs(cast<MachineSDNode>(Load.getNode()), {MemOp});
      return Load;
    }

    // Generate a sequence for accessing addresses within any 2GiB range within
    // the address space. This generates the pattern (PseudoLLA sym), which
    // expands to (addi (auipc %pcrel_hi(sym)) %pcrel_lo(auipc)).
    return DAG.getNode(YSXISD::LLA, DL, Ty, Addr);
  }
  case CodeModel::Large: {
    if (GlobalAddressSDNode *G = dyn_cast<GlobalAddressSDNode>(N))
      return getLargeGlobalAddress(G, DL, Ty, DAG);

    // Using pc-relative mode for other node type.
    SDValue Addr = getTargetNode(N, DL, Ty, DAG, 0);
    return DAG.getNode(YSXISD::LLA, DL, Ty, Addr);
  }
  }
}

SDValue YSXTargetLowering::lowerGlobalAddress(SDValue Op,
                                                SelectionDAG &DAG) const {
  GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
  assert(N->getOffset() == 0 && "unexpected offset in global node");
  const GlobalValue *GV = N->getGlobal();
  return getAddr(N, DAG, GV->isDSOLocal(), GV->hasExternalWeakLinkage());
}

SDValue YSXTargetLowering::lowerBlockAddress(SDValue Op,
                                               SelectionDAG &DAG) const {
  BlockAddressSDNode *N = cast<BlockAddressSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue YSXTargetLowering::lowerConstantPool(SDValue Op,
                                               SelectionDAG &DAG) const {
  ConstantPoolSDNode *N = cast<ConstantPoolSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue YSXTargetLowering::lowerJumpTable(SDValue Op,
                                            SelectionDAG &DAG) const {
  JumpTableSDNode *N = cast<JumpTableSDNode>(Op);

  return getAddr(N, DAG);
}

SDValue YSXTargetLowering::getStaticTLSAddr(GlobalAddressSDNode *N,
                                              SelectionDAG &DAG,
                                              bool UseGOT) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());
  const GlobalValue *GV = N->getGlobal();
  MVT XLenVT = Subtarget.getXLenVT();

  if (UseGOT) {
    // Use PC-relative addressing to access the GOT for this TLS symbol, then
    // load the address from the GOT and add the thread pointer. This generates
    // the pattern (PseudoLA_TLS_IE sym), which expands to
    // (ld (auipc %tls_ie_pcrel_hi(sym)) %pcrel_lo(auipc)).
    SDValue Addr = DAG.getTargetGlobalAddress(GV, DL, Ty, 0, 0);
    SDValue Load =
        SDValue(DAG.getMachineNode(YSX::PseudoLA_TLS_IE, DL, Ty, Addr), 0);
    MachineFunction &MF = DAG.getMachineFunction();
    MachineMemOperand *MemOp = MF.getMachineMemOperand(
        MachinePointerInfo::getGOT(MF),
        MachineMemOperand::MOLoad | MachineMemOperand::MODereferenceable |
            MachineMemOperand::MOInvariant,
        LLT(Ty.getSimpleVT()), Align(Ty.getFixedSizeInBits() / 8));
    DAG.setNodeMemRefs(cast<MachineSDNode>(Load.getNode()), {MemOp});

    // Add the thread pointer.
    SDValue TPReg = DAG.getRegister(YSX::X4, XLenVT);
    return DAG.getNode(ISD::ADD, DL, Ty, Load, TPReg);
  }

  // Generate a sequence for accessing the address relative to the thread
  // pointer, with the appropriate adjustment for the thread pointer offset.
  // This generates the pattern
  // (add (add_tprel (lui %tprel_hi(sym)) tp %tprel_add(sym)) %tprel_lo(sym))
  SDValue AddrHi =
      DAG.getTargetGlobalAddress(GV, DL, Ty, 0, YSXII::MO_TPREL_HI);
  SDValue AddrAdd =
      DAG.getTargetGlobalAddress(GV, DL, Ty, 0, YSXII::MO_TPREL_ADD);
  SDValue AddrLo =
      DAG.getTargetGlobalAddress(GV, DL, Ty, 0, YSXII::MO_TPREL_LO);

  SDValue MNHi = DAG.getNode(YSXISD::HI, DL, Ty, AddrHi);
  SDValue TPReg = DAG.getRegister(YSX::X4, XLenVT);
  SDValue MNAdd =
      DAG.getNode(YSXISD::ADD_TPREL, DL, Ty, MNHi, TPReg, AddrAdd);
  return DAG.getNode(YSXISD::ADD_LO, DL, Ty, MNAdd, AddrLo);
}

SDValue YSXTargetLowering::getDynamicTLSAddr(GlobalAddressSDNode *N,
                                               SelectionDAG &DAG) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());
  IntegerType *CallTy = Type::getIntNTy(*DAG.getContext(), Ty.getSizeInBits());
  const GlobalValue *GV = N->getGlobal();

  // Use a PC-relative addressing mode to access the global dynamic GOT address.
  // This generates the pattern (PseudoLA_TLS_GD sym), which expands to
  // (addi (auipc %tls_gd_pcrel_hi(sym)) %pcrel_lo(auipc)).
  SDValue Addr = DAG.getTargetGlobalAddress(GV, DL, Ty, 0, 0);
  SDValue Load =
      SDValue(DAG.getMachineNode(YSX::PseudoLA_TLS_GD, DL, Ty, Addr), 0);

  // Prepare argument list to generate call.
  ArgListTy Args;
  Args.emplace_back(Load, CallTy);

  // Setup call to __tls_get_addr.
  TargetLowering::CallLoweringInfo CLI(DAG);
  CLI.setDebugLoc(DL)
      .setChain(DAG.getEntryNode())
      .setLibCallee(CallingConv::C, CallTy,
                    DAG.getExternalSymbol("__tls_get_addr", Ty),
                    std::move(Args));

  return LowerCallTo(CLI).first;
}

SDValue YSXTargetLowering::getTLSDescAddr(GlobalAddressSDNode *N,
                                            SelectionDAG &DAG) const {
  SDLoc DL(N);
  EVT Ty = getPointerTy(DAG.getDataLayout());
  const GlobalValue *GV = N->getGlobal();

  // Use a PC-relative addressing mode to access the global dynamic GOT address.
  // This generates the pattern (PseudoLA_TLSDESC sym), which expands to
  //
  // auipc tX, %tlsdesc_hi(symbol)         // R_RISCV_TLSDESC_HI20(symbol)
  // lw    tY, tX, %tlsdesc_load_lo(label) // R_RISCV_TLSDESC_LOAD_LO12(label)
  // addi  a0, tX, %tlsdesc_add_lo(label)  // R_RISCV_TLSDESC_ADD_LO12(label)
  // jalr  t0, tY                          // R_RISCV_TLSDESC_CALL(label)
  SDValue Addr = DAG.getTargetGlobalAddress(GV, DL, Ty, 0, 0);
  return SDValue(DAG.getMachineNode(YSX::PseudoLA_TLSDESC, DL, Ty, Addr), 0);
}

SDValue YSXTargetLowering::lowerGlobalTLSAddress(SDValue Op,
                                                   SelectionDAG &DAG) const {
  GlobalAddressSDNode *N = cast<GlobalAddressSDNode>(Op);
  assert(N->getOffset() == 0 && "unexpected offset in global node");

  if (DAG.getTarget().useEmulatedTLS())
    return LowerToTLSEmulatedModel(N, DAG);

  TLSModel::Model Model = getTargetMachine().getTLSModel(N->getGlobal());

  if (DAG.getMachineFunction().getFunction().getCallingConv() ==
      CallingConv::GHC)
    reportFatalUsageError("In GHC calling convention TLS is not supported");

  SDValue Addr;
  switch (Model) {
  case TLSModel::LocalExec:
    Addr = getStaticTLSAddr(N, DAG, /*UseGOT=*/false);
    break;
  case TLSModel::InitialExec:
    Addr = getStaticTLSAddr(N, DAG, /*UseGOT=*/true);
    break;
  case TLSModel::LocalDynamic:
  case TLSModel::GeneralDynamic:
    Addr = DAG.getTarget().useTLSDESC() ? getTLSDescAddr(N, DAG)
                                        : getDynamicTLSAddr(N, DAG);
    break;
  }

  return Addr;
}

// Return true if Val is equal to (setcc LHS, RHS, CC).
// Return false if Val is the inverse of (setcc LHS, RHS, CC).
// Otherwise, return std::nullopt.
static std::optional<bool> matchSetCC(SDValue LHS, SDValue RHS,
                                      ISD::CondCode CC, SDValue Val) {
  assert(Val->getOpcode() == ISD::SETCC);
  SDValue LHS2 = Val.getOperand(0);
  SDValue RHS2 = Val.getOperand(1);
  ISD::CondCode CC2 = cast<CondCodeSDNode>(Val.getOperand(2))->get();

  if (LHS == LHS2 && RHS == RHS2) {
    if (CC == CC2)
      return true;
    if (CC == ISD::getSetCCInverse(CC2, LHS2.getValueType()))
      return false;
  } else if (LHS == RHS2 && RHS == LHS2) {
    CC2 = ISD::getSetCCSwappedOperands(CC2);
    if (CC == CC2)
      return true;
    if (CC == ISD::getSetCCInverse(CC2, LHS2.getValueType()))
      return false;
  }

  return std::nullopt;
}

static bool isSimm12Constant(SDValue V) {
  return isa<ConstantSDNode>(V) && V->getAsAPIntVal().isSignedIntN(12);
}

static SDValue lowerSelectToBinOp(SDNode *N, SelectionDAG &DAG,
                                  const YSXSubtarget &Subtarget) {
  SDValue CondV = N->getOperand(0);
  SDValue TrueV = N->getOperand(1);
  SDValue FalseV = N->getOperand(2);
  MVT VT = N->getSimpleValueType(0);
  SDLoc DL(N);

  if (!Subtarget.hasConditionalMoveFusion()) {
    // (select c, -1, y) -> -c | y
    if (isAllOnesConstant(TrueV)) {
      SDValue Neg = DAG.getNegative(CondV, DL, VT);
      return DAG.getNode(ISD::OR, DL, VT, Neg, DAG.getFreeze(FalseV));
    }
    // (select c, y, -1) -> (c-1) | y
    if (isAllOnesConstant(FalseV)) {
      SDValue Neg = DAG.getNode(ISD::ADD, DL, VT, CondV,
                                DAG.getAllOnesConstant(DL, VT));
      return DAG.getNode(ISD::OR, DL, VT, Neg, DAG.getFreeze(TrueV));
    }

    const bool HasCZero = VT.isScalarInteger() && Subtarget.hasCZEROLike();

    // (select c, 0, y) -> (c-1) & y
    if (isNullConstant(TrueV) && (!HasCZero || isSimm12Constant(FalseV))) {
      SDValue Neg =
          DAG.getNode(ISD::ADD, DL, VT, CondV, DAG.getAllOnesConstant(DL, VT));
      return DAG.getNode(ISD::AND, DL, VT, Neg, DAG.getFreeze(FalseV));
    }
    if (isNullConstant(FalseV)) {
      // (select c, y, 0) -> -c & y
      if (!HasCZero || isSimm12Constant(TrueV)) {
        SDValue Neg = DAG.getNegative(CondV, DL, VT);
        return DAG.getNode(ISD::AND, DL, VT, Neg, DAG.getFreeze(TrueV));
      }
    }
  }

  // select c, ~x, x --> xor -c, x
  if (isa<ConstantSDNode>(TrueV) && isa<ConstantSDNode>(FalseV)) {
    const APInt &TrueVal = TrueV->getAsAPIntVal();
    const APInt &FalseVal = FalseV->getAsAPIntVal();
    if (~TrueVal == FalseVal) {
      SDValue Neg = DAG.getNegative(CondV, DL, VT);
      return DAG.getNode(ISD::XOR, DL, VT, Neg, FalseV);
    }
  }

  // Try to fold (select (setcc lhs, rhs, cc), truev, falsev) into bitwise ops
  // when both truev and falsev are also setcc.
  if (CondV.getOpcode() == ISD::SETCC && TrueV.getOpcode() == ISD::SETCC &&
      FalseV.getOpcode() == ISD::SETCC) {
    SDValue LHS = CondV.getOperand(0);
    SDValue RHS = CondV.getOperand(1);
    ISD::CondCode CC = cast<CondCodeSDNode>(CondV.getOperand(2))->get();

    // (select x, x, y) -> x | y
    // (select !x, x, y) -> x & y
    if (std::optional<bool> MatchResult = matchSetCC(LHS, RHS, CC, TrueV)) {
      return DAG.getNode(*MatchResult ? ISD::OR : ISD::AND, DL, VT, TrueV,
                         DAG.getFreeze(FalseV));
    }
    // (select x, y, x) -> x & y
    // (select !x, y, x) -> x | y
    if (std::optional<bool> MatchResult = matchSetCC(LHS, RHS, CC, FalseV)) {
      return DAG.getNode(*MatchResult ? ISD::AND : ISD::OR, DL, VT,
                         DAG.getFreeze(TrueV), FalseV);
    }
  }

  return SDValue();
}

SDValue YSXTargetLowering::lowerSELECT(SDValue Op, SelectionDAG &DAG) const {
  SDValue CondV = Op.getOperand(0);
  SDValue TrueV = Op.getOperand(1);
  SDValue FalseV = Op.getOperand(2);
  SDLoc DL(Op);
  MVT VT = Op.getSimpleValueType();
  MVT XLenVT = Subtarget.getXLenVT();

  if (VT.isVector())
    return SDValue();

  if (SDValue V = lowerSelectToBinOp(Op.getNode(), DAG, Subtarget))
    return V;

  if (CondV.getOpcode() != ISD::SETCC ||
      CondV.getOperand(0).getSimpleValueType() != XLenVT) {
    SDValue Zero = DAG.getConstant(0, DL, XLenVT);
    SDValue SetNE = DAG.getCondCode(ISD::SETNE);
    SDValue Ops[] = {CondV, Zero, SetNE, TrueV, FalseV};
    return DAG.getNode(YSXISD::SELECT_CC, DL, VT, Ops);
  }

  SDValue LHS = CondV.getOperand(0);
  SDValue RHS = CondV.getOperand(1);
  ISD::CondCode CCVal = cast<CondCodeSDNode>(CondV.getOperand(2))->get();
  translateSetCCForBranch(DL, LHS, RHS, CCVal, DAG, Subtarget);
  SDValue Ops[] = {LHS, RHS, DAG.getCondCode(CCVal), TrueV, FalseV};
  return DAG.getNode(YSXISD::SELECT_CC, DL, VT, Ops);
}

SDValue YSXTargetLowering::lowerBRCOND(SDValue Op, SelectionDAG &DAG) const {
  SDValue CondV = Op.getOperand(1);
  SDLoc DL(Op);
  MVT XLenVT = Subtarget.getXLenVT();

  if (CondV.getOpcode() == ISD::SETCC &&
      CondV.getOperand(0).getValueType() == XLenVT) {
    SDValue LHS = CondV.getOperand(0);
    SDValue RHS = CondV.getOperand(1);
    ISD::CondCode CCVal = cast<CondCodeSDNode>(CondV.getOperand(2))->get();

    translateSetCCForBranch(DL, LHS, RHS, CCVal, DAG, Subtarget);

    SDValue TargetCC = DAG.getCondCode(CCVal);
    return DAG.getNode(YSXISD::BR_CC, DL, Op.getValueType(), Op.getOperand(0),
                       LHS, RHS, TargetCC, Op.getOperand(2));
  }

  return DAG.getNode(YSXISD::BR_CC, DL, Op.getValueType(), Op.getOperand(0),
                     CondV, DAG.getConstant(0, DL, XLenVT),
                     DAG.getCondCode(ISD::SETNE), Op.getOperand(2));
}

SDValue YSXTargetLowering::lowerVASTART(SDValue Op, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  YSXMachineFunctionInfo *FuncInfo = MF.getInfo<YSXMachineFunctionInfo>();

  SDLoc DL(Op);
  SDValue FI = DAG.getFrameIndex(FuncInfo->getVarArgsFrameIndex(),
                                 getPointerTy(MF.getDataLayout()));

  // vastart just stores the address of the VarArgsFrameIndex slot into the
  // memory location argument.
  const Value *SV = cast<SrcValueSDNode>(Op.getOperand(2))->getValue();
  return DAG.getStore(Op.getOperand(0), DL, FI, Op.getOperand(1),
                      MachinePointerInfo(SV));
}

SDValue YSXTargetLowering::lowerFRAMEADDR(SDValue Op,
                                            SelectionDAG &DAG) const {
  const YSXRegisterInfo &RI = *Subtarget.getRegisterInfo();
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setFrameAddressIsTaken(true);
  Register FrameReg = RI.getFrameRegister(MF);
  int XLenInBytes = Subtarget.getXLen() / 8;

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  SDValue FrameAddr = DAG.getCopyFromReg(DAG.getEntryNode(), DL, FrameReg, VT);
  unsigned Depth = Op.getConstantOperandVal(0);
  while (Depth--) {
    int Offset = -(XLenInBytes * 2);
    SDValue Ptr = DAG.getNode(
        ISD::ADD, DL, VT, FrameAddr,
        DAG.getSignedConstant(Offset, DL, getPointerTy(DAG.getDataLayout())));
    FrameAddr =
        DAG.getLoad(VT, DL, DAG.getEntryNode(), Ptr, MachinePointerInfo());
  }
  return FrameAddr;
}

SDValue YSXTargetLowering::lowerRETURNADDR(SDValue Op,
                                             SelectionDAG &DAG) const {
  const YSXRegisterInfo &RI = *Subtarget.getRegisterInfo();
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  MFI.setReturnAddressIsTaken(true);
  MVT XLenVT = Subtarget.getXLenVT();
  int XLenInBytes = Subtarget.getXLen() / 8;

  EVT VT = Op.getValueType();
  SDLoc DL(Op);
  unsigned Depth = Op.getConstantOperandVal(0);
  if (Depth) {
    int Off = -XLenInBytes;
    SDValue FrameAddr = lowerFRAMEADDR(Op, DAG);
    SDValue Offset = DAG.getSignedConstant(Off, DL, VT);
    return DAG.getLoad(VT, DL, DAG.getEntryNode(),
                       DAG.getNode(ISD::ADD, DL, VT, FrameAddr, Offset),
                       MachinePointerInfo());
  }

  // Return the value of the return address register, marking it an implicit
  // live-in.
  Register Reg = MF.addLiveIn(RI.getRARegister(), getRegClassFor(XLenVT));
  return DAG.getCopyFromReg(DAG.getEntryNode(), DL, Reg, XLenVT);
}

SDValue YSXTargetLowering::lowerShiftLeftParts(SDValue Op,
                                                 SelectionDAG &DAG) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue Shamt = Op.getOperand(2);
  EVT VT = Lo.getValueType();

  // if Shamt-XLEN < 0: // Shamt < XLEN
  //   Lo = Lo << Shamt
  //   Hi = (Hi << Shamt) | ((Lo >>u 1) >>u (XLEN-1 - Shamt))
  // else:
  //   Lo = 0
  //   Hi = Lo << (Shamt-XLEN)

  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue One = DAG.getConstant(1, DL, VT);
  SDValue MinusXLen = DAG.getSignedConstant(-(int)Subtarget.getXLen(), DL, VT);
  SDValue XLenMinus1 = DAG.getConstant(Subtarget.getXLen() - 1, DL, VT);
  SDValue ShamtMinusXLen = DAG.getNode(ISD::ADD, DL, VT, Shamt, MinusXLen);
  SDValue XLenMinus1Shamt = DAG.getNode(ISD::SUB, DL, VT, XLenMinus1, Shamt);

  SDValue LoTrue = DAG.getNode(ISD::SHL, DL, VT, Lo, Shamt);
  SDValue ShiftRight1Lo = DAG.getNode(ISD::SRL, DL, VT, Lo, One);
  SDValue ShiftRightLo =
      DAG.getNode(ISD::SRL, DL, VT, ShiftRight1Lo, XLenMinus1Shamt);
  SDValue ShiftLeftHi = DAG.getNode(ISD::SHL, DL, VT, Hi, Shamt);
  SDValue HiTrue = DAG.getNode(ISD::OR, DL, VT, ShiftLeftHi, ShiftRightLo);
  SDValue HiFalse = DAG.getNode(ISD::SHL, DL, VT, Lo, ShamtMinusXLen);

  SDValue CC = DAG.getSetCC(DL, VT, ShamtMinusXLen, Zero, ISD::SETLT);

  Lo = DAG.getNode(ISD::SELECT, DL, VT, CC, LoTrue, Zero);
  Hi = DAG.getNode(ISD::SELECT, DL, VT, CC, HiTrue, HiFalse);

  SDValue Parts[2] = {Lo, Hi};
  return DAG.getMergeValues(Parts, DL);
}

SDValue YSXTargetLowering::lowerShiftRightParts(SDValue Op, SelectionDAG &DAG,
                                                  bool IsSRA) const {
  SDLoc DL(Op);
  SDValue Lo = Op.getOperand(0);
  SDValue Hi = Op.getOperand(1);
  SDValue Shamt = Op.getOperand(2);
  EVT VT = Lo.getValueType();

  // SRA expansion:
  //   if Shamt-XLEN < 0: // Shamt < XLEN
  //     Lo = (Lo >>u Shamt) | ((Hi << 1) << (XLEN-1 - ShAmt))
  //     Hi = Hi >>s Shamt
  //   else:
  //     Lo = Hi >>s (Shamt-XLEN);
  //     Hi = Hi >>s (XLEN-1)
  //
  // SRL expansion:
  //   if Shamt-XLEN < 0: // Shamt < XLEN
  //     Lo = (Lo >>u Shamt) | ((Hi << 1) << (XLEN-1 - ShAmt))
  //     Hi = Hi >>u Shamt
  //   else:
  //     Lo = Hi >>u (Shamt-XLEN);
  //     Hi = 0;

  unsigned ShiftRightOp = IsSRA ? ISD::SRA : ISD::SRL;

  SDValue Zero = DAG.getConstant(0, DL, VT);
  SDValue One = DAG.getConstant(1, DL, VT);
  SDValue MinusXLen = DAG.getSignedConstant(-(int)Subtarget.getXLen(), DL, VT);
  SDValue XLenMinus1 = DAG.getConstant(Subtarget.getXLen() - 1, DL, VT);
  SDValue ShamtMinusXLen = DAG.getNode(ISD::ADD, DL, VT, Shamt, MinusXLen);
  SDValue XLenMinus1Shamt = DAG.getNode(ISD::SUB, DL, VT, XLenMinus1, Shamt);

  SDValue ShiftRightLo = DAG.getNode(ISD::SRL, DL, VT, Lo, Shamt);
  SDValue ShiftLeftHi1 = DAG.getNode(ISD::SHL, DL, VT, Hi, One);
  SDValue ShiftLeftHi =
      DAG.getNode(ISD::SHL, DL, VT, ShiftLeftHi1, XLenMinus1Shamt);
  SDValue LoTrue = DAG.getNode(ISD::OR, DL, VT, ShiftRightLo, ShiftLeftHi);
  SDValue HiTrue = DAG.getNode(ShiftRightOp, DL, VT, Hi, Shamt);
  SDValue LoFalse = DAG.getNode(ShiftRightOp, DL, VT, Hi, ShamtMinusXLen);
  SDValue HiFalse =
      IsSRA ? DAG.getNode(ISD::SRA, DL, VT, Hi, XLenMinus1) : Zero;

  SDValue CC = DAG.getSetCC(DL, VT, ShamtMinusXLen, Zero, ISD::SETLT);

  Lo = DAG.getNode(ISD::SELECT, DL, VT, CC, LoTrue, LoFalse);
  Hi = DAG.getNode(ISD::SELECT, DL, VT, CC, HiTrue, HiFalse);

  SDValue Parts[2] = {Lo, Hi};
  return DAG.getMergeValues(Parts, DL);
}

SDValue YSXTargetLowering::LowerINTRINSIC_WO_CHAIN(SDValue Op,
                                                   SelectionDAG &DAG) const {
  unsigned IntNo = Op.getConstantOperandVal(0);
  if (IntNo == Intrinsic::thread_pointer) {
    EVT PtrVT = getPointerTy(DAG.getDataLayout());
    return DAG.getRegister(YSX::X4, PtrVT);
  }
  return SDValue();
}

SDValue YSXTargetLowering::LowerINTRINSIC_W_CHAIN(SDValue Op,
                                                  SelectionDAG &DAG) const {
  return SDValue();
}

SDValue YSXTargetLowering::LowerINTRINSIC_VOID(SDValue Op,
                                               SelectionDAG &DAG) const {
  return SDValue();
}

SDValue YSXTargetLowering::lowerEH_DWARF_CFA(SDValue Op,
                                               SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  bool isYSX64 = Subtarget.is64Bit();
  EVT PtrVT = getPointerTy(DAG.getDataLayout());

  int FI = MF.getFrameInfo().CreateFixedObject(isYSX64 ? 8 : 4, 0, false);
  return DAG.getFrameIndex(FI, PtrVT);
}

// Returns the opcode of the target-specific SDNode that implements the 32-bit
// form of the given Opcode.
static unsigned getYSXWOpcode(unsigned Opcode) {
  switch (Opcode) {
  default:
    llvm_unreachable("Unexpected opcode");
  case ISD::SHL:
    return YSXISD::SLLW;
  case ISD::SRA:
    return YSXISD::SRAW;
  case ISD::SRL:
    return YSXISD::SRLW;
  case ISD::SDIV:
    return YSXISD::DIVW;
  case ISD::UDIV:
    return YSXISD::DIVUW;
  case ISD::UREM:
    return YSXISD::REMUW;
  }
}

// Converts the given i8/i16/i32 operation to a target-specific SelectionDAG
// node. Because i8/i16/i32 isn't a legal type for RV64, these operations would
// otherwise be promoted to i64, making it difficult to select the
// SLLW/DIVUW/.../*W later one because the fact the operation was originally of
// type i8/i16/i32 is lost.
static SDValue customLegalizeToWOp(SDNode *N, SelectionDAG &DAG,
                                   unsigned ExtOpc = ISD::ANY_EXTEND) {
  SDLoc DL(N);
  unsigned WOpcode = getYSXWOpcode(N->getOpcode());
  SDValue NewOp0 = DAG.getNode(ExtOpc, DL, MVT::i64, N->getOperand(0));
  SDValue NewOp1 = DAG.getNode(ExtOpc, DL, MVT::i64, N->getOperand(1));
  SDValue NewRes = DAG.getNode(WOpcode, DL, MVT::i64, NewOp0, NewOp1);
  // ReplaceNodeResults requires we maintain the same type for the return value.
  return DAG.getNode(ISD::TRUNCATE, DL, N->getValueType(0), NewRes);
}

// Converts the given 32-bit operation to a i64 operation with signed extension
// semantic to reduce the signed extension instructions.
static SDValue customLegalizeToWOpWithSExt(SDNode *N, SelectionDAG &DAG) {
  SDLoc DL(N);
  SDValue NewOp0 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(0));
  SDValue NewOp1 = DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(1));
  SDValue NewWOp = DAG.getNode(N->getOpcode(), DL, MVT::i64, NewOp0, NewOp1);
  SDValue NewRes = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64, NewWOp,
                               DAG.getValueType(MVT::i32));
  return DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, NewRes);
}

void YSXTargetLowering::ReplaceNodeResults(SDNode *N,
                                                SmallVectorImpl<SDValue> &Results,
                                                SelectionDAG &DAG) const {
  SDLoc DL(N);
  switch (N->getOpcode()) {
  default:
    return;
  case ISD::LOAD: {
    if (!ISD::isNON_EXTLoad(N))
      return;
    auto *Ld = cast<LoadSDNode>(N);
    if (N->getValueType(0) != MVT::i32 || !Subtarget.is64Bit())
      return;
    SDValue Res = DAG.getExtLoad(ISD::SEXTLOAD, DL, MVT::i64, Ld->getChain(),
                                 Ld->getBasePtr(), Ld->getMemoryVT(),
                                 Ld->getMemOperand());
    Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, Res));
    Results.push_back(Res.getValue(1));
    return;
  }
  case ISD::MUL: {
    unsigned Size = N->getSimpleValueType(0).getSizeInBits();
    unsigned XLen = Subtarget.getXLen();
    if (Size <= XLen) {
      if (N->getValueType(0) == MVT::i32 && Subtarget.is64Bit())
        Results.push_back(customLegalizeToWOpWithSExt(N, DAG));
      return;
    }

    if (Size != XLen * 2)
      return;
    SDValue LHS = N->getOperand(0);
    SDValue RHS = N->getOperand(1);
    APInt HighMask = APInt::getHighBitsSet(Size, XLen);
    bool LHSIsU = DAG.MaskedValueIsZero(LHS, HighMask);
    bool RHSIsU = DAG.MaskedValueIsZero(RHS, HighMask);
    if (LHSIsU == RHSIsU)
      return;

    auto MakeMULPair = [&](SDValue S, SDValue U) {
      MVT XLenVT = Subtarget.getXLenVT();
      S = DAG.getNode(ISD::TRUNCATE, DL, XLenVT, S);
      U = DAG.getNode(ISD::TRUNCATE, DL, XLenVT, U);
      SDValue Lo = DAG.getNode(ISD::MUL, DL, XLenVT, S, U);
      SDValue Hi = DAG.getNode(YSXISD::MULHSU, DL, XLenVT, S, U);
      return DAG.getNode(ISD::BUILD_PAIR, DL, N->getValueType(0), Lo, Hi);
    };

    bool LHSIsS = DAG.ComputeNumSignBits(LHS) > XLen;
    bool RHSIsS = DAG.ComputeNumSignBits(RHS) > XLen;
    if (RHSIsU && LHSIsS && !RHSIsS)
      Results.push_back(MakeMULPair(LHS, RHS));
    else if (LHSIsU && RHSIsS && !LHSIsS)
      Results.push_back(MakeMULPair(RHS, LHS));
    return;
  }
  case ISD::ADD:
  case ISD::SUB:
    if (N->getValueType(0) == MVT::i32 && Subtarget.is64Bit())
      Results.push_back(customLegalizeToWOpWithSExt(N, DAG));
    return;
  case ISD::SHL:
  case ISD::SRA:
  case ISD::SRL:
    if (N->getValueType(0) != MVT::i32 || !Subtarget.is64Bit())
      return;
    if (N->getOperand(1).getOpcode() != ISD::Constant) {
      Results.push_back(customLegalizeToWOp(N, DAG));
      return;
    }
    if (N->getOpcode() == ISD::SHL) {
      SDValue NewOp0 =
          DAG.getNode(ISD::ANY_EXTEND, DL, MVT::i64, N->getOperand(0));
      SDValue NewOp1 =
          DAG.getNode(ISD::ZERO_EXTEND, DL, MVT::i64, N->getOperand(1));
      SDValue NewWOp = DAG.getNode(ISD::SHL, DL, MVT::i64, NewOp0, NewOp1);
      SDValue NewRes = DAG.getNode(ISD::SIGN_EXTEND_INREG, DL, MVT::i64,
                                   NewWOp, DAG.getValueType(MVT::i32));
      Results.push_back(DAG.getNode(ISD::TRUNCATE, DL, MVT::i32, NewRes));
    }
    return;
  case ISD::SDIV:
  case ISD::UDIV:
  case ISD::UREM: {
    MVT VT = N->getSimpleValueType(0);
    if (!(VT == MVT::i8 || VT == MVT::i16 || VT == MVT::i32) ||
        !Subtarget.is64Bit() || !Subtarget.hasStdExtM())
      return;
    AttributeList Attr = DAG.getMachineFunction().getFunction().getAttributes();
    if (N->getOperand(1).getOpcode() == ISD::Constant &&
        !isIntDivCheap(N->getValueType(0), Attr))
      return;
    unsigned ExtOpc = ISD::ANY_EXTEND;
    if (VT != MVT::i32)
      ExtOpc = N->getOpcode() == ISD::SDIV ? ISD::SIGN_EXTEND
                                           : ISD::ZERO_EXTEND;
    Results.push_back(customLegalizeToWOp(N, DAG, ExtOpc));
    return;
  }
  }
}

SDValue YSXTargetLowering::PerformDAGCombine(SDNode *N,
                                                   DAGCombinerInfo &DCI) const {
  return SDValue();
}

bool YSXTargetLowering::shouldTransformSignedTruncationCheck(
    EVT XVT, unsigned KeptBits) const {
  // For vectors, we don't have a preference..
  if (XVT.isVector())
    return false;

  if (XVT != MVT::i32 && XVT != MVT::i64)
    return false;

  // We can use sext.w for RV64 or an srai 31 on RV32.
  if (KeptBits == 32 || KeptBits == 64)
    return true;

  // With Zbb we can use sext.h/sext.b.
  return Subtarget.hasStdExtZbb() &&
         ((KeptBits == 8 && XVT == MVT::i64 && !Subtarget.is64Bit()) ||
          KeptBits == 16);
}

bool YSXTargetLowering::isDesirableToCommuteWithShift(
    const SDNode *N, CombineLevel Level) const {
  assert((N->getOpcode() == ISD::SHL || N->getOpcode() == ISD::SRA ||
          N->getOpcode() == ISD::SRL) &&
         "Expected shift op");

  // The following folds are only desirable if `(OP _, c1 << c2)` can be
  // materialised in fewer instructions than `(OP _, c1)`:
  //
  //   (shl (add x, c1), c2) -> (add (shl x, c2), c1 << c2)
  //   (shl (or x, c1), c2) -> (or (shl x, c2), c1 << c2)
  SDValue N0 = N->getOperand(0);
  EVT Ty = N0.getValueType();

  // LD/ST will optimize constant Offset extraction, so when AddNode is used by
  // LD/ST, it can still complete the folding optimization operation performed
  // above.
  auto isUsedByLdSt = [](const SDNode *X, const SDNode *User) {
    for (SDNode *Use : X->users()) {
      // This use is the one we're on right now. Skip it
      if (Use == User || Use->getOpcode() == ISD::SELECT)
        continue;
      if (!isa<StoreSDNode>(Use) && !isa<LoadSDNode>(Use))
        return false;
    }
    return true;
  };

  if (Ty.isScalarInteger() &&
      (N0.getOpcode() == ISD::ADD || N0.getOpcode() == ISD::OR)) {
    if (N0.getOpcode() == ISD::ADD && !N0->hasOneUse())
      return isUsedByLdSt(N0.getNode(), N);

    auto *C1 = dyn_cast<ConstantSDNode>(N0->getOperand(1));
    auto *C2 = dyn_cast<ConstantSDNode>(N->getOperand(1));

    // Bail if we might break a sh{1,2,3}add/qc.shladd pattern.
    if (C2 && Subtarget.hasShlAdd(C2->getZExtValue()) && N->hasOneUse() &&
        N->user_begin()->getOpcode() == ISD::ADD &&
        !isUsedByLdSt(*N->user_begin(), nullptr) &&
        !isa<ConstantSDNode>(N->user_begin()->getOperand(1)))
      return false;

    if (C1 && C2) {
      const APInt &C1Int = C1->getAPIntValue();
      APInt ShiftedC1Int = C1Int << C2->getAPIntValue();

      // We can materialise `c1 << c2` into an add immediate, so it's "free",
      // and the combine should happen, to potentially allow further combines
      // later.
      if (ShiftedC1Int.getSignificantBits() <= 64 &&
          isLegalAddImmediate(ShiftedC1Int.getSExtValue()))
        return true;

      // We can materialise `c1` in an add immediate, so it's "free", and the
      // combine should be prevented.
      if (C1Int.getSignificantBits() <= 64 &&
          isLegalAddImmediate(C1Int.getSExtValue()))
        return false;

      // Neither constant will fit into an immediate, so find materialisation
      // costs.
      int C1Cost =
          YSXMatInt::getIntMatCost(C1Int, Ty.getSizeInBits(), Subtarget,
                                     /*CompressionCost*/ true);
      int ShiftedC1Cost = YSXMatInt::getIntMatCost(
          ShiftedC1Int, Ty.getSizeInBits(), Subtarget,
          /*CompressionCost*/ true);

      // Materialising `c1` is cheaper than materialising `c1 << c2`, so the
      // combine should be prevented.
      if (C1Cost < ShiftedC1Cost)
        return false;
    }
  }

  if (!N0->hasOneUse())
    return false;

  if (N0->getOpcode() == ISD::SIGN_EXTEND &&
      N0->getOperand(0)->getOpcode() == ISD::ADD &&
      !N0->getOperand(0)->hasOneUse())
    return isUsedByLdSt(N0->getOperand(0).getNode(), N0.getNode());

  return true;
}

bool YSXTargetLowering::targetShrinkDemandedConstant(
    SDValue Op, const APInt &DemandedBits, const APInt &DemandedElts,
    TargetLoweringOpt &TLO) const {
  // Delay this optimization as late as possible.
  if (!TLO.LegalOps)
    return false;

  EVT VT = Op.getValueType();
  if (VT.isVector())
    return false;

  unsigned Opcode = Op.getOpcode();
  if (Opcode != ISD::AND && Opcode != ISD::OR && Opcode != ISD::XOR)
    return false;

  ConstantSDNode *C = dyn_cast<ConstantSDNode>(Op.getOperand(1));
  if (!C)
    return false;

  const APInt &Mask = C->getAPIntValue();

  // Clear all non-demanded bits initially.
  APInt ShrunkMask = Mask & DemandedBits;

  // Try to make a smaller immediate by setting undemanded bits.

  APInt ExpandedMask = Mask | ~DemandedBits;

  auto IsLegalMask = [ShrunkMask, ExpandedMask](const APInt &Mask) -> bool {
    return ShrunkMask.isSubsetOf(Mask) && Mask.isSubsetOf(ExpandedMask);
  };
  auto UseMask = [Mask, Op, &TLO](const APInt &NewMask) -> bool {
    if (NewMask == Mask)
      return true;
    SDLoc DL(Op);
    SDValue NewC = TLO.DAG.getConstant(NewMask, DL, Op.getValueType());
    SDValue NewOp = TLO.DAG.getNode(Op.getOpcode(), DL, Op.getValueType(),
                                    Op.getOperand(0), NewC);
    return TLO.CombineTo(Op, NewOp);
  };

  // If the shrunk mask fits in sign extended 12 bits, let the target
  // independent code apply it.
  if (ShrunkMask.isSignedIntN(12))
    return false;

  // And has a few special cases for zext.
  if (Opcode == ISD::AND) {
    // Preserve (and X, 0xffff), if zext.h exists use zext.h,
    // otherwise use SLLI + SRLI.
    APInt NewMask = APInt(Mask.getBitWidth(), 0xffff);
    if (IsLegalMask(NewMask))
      return UseMask(NewMask);

    // Try to preserve (and X, 0xffffffff), the (zext_inreg X, i32) pattern.
    if (VT == MVT::i64) {
      APInt NewMask = APInt(64, 0xffffffff);
      if (IsLegalMask(NewMask))
        return UseMask(NewMask);
    }
  }

  // For the remaining optimizations, we need to be able to make a negative
  // number through a combination of mask and undemanded bits.
  if (!ExpandedMask.isNegative())
    return false;

  // What is the fewest number of bits we need to represent the negative number.
  unsigned MinSignedBits = ExpandedMask.getSignificantBits();

  // Try to make a 12 bit negative immediate. If that fails try to make a 32
  // bit negative immediate unless the shrunk immediate already fits in 32 bits.
  // If we can't create a simm12, we shouldn't change opaque constants.
  APInt NewMask = ShrunkMask;
  if (MinSignedBits <= 12)
    NewMask.setBitsFrom(11);
  else if (!C->isOpaque() && MinSignedBits <= 32 && !ShrunkMask.isSignedIntN(32))
    NewMask.setBitsFrom(31);
  else
    return false;

  // Check that our new mask is a subset of the demanded mask.
  assert(IsLegalMask(NewMask));
  return UseMask(NewMask);
}

void YSXTargetLowering::computeKnownBitsForTargetNode(
    const SDValue Op, KnownBits &Known, const APInt &DemandedElts,
    const SelectionDAG &DAG, unsigned Depth) const {
  Known.resetAll();
  switch (Op.getOpcode()) {
  default:
    return;
  case YSXISD::SELECT_CC: {
    Known = DAG.computeKnownBits(Op.getOperand(4), Depth + 1);
    KnownBits Known2 = DAG.computeKnownBits(Op.getOperand(3), Depth + 1);
    Known = Known.intersectWith(Known2);
    return;
  }
  case YSXISD::REMUW:
  case YSXISD::DIVUW:
  case YSXISD::SLLW:
  case YSXISD::SRLW:
  case YSXISD::SRAW:
    return;
  }
}

unsigned YSXTargetLowering::ComputeNumSignBitsForTargetNode(
    SDValue Op, const APInt &DemandedElts, const SelectionDAG &DAG,
    unsigned Depth) const {
  switch (Op.getOpcode()) {
  default:
    return 1;
  case YSXISD::SELECT_CC: {
    unsigned Tmp =
        DAG.ComputeNumSignBits(Op.getOperand(3), DemandedElts, Depth + 1);
    if (Tmp == 1)
      return 1;
    unsigned Tmp2 =
        DAG.ComputeNumSignBits(Op.getOperand(4), DemandedElts, Depth + 1);
    return std::min(Tmp, Tmp2);
  }
  case YSXISD::SRAW:
    return std::max(
        DAG.ComputeNumSignBits(Op.getOperand(0), DemandedElts, Depth + 1),
        33U);
  case YSXISD::SLLW:
  case YSXISD::SRLW:
  case YSXISD::DIVW:
  case YSXISD::DIVUW:
  case YSXISD::REMUW:
    return 33;
  }
}

bool YSXTargetLowering::SimplifyDemandedBitsForTargetNode(
    SDValue Op, const APInt &OriginalDemandedBits,
    const APInt &OriginalDemandedElts, KnownBits &Known, TargetLoweringOpt &TLO,
    unsigned Depth) const {
  return TargetLowering::SimplifyDemandedBitsForTargetNode(
      Op, OriginalDemandedBits, OriginalDemandedElts, Known, TLO, Depth);
}

bool YSXTargetLowering::canCreateUndefOrPoisonForTargetNode(
    SDValue Op, const APInt &DemandedElts, const SelectionDAG &DAG,
    bool PoisonOnly, bool ConsiderFlags, unsigned Depth) const {
  switch (Op.getOpcode()) {
  case YSXISD::SLLW:
  case YSXISD::SRAW:
  case YSXISD::SRLW:
  case YSXISD::SELECT_CC:
    return false;
  default:
    return TargetLowering::canCreateUndefOrPoisonForTargetNode(
        Op, DemandedElts, DAG, PoisonOnly, ConsiderFlags, Depth);
  }
}

const Constant *
YSXTargetLowering::getTargetConstantFromLoad(LoadSDNode *Ld) const {
  assert(Ld && "Unexpected null LoadSDNode");
  if (!ISD::isNormalLoad(Ld))
    return nullptr;

  SDValue Ptr = Ld->getBasePtr();

  // Only constant pools with no offset are supported.
  auto GetSupportedConstantPool = [](SDValue Ptr) -> ConstantPoolSDNode * {
    auto *CNode = dyn_cast<ConstantPoolSDNode>(Ptr);
    if (!CNode || CNode->isMachineConstantPoolEntry() ||
        CNode->getOffset() != 0)
      return nullptr;

    return CNode;
  };

  // Simple case, LLA.
  if (Ptr.getOpcode() == YSXISD::LLA) {
    auto *CNode = GetSupportedConstantPool(Ptr.getOperand(0));
    if (!CNode || CNode->getTargetFlags() != 0)
      return nullptr;

    return CNode->getConstVal();
  }

  // Look for a HI and ADD_LO pair.
  if (Ptr.getOpcode() != YSXISD::ADD_LO ||
      Ptr.getOperand(0).getOpcode() != YSXISD::HI)
    return nullptr;

  auto *CNodeLo = GetSupportedConstantPool(Ptr.getOperand(1));
  auto *CNodeHi = GetSupportedConstantPool(Ptr.getOperand(0).getOperand(0));

  if (!CNodeLo || CNodeLo->getTargetFlags() != YSXII::MO_LO ||
      !CNodeHi || CNodeHi->getTargetFlags() != YSXII::MO_HI)
    return nullptr;

  if (CNodeLo->getConstVal() != CNodeHi->getConstVal())
    return nullptr;

  return CNodeLo->getConstVal();
}


static MachineBasicBlock *
EmitLoweredCascadedSelect(MachineInstr &First, MachineInstr &Second,
                          MachineBasicBlock *ThisMBB,
                          const YSXSubtarget &Subtarget) {
  // Select_FPRX_ (rs1, rs2, imm, rs4, (Select_FPRX_ rs1, rs2, imm, rs4, rs5)
  // Without this, custom-inserter would have generated:
  //
  //   A
  //   | \
  //   |  B
  //   | /
  //   C
  //   | \
  //   |  D
  //   | /
  //   E
  //
  // A: X = ...; Y = ...
  // B: empty
  // C: Z = PHI [X, A], [Y, B]
  // D: empty
  // E: PHI [X, C], [Z, D]
  //
  // If we lower both Select_FPRX_ in a single step, we can instead generate:
  //
  //   A
  //   | \
  //   |  C
  //   | /|
  //   |/ |
  //   |  |
  //   |  D
  //   | /
  //   E
  //
  // A: X = ...; Y = ...
  // D: empty
  // E: PHI [X, A], [X, C], [Y, D]

  const YSXInstrInfo &TII = *Subtarget.getInstrInfo();
  const DebugLoc &DL = First.getDebugLoc();
  const BasicBlock *LLVM_BB = ThisMBB->getBasicBlock();
  MachineFunction *F = ThisMBB->getParent();
  MachineBasicBlock *FirstMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SecondMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *SinkMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineFunction::iterator It = ++ThisMBB->getIterator();
  F->insert(It, FirstMBB);
  F->insert(It, SecondMBB);
  F->insert(It, SinkMBB);

  // Transfer the remainder of ThisMBB and its successor edges to SinkMBB.
  SinkMBB->splice(SinkMBB->begin(), ThisMBB,
                  std::next(MachineBasicBlock::iterator(First)),
                  ThisMBB->end());
  SinkMBB->transferSuccessorsAndUpdatePHIs(ThisMBB);

  // Fallthrough block for ThisMBB.
  ThisMBB->addSuccessor(FirstMBB);
  // Fallthrough block for FirstMBB.
  FirstMBB->addSuccessor(SecondMBB);
  ThisMBB->addSuccessor(SinkMBB);
  FirstMBB->addSuccessor(SinkMBB);
  // This is fallthrough.
  SecondMBB->addSuccessor(SinkMBB);

  auto FirstCC = static_cast<YSXCC::CondCode>(First.getOperand(3).getImm());
  Register FLHS = First.getOperand(1).getReg();
  Register FRHS = First.getOperand(2).getReg();
  // Insert appropriate branch.
  BuildMI(FirstMBB, DL, TII.get(YSXCC::getBrCond(FirstCC, First.getOpcode())))
      .addReg(FLHS)
      .addReg(FRHS)
      .addMBB(SinkMBB);

  Register SLHS = Second.getOperand(1).getReg();
  Register SRHS = Second.getOperand(2).getReg();
  Register Op1Reg4 = First.getOperand(4).getReg();
  Register Op1Reg5 = First.getOperand(5).getReg();

  auto SecondCC = static_cast<YSXCC::CondCode>(Second.getOperand(3).getImm());
  // Insert appropriate branch.
  BuildMI(ThisMBB, DL,
          TII.get(YSXCC::getBrCond(SecondCC, Second.getOpcode())))
      .addReg(SLHS)
      .addReg(SRHS)
      .addMBB(SinkMBB);

  Register DestReg = Second.getOperand(0).getReg();
  Register Op2Reg4 = Second.getOperand(4).getReg();
  BuildMI(*SinkMBB, SinkMBB->begin(), DL, TII.get(YSX::PHI), DestReg)
      .addReg(Op2Reg4)
      .addMBB(ThisMBB)
      .addReg(Op1Reg4)
      .addMBB(FirstMBB)
      .addReg(Op1Reg5)
      .addMBB(SecondMBB);

  // Now remove the Select_FPRX_s.
  First.eraseFromParent();
  Second.eraseFromParent();
  return SinkMBB;
}

static MachineBasicBlock *emitSelectPseudo(MachineInstr &MI,
                                           MachineBasicBlock *BB,
                                           const YSXSubtarget &Subtarget) {
  // To "insert" Select_* instructions, we actually have to insert the triangle
  // control-flow pattern.  The incoming instructions know the destination vreg
  // to set, the condition code register to branch on, the true/false values to
  // select between, and the condcode to use to select the appropriate branch.
  //
  // We produce the following control flow:
  //     HeadMBB
  //     |  \
  //     |  IfFalseMBB
  //     | /
  //    TailMBB
  //
  // When we find a sequence of selects we attempt to optimize their emission
  // by sharing the control flow. Currently we only handle cases where we have
  // multiple selects with the exact same condition (same LHS, RHS and CC).
  // The selects may be interleaved with other instructions if the other
  // instructions meet some requirements we deem safe:
  // - They are not pseudo instructions.
  // - They are debug instructions. Otherwise,
  // - They do not have side-effects, do not access memory and their inputs do
  //   not depend on the results of the select pseudo-instructions.
  // - They don't adjust stack.
  // The TrueV/FalseV operands of the selects cannot depend on the result of
  // previous selects in the sequence.
  // These conditions could be further relaxed. See the X86 target for a
  // related approach and more information.
  //
  // Select_FPRX_ (rs1, rs2, imm, rs4, (Select_FPRX_ rs1, rs2, imm, rs4, rs5))
  // is checked here and handled by a separate function -
  // EmitLoweredCascadedSelect.

  auto Next = next_nodbg(MI.getIterator(), BB->instr_end());
  if (MI.getOpcode() != YSX::Select_GPR_Using_CC_GPR &&
      MI.getOperand(1).isReg() && MI.getOperand(2).isReg() &&
      Next != BB->end() && Next->getOpcode() == MI.getOpcode() &&
      Next->getOperand(5).getReg() == MI.getOperand(0).getReg() &&
      Next->getOperand(5).isKill())
    return EmitLoweredCascadedSelect(MI, *Next, BB, Subtarget);

  Register LHS = MI.getOperand(1).getReg();
  Register RHS;
  if (MI.getOperand(2).isReg())
    RHS = MI.getOperand(2).getReg();
  auto CC = static_cast<YSXCC::CondCode>(MI.getOperand(3).getImm());

  SmallVector<MachineInstr *, 4> SelectDebugValues;
  SmallSet<Register, 4> SelectDests;
  SelectDests.insert(MI.getOperand(0).getReg());

  MachineInstr *LastSelectPseudo = &MI;
  const YSXInstrInfo &TII = *Subtarget.getInstrInfo();

  for (auto E = BB->end(), SequenceMBBI = MachineBasicBlock::iterator(MI);
       SequenceMBBI != E; ++SequenceMBBI) {
    if (SequenceMBBI->isDebugInstr())
      continue;
    if (YSXInstrInfo::isSelectPseudo(*SequenceMBBI)) {
      if (SequenceMBBI->getOperand(1).getReg() != LHS ||
          !SequenceMBBI->getOperand(2).isReg() ||
          SequenceMBBI->getOperand(2).getReg() != RHS ||
          SequenceMBBI->getOperand(3).getImm() != CC ||
          SelectDests.count(SequenceMBBI->getOperand(4).getReg()) ||
          SelectDests.count(SequenceMBBI->getOperand(5).getReg()))
        break;
      LastSelectPseudo = &*SequenceMBBI;
      SequenceMBBI->collectDebugValues(SelectDebugValues);
      SelectDests.insert(SequenceMBBI->getOperand(0).getReg());
      continue;
    }
    if (SequenceMBBI->hasUnmodeledSideEffects() ||
        SequenceMBBI->mayLoadOrStore() ||
        SequenceMBBI->usesCustomInsertionHook() ||
        TII.isFrameInstr(*SequenceMBBI) ||
        SequenceMBBI->isStackAligningInlineAsm())
      break;
    if (llvm::any_of(SequenceMBBI->operands(), [&](MachineOperand &MO) {
          return MO.isReg() && MO.isUse() && SelectDests.count(MO.getReg());
        }))
      break;
  }

  const BasicBlock *LLVM_BB = BB->getBasicBlock();
  DebugLoc DL = MI.getDebugLoc();
  MachineFunction::iterator I = ++BB->getIterator();

  MachineBasicBlock *HeadMBB = BB;
  MachineFunction *F = BB->getParent();
  MachineBasicBlock *TailMBB = F->CreateMachineBasicBlock(LLVM_BB);
  MachineBasicBlock *IfFalseMBB = F->CreateMachineBasicBlock(LLVM_BB);

  F->insert(I, IfFalseMBB);
  F->insert(I, TailMBB);

  // Set the call frame size on entry to the new basic blocks.
  unsigned CallFrameSize = TII.getCallFrameSizeAt(*LastSelectPseudo);
  IfFalseMBB->setCallFrameSize(CallFrameSize);
  TailMBB->setCallFrameSize(CallFrameSize);

  // Transfer debug instructions associated with the selects to TailMBB.
  for (MachineInstr *DebugInstr : SelectDebugValues) {
    TailMBB->push_back(DebugInstr->removeFromParent());
  }

  // Move all instructions after the sequence to TailMBB.
  TailMBB->splice(TailMBB->end(), HeadMBB,
                  std::next(LastSelectPseudo->getIterator()), HeadMBB->end());
  // Update machine-CFG edges by transferring all successors of the current
  // block to the new block which will contain the Phi nodes for the selects.
  TailMBB->transferSuccessorsAndUpdatePHIs(HeadMBB);
  // Set the successors for HeadMBB.
  HeadMBB->addSuccessor(IfFalseMBB);
  HeadMBB->addSuccessor(TailMBB);

  // Insert appropriate branch.
  if (MI.getOperand(2).isImm())
    BuildMI(HeadMBB, DL, TII.get(YSXCC::getBrCond(CC, MI.getOpcode())))
        .addReg(LHS)
        .addImm(MI.getOperand(2).getImm())
        .addMBB(TailMBB);
  else
    BuildMI(HeadMBB, DL, TII.get(YSXCC::getBrCond(CC, MI.getOpcode())))
        .addReg(LHS)
        .addReg(RHS)
        .addMBB(TailMBB);

  // IfFalseMBB just falls through to TailMBB.
  IfFalseMBB->addSuccessor(TailMBB);

  // Create PHIs for all of the select pseudo-instructions.
  auto SelectMBBI = MI.getIterator();
  auto SelectEnd = std::next(LastSelectPseudo->getIterator());
  auto InsertionPoint = TailMBB->begin();
  while (SelectMBBI != SelectEnd) {
    auto Next = std::next(SelectMBBI);
    if (YSXInstrInfo::isSelectPseudo(*SelectMBBI)) {
      // %Result = phi [ %TrueValue, HeadMBB ], [ %FalseValue, IfFalseMBB ]
      BuildMI(*TailMBB, InsertionPoint, SelectMBBI->getDebugLoc(),
              TII.get(YSX::PHI), SelectMBBI->getOperand(0).getReg())
          .addReg(SelectMBBI->getOperand(4).getReg())
          .addMBB(HeadMBB)
          .addReg(SelectMBBI->getOperand(5).getReg())
          .addMBB(IfFalseMBB);
      SelectMBBI->eraseFromParent();
    }
    SelectMBBI = Next;
  }

  F->getProperties().resetNoPHIs();
  return TailMBB;
}


MachineBasicBlock *
YSXTargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                                 MachineBasicBlock *BB) const {
  switch (MI.getOpcode()) {
  default:
    llvm_unreachable("Unexpected instr type to insert");
  case YSX::Select_GPR_Using_CC_GPR:
    return emitSelectPseudo(MI, BB, Subtarget);
  case YSX::PROBED_STACKALLOC_DYN:
    return emitDynamicProbedAlloc(MI, BB);
  case TargetOpcode::STATEPOINT:
    // STATEPOINT is a pseudo instruction which has no implicit defs/uses
    // while jal call instruction (where statepoint will be lowered at the end)
    // has implicit def. This def is early-clobber as it will be set at
    // the moment of the call and earlier than any use is read.
    // Add this implicit dead def here as a workaround.
    MI.addOperand(*MI.getMF(),
                  MachineOperand::CreateReg(
                      YSX::X1, /*isDef*/ true,
                      /*isImp*/ true, /*isKill*/ false, /*isDead*/ true,
                      /*isUndef*/ false, /*isEarlyClobber*/ true));
    [[fallthrough]];
  case TargetOpcode::STACKMAP:
  case TargetOpcode::PATCHPOINT:
    if (!Subtarget.is64Bit())
      reportFatalUsageError("STACKMAP, PATCHPOINT and STATEPOINT are only "
                            "supported on 64-bit targets");
    return emitPatchPoint(MI, BB);
  }
}

void YSXTargetLowering::AdjustInstrPostInstrSelection(MachineInstr &MI,
                                                        SDNode *Node) const {
  return;
}

void YSXTargetLowering::analyzeInputArgs(
    MachineFunction &MF, CCState &CCInfo,
    const SmallVectorImpl<ISD::InputArg> &Ins, bool IsRet,
    YSXCCAssignFn Fn) const {
  for (const auto &[Idx, In] : enumerate(Ins)) {
    MVT ArgVT = In.VT;
    ISD::ArgFlagsTy ArgFlags = In.Flags;

    if (Fn(Idx, ArgVT, ArgVT, CCValAssign::Full, ArgFlags, CCInfo, IsRet,
           In.OrigTy)) {
      LLVM_DEBUG(dbgs() << "InputArg #" << Idx << " has unhandled type "
                        << ArgVT << '\n');
      llvm_unreachable(nullptr);
    }
  }
}

void YSXTargetLowering::analyzeOutputArgs(
    MachineFunction &MF, CCState &CCInfo,
    const SmallVectorImpl<ISD::OutputArg> &Outs, bool IsRet,
    CallLoweringInfo *CLI, YSXCCAssignFn Fn) const {
  for (const auto &[Idx, Out] : enumerate(Outs)) {
    MVT ArgVT = Out.VT;
    ISD::ArgFlagsTy ArgFlags = Out.Flags;

    if (Fn(Idx, ArgVT, ArgVT, CCValAssign::Full, ArgFlags, CCInfo, IsRet,
           Out.OrigTy)) {
      LLVM_DEBUG(dbgs() << "OutputArg #" << Idx << " has unhandled type "
                        << ArgVT << "\n");
      llvm_unreachable(nullptr);
    }
  }
}

// Convert Val to a ValVT. Should not be called for CCValAssign::Indirect
// values.
static SDValue convertLocVTToValVT(SelectionDAG &DAG, SDValue Val,
                                   const CCValAssign &VA, const SDLoc &DL,
                                   const YSXSubtarget &Subtarget) {
  if (VA.needsCustom())
    llvm_unreachable("Unexpected Custom handling.");

  switch (VA.getLocInfo()) {
  default:
    llvm_unreachable("Unexpected CCValAssign::LocInfo");
  case CCValAssign::Full:
    break;
  case CCValAssign::BCvt:
    Val = DAG.getNode(ISD::BITCAST, DL, VA.getValVT(), Val);
    break;
  }
  return Val;
}

// The caller is responsible for loading the full value if the argument is
// passed with CCValAssign::Indirect.
static SDValue unpackFromRegLoc(SelectionDAG &DAG, SDValue Chain,
                                const CCValAssign &VA, const SDLoc &DL,
                                const ISD::InputArg &In,
                                const YSXTargetLowering &TLI) {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();
  EVT LocVT = VA.getLocVT();
  SDValue Val;
  const TargetRegisterClass *RC = TLI.getRegClassFor(LocVT.getSimpleVT());
  Register VReg = RegInfo.createVirtualRegister(RC);
  RegInfo.addLiveIn(VA.getLocReg(), VReg);
  Val = DAG.getCopyFromReg(Chain, DL, VReg, LocVT);

  // If input is sign extended from 32 bits, note it for the SExtWRemoval pass.
  if (In.isOrigArg()) {
    Argument *OrigArg = MF.getFunction().getArg(In.getOrigArgIndex());
    if (OrigArg->getType()->isIntegerTy()) {
      unsigned BitWidth = OrigArg->getType()->getIntegerBitWidth();
      // An input zero extended from i31 can also be considered sign extended.
      if ((BitWidth <= 32 && In.Flags.isSExt()) ||
          (BitWidth < 32 && In.Flags.isZExt())) {
        YSXMachineFunctionInfo *RVFI = MF.getInfo<YSXMachineFunctionInfo>();
        RVFI->addSExt32Register(VReg);
      }
    }
  }

  if (VA.getLocInfo() == CCValAssign::Indirect)
    return Val;

  return convertLocVTToValVT(DAG, Val, VA, DL, TLI.getSubtarget());
}

static SDValue convertValVTToLocVT(SelectionDAG &DAG, SDValue Val,
                                   const CCValAssign &VA, const SDLoc &DL,
                                   const YSXSubtarget &Subtarget) {
  EVT LocVT = VA.getLocVT();

  if (VA.needsCustom())
    llvm_unreachable("Unexpected Custom handling.");

  switch (VA.getLocInfo()) {
  default:
    llvm_unreachable("Unexpected CCValAssign::LocInfo");
  case CCValAssign::Full:
    break;
  case CCValAssign::BCvt:
    Val = DAG.getNode(ISD::BITCAST, DL, LocVT, Val);
    break;
  }
  return Val;
}

// The caller is responsible for loading the full value if the argument is
// passed with CCValAssign::Indirect.
static SDValue unpackFromMemLoc(SelectionDAG &DAG, SDValue Chain,
                                const CCValAssign &VA, const SDLoc &DL) {
  MachineFunction &MF = DAG.getMachineFunction();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  EVT LocVT = VA.getLocVT();
  EVT ValVT = VA.getValVT();
  EVT PtrVT = MVT::getIntegerVT(DAG.getDataLayout().getPointerSizeInBits(0));
  if (VA.getLocInfo() == CCValAssign::Indirect) {
    // When the value is a scalable vector, we save the pointer which points to
    // the scalable vector value in the stack. The ValVT will be the pointer
    // type, instead of the scalable vector type.
    ValVT = LocVT;
  }
  int FI = MFI.CreateFixedObject(ValVT.getStoreSize(), VA.getLocMemOffset(),
                                 /*IsImmutable=*/true);
  SDValue FIN = DAG.getFrameIndex(FI, PtrVT);
  SDValue Val;

  ISD::LoadExtType ExtType = ISD::NON_EXTLOAD;
  switch (VA.getLocInfo()) {
  default:
    llvm_unreachable("Unexpected CCValAssign::LocInfo");
  case CCValAssign::Full:
  case CCValAssign::Indirect:
  case CCValAssign::BCvt:
    break;
  }
  Val = DAG.getExtLoad(
      ExtType, DL, LocVT, Chain, FIN,
      MachinePointerInfo::getFixedStack(DAG.getMachineFunction(), FI), ValVT);
  return Val;
}

// Transform physical registers into virtual registers.
SDValue YSXTargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

  MachineFunction &MF = DAG.getMachineFunction();

  switch (CallConv) {
  default:
    reportFatalUsageError("Unsupported calling convention");
  case CallingConv::C:
  case CallingConv::Fast:
  case CallingConv::SPIR_KERNEL:
  case CallingConv::PreserveMost:
  case CallingConv::GRAAL:
  case CallingConv::RISCV_VectorCall:
#define CC_VLS_CASE(ABI_VLEN) case CallingConv::RISCV_VLSCall_##ABI_VLEN:
    CC_VLS_CASE(32)
    CC_VLS_CASE(64)
    CC_VLS_CASE(128)
    CC_VLS_CASE(256)
    CC_VLS_CASE(512)
    CC_VLS_CASE(1024)
    CC_VLS_CASE(2048)
    CC_VLS_CASE(4096)
    CC_VLS_CASE(8192)
    CC_VLS_CASE(16384)
    CC_VLS_CASE(32768)
    CC_VLS_CASE(65536)
#undef CC_VLS_CASE
    break;
  case CallingConv::GHC:
    if (Subtarget.hasStdExtE())
      reportFatalUsageError("GHC calling convention is not supported on RVE!");
    if (!Subtarget.hasStdExtFOrZfinx() || !Subtarget.hasStdExtDOrZdinx())
      reportFatalUsageError("GHC calling convention requires the (Zfinx/F) and "
                            "(Zdinx/D) instruction set extensions");
  }

  const Function &Func = MF.getFunction();
  if (Func.hasFnAttribute("interrupt")) {
    reportFatalUsageError(
        "YSX rv64ima does not support interrupt handlers");
  }

  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  MVT XLenVT = Subtarget.getXLenVT();
  unsigned XLenInBytes = Subtarget.getXLen() / 8;
  // Used with vargs to accumulate store chains.
  std::vector<SDValue> OutChains;

  // Assign locations to all of the incoming arguments.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());

  if (CallConv == CallingConv::GHC)
    CCInfo.AnalyzeFormalArguments(Ins, CC_YSX_GHC);
  else
    analyzeInputArgs(MF, CCInfo, Ins, /*IsRet=*/false,
                     CallConv == CallingConv::Fast ? CC_YSX_FastCC
                                                   : CC_YSX);

  for (unsigned i = 0, e = ArgLocs.size(), InsIdx = 0; i != e; ++i, ++InsIdx) {
    CCValAssign &VA = ArgLocs[i];
    SDValue ArgValue;
    if (VA.isRegLoc())
      ArgValue = unpackFromRegLoc(DAG, Chain, VA, DL, Ins[InsIdx], *this);
    else
      ArgValue = unpackFromMemLoc(DAG, Chain, VA, DL);

    if (VA.getLocInfo() == CCValAssign::Indirect) {
      // If the original argument was split and passed by reference (e.g. i128
      // on RV32), we need to load all parts of it here (using the same
      // address). Vectors may be partly split to registers and partly to the
      // stack, in which case the base address is partly offset and subsequent
      // stores are relative to that.
      InVals.push_back(DAG.getLoad(VA.getValVT(), DL, Chain, ArgValue,
                                   MachinePointerInfo()));
      unsigned ArgIndex = Ins[InsIdx].OrigArgIndex;
      unsigned ArgPartOffset = Ins[InsIdx].PartOffset;
      assert(VA.getValVT().isVector() || ArgPartOffset == 0);
      while (i + 1 != e && Ins[InsIdx + 1].OrigArgIndex == ArgIndex) {
        CCValAssign &PartVA = ArgLocs[i + 1];
        unsigned PartOffset = Ins[InsIdx + 1].PartOffset - ArgPartOffset;
        SDValue Offset = DAG.getIntPtrConstant(PartOffset, DL);
        if (PartVA.getValVT().isScalableVector())
          Offset = DAG.getNode(ISD::VSCALE, DL, XLenVT, Offset);
        SDValue Address = DAG.getNode(ISD::ADD, DL, PtrVT, ArgValue, Offset);
        InVals.push_back(DAG.getLoad(PartVA.getValVT(), DL, Chain, Address,
                                     MachinePointerInfo()));
        ++i;
        ++InsIdx;
      }
      continue;
    }
    InVals.push_back(ArgValue);
  }

  if (any_of(ArgLocs,
             [](CCValAssign &VA) { return VA.getLocVT().isScalableVector(); }))
    MF.getInfo<YSXMachineFunctionInfo>()->setIsVectorCall();

  if (IsVarArg) {
    ArrayRef<MCPhysReg> ArgRegs = YSX::getArgGPRs(Subtarget.getTargetABI());
    unsigned Idx = CCInfo.getFirstUnallocated(ArgRegs);
    const TargetRegisterClass *RC = &YSX::GPRRegClass;
    MachineFrameInfo &MFI = MF.getFrameInfo();
    MachineRegisterInfo &RegInfo = MF.getRegInfo();
    YSXMachineFunctionInfo *RVFI = MF.getInfo<YSXMachineFunctionInfo>();

    // Size of the vararg save area. For now, the varargs save area is either
    // zero or large enough to hold a0-a7.
    int VarArgsSaveSize = XLenInBytes * (ArgRegs.size() - Idx);
    int FI;

    // If all registers are allocated, then all varargs must be passed on the
    // stack and we don't need to save any argregs.
    if (VarArgsSaveSize == 0) {
      int VaArgOffset = CCInfo.getStackSize();
      FI = MFI.CreateFixedObject(XLenInBytes, VaArgOffset, true);
    } else {
      int VaArgOffset = -VarArgsSaveSize;
      FI = MFI.CreateFixedObject(VarArgsSaveSize, VaArgOffset, true);

      // If saving an odd number of registers then create an extra stack slot to
      // ensure that the frame pointer is 2*XLEN-aligned, which in turn ensures
      // offsets to even-numbered registered remain 2*XLEN-aligned.
      if (Idx % 2) {
        MFI.CreateFixedObject(
            XLenInBytes, VaArgOffset - static_cast<int>(XLenInBytes), true);
        VarArgsSaveSize += XLenInBytes;
      }

      SDValue FIN = DAG.getFrameIndex(FI, PtrVT);

      // Copy the integer registers that may have been used for passing varargs
      // to the vararg save area.
      for (unsigned I = Idx; I < ArgRegs.size(); ++I) {
        const Register Reg = RegInfo.createVirtualRegister(RC);
        RegInfo.addLiveIn(ArgRegs[I], Reg);
        SDValue ArgValue = DAG.getCopyFromReg(Chain, DL, Reg, XLenVT);
        SDValue Store = DAG.getStore(
            Chain, DL, ArgValue, FIN,
            MachinePointerInfo::getFixedStack(MF, FI, (I - Idx) * XLenInBytes));
        OutChains.push_back(Store);
        FIN =
            DAG.getMemBasePlusOffset(FIN, TypeSize::getFixed(XLenInBytes), DL);
      }
    }

    // Record the frame index of the first variable argument
    // which is a value necessary to VASTART.
    RVFI->setVarArgsFrameIndex(FI);
    RVFI->setVarArgsSaveSize(VarArgsSaveSize);
  }

  // All stores are grouped in one node to allow the matching between
  // the size of Ins and InVals. This only happens for vararg functions.
  if (!OutChains.empty()) {
    OutChains.push_back(Chain);
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, OutChains);
  }

  return Chain;
}

/// isEligibleForTailCallOptimization - Check whether the call is eligible
/// for tail call optimization.
/// Note: This is modelled after ARM's IsEligibleForTailCallOptimization.
bool YSXTargetLowering::isEligibleForTailCallOptimization(
    CCState &CCInfo, CallLoweringInfo &CLI, MachineFunction &MF,
    const SmallVector<CCValAssign, 16> &ArgLocs) const {

  auto CalleeCC = CLI.CallConv;
  auto &Outs = CLI.Outs;
  auto &Caller = MF.getFunction();
  auto CallerCC = Caller.getCallingConv();

  // Exception-handling functions need a special set of instructions to
  // indicate a return to the hardware. Tail-calling another function would
  // probably break this.
  // TODO: The "interrupt" attribute isn't currently defined by RISC-V. This
  // should be expanded as new function attributes are introduced.
  if (Caller.hasFnAttribute("interrupt"))
    return false;

  // Do not tail call opt if the stack is used to pass parameters.
  if (CCInfo.getStackSize() != 0)
    return false;

  // Do not tail call opt if any parameters need to be passed indirectly.
  // Since long doubles (fp128) and i128 are larger than 2*XLEN, they are
  // passed indirectly. So the address of the value will be passed in a
  // register, or if not available, then the address is put on the stack. In
  // order to pass indirectly, space on the stack often needs to be allocated
  // in order to store the value. In this case the CCInfo.getNextStackOffset()
  // != 0 check is not enough and we need to check if any CCValAssign ArgsLocs
  // are passed CCValAssign::Indirect.
  for (auto &VA : ArgLocs)
    if (VA.getLocInfo() == CCValAssign::Indirect)
      return false;

  // Do not tail call opt if either caller or callee uses struct return
  // semantics.
  auto IsCallerStructRet = Caller.hasStructRetAttr();
  auto IsCalleeStructRet = Outs.empty() ? false : Outs[0].Flags.isSRet();
  if (IsCallerStructRet || IsCalleeStructRet)
    return false;

  // The callee has to preserve all registers the caller needs to preserve.
  const YSXRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *CallerPreserved = TRI->getCallPreservedMask(MF, CallerCC);
  if (CalleeCC != CallerCC) {
    const uint32_t *CalleePreserved = TRI->getCallPreservedMask(MF, CalleeCC);
    if (!TRI->regmaskSubsetEqual(CallerPreserved, CalleePreserved))
      return false;
  }

  // Byval parameters hand the function a pointer directly into the stack area
  // we want to reuse during a tail call. Working around this *is* possible
  // but less efficient and uglier in LowerCall.
  for (auto &Arg : Outs)
    if (Arg.Flags.isByVal())
      return false;

  return true;
}

static Align getPrefTypeAlign(EVT VT, SelectionDAG &DAG) {
  return DAG.getDataLayout().getPrefTypeAlign(
      VT.getTypeForEVT(*DAG.getContext()));
}

// Lower a call to a callseq_start + CALL + callseq_end chain, and add input
// and output parameter nodes.
SDValue YSXTargetLowering::LowerCall(CallLoweringInfo &CLI,
                                       SmallVectorImpl<SDValue> &InVals) const {
  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  bool &IsTailCall = CLI.IsTailCall;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;
  EVT PtrVT = getPointerTy(DAG.getDataLayout());
  MVT XLenVT = Subtarget.getXLenVT();
  const CallBase *CB = CLI.CB;

  MachineFunction &MF = DAG.getMachineFunction();
  MachineFunction::CallSiteInfo CSInfo;

  // Set type id for call site info.
  if (MF.getTarget().Options.EmitCallGraphSection && CB && CB->isIndirectCall())
    CSInfo = MachineFunction::CallSiteInfo(*CB);

  // Analyze the operands of the call, assigning locations to each operand.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState ArgCCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());

  if (CallConv == CallingConv::GHC) {
    if (Subtarget.hasStdExtE())
      reportFatalUsageError("GHC calling convention is not supported on RVE!");
    ArgCCInfo.AnalyzeCallOperands(Outs, CC_YSX_GHC);
  } else
    analyzeOutputArgs(MF, ArgCCInfo, Outs, /*IsRet=*/false, &CLI,
                      CallConv == CallingConv::Fast ? CC_YSX_FastCC
                                                    : CC_YSX);

  // Check if it's really possible to do a tail call.
  if (IsTailCall)
    IsTailCall = isEligibleForTailCallOptimization(ArgCCInfo, CLI, MF, ArgLocs);

  if (IsTailCall)
    ++NumTailCalls;
  else if (CLI.CB && CLI.CB->isMustTailCall())
    reportFatalInternalError("failed to perform tail call elimination on a "
                             "call site marked musttail");

  // Get a count of how many bytes are to be pushed on the stack.
  unsigned NumBytes = ArgCCInfo.getStackSize();

  // Create local copies for byval args
  SmallVector<SDValue, 8> ByValArgs;
  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    ISD::ArgFlagsTy Flags = Outs[i].Flags;
    if (!Flags.isByVal())
      continue;

    SDValue Arg = OutVals[i];
    unsigned Size = Flags.getByValSize();
    Align Alignment = Flags.getNonZeroByValAlign();

    int FI =
        MF.getFrameInfo().CreateStackObject(Size, Alignment, /*isSS=*/false);
    SDValue FIPtr = DAG.getFrameIndex(FI, getPointerTy(DAG.getDataLayout()));
    SDValue SizeNode = DAG.getConstant(Size, DL, XLenVT);

    Chain = DAG.getMemcpy(Chain, DL, FIPtr, Arg, SizeNode, Alignment,
                          /*IsVolatile=*/false,
                          /*AlwaysInline=*/false, /*CI*/ nullptr, IsTailCall,
                          MachinePointerInfo(), MachinePointerInfo());
    ByValArgs.push_back(FIPtr);
  }

  if (!IsTailCall)
    Chain = DAG.getCALLSEQ_START(Chain, NumBytes, 0, CLI.DL);

  // Copy argument values to their designated locations.
  SmallVector<std::pair<Register, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;
  SDValue StackPtr;
  for (unsigned i = 0, j = 0, e = ArgLocs.size(), OutIdx = 0; i != e;
       ++i, ++OutIdx) {
    CCValAssign &VA = ArgLocs[i];
    SDValue ArgValue = OutVals[OutIdx];
    ISD::ArgFlagsTy Flags = Outs[OutIdx].Flags;

    // Promote the value if needed.
    // For now, only handle fully promoted and indirect arguments.
    if (VA.getLocInfo() == CCValAssign::Indirect) {
      // Store the argument in a stack slot and pass its address.
      Align StackAlign =
          std::max(getPrefTypeAlign(Outs[OutIdx].ArgVT, DAG),
                   getPrefTypeAlign(ArgValue.getValueType(), DAG));
      TypeSize StoredSize = ArgValue.getValueType().getStoreSize();
      // If the original argument was split (e.g. i128), we need
      // to store the required parts of it here (and pass just one address).
      // Vectors may be partly split to registers and partly to the stack, in
      // which case the base address is partly offset and subsequent stores are
      // relative to that.
      unsigned ArgIndex = Outs[OutIdx].OrigArgIndex;
      unsigned ArgPartOffset = Outs[OutIdx].PartOffset;
      assert(VA.getValVT().isVector() || ArgPartOffset == 0);
      // Calculate the total size to store. We don't have access to what we're
      // actually storing other than performing the loop and collecting the
      // info.
      SmallVector<std::pair<SDValue, SDValue>> Parts;
      while (i + 1 != e && Outs[OutIdx + 1].OrigArgIndex == ArgIndex) {
        SDValue PartValue = OutVals[OutIdx + 1];
        unsigned PartOffset = Outs[OutIdx + 1].PartOffset - ArgPartOffset;
        SDValue Offset = DAG.getIntPtrConstant(PartOffset, DL);
        EVT PartVT = PartValue.getValueType();
        if (PartVT.isScalableVector())
          Offset = DAG.getNode(ISD::VSCALE, DL, XLenVT, Offset);
        StoredSize += PartVT.getStoreSize();
        StackAlign = std::max(StackAlign, getPrefTypeAlign(PartVT, DAG));
        Parts.push_back(std::make_pair(PartValue, Offset));
        ++i;
        ++OutIdx;
      }
      SDValue SpillSlot = DAG.CreateStackTemporary(StoredSize, StackAlign);
      int FI = cast<FrameIndexSDNode>(SpillSlot)->getIndex();
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, ArgValue, SpillSlot,
                       MachinePointerInfo::getFixedStack(MF, FI)));
      for (const auto &Part : Parts) {
        SDValue PartValue = Part.first;
        SDValue PartOffset = Part.second;
        SDValue Address =
            DAG.getNode(ISD::ADD, DL, PtrVT, SpillSlot, PartOffset);
        MemOpChains.push_back(
            DAG.getStore(Chain, DL, PartValue, Address,
                         MachinePointerInfo::getFixedStack(MF, FI)));
      }
      ArgValue = SpillSlot;
    } else {
      ArgValue = convertValVTToLocVT(DAG, ArgValue, VA, DL, Subtarget);
    }

    // Use local copy if it is a byval arg.
    if (Flags.isByVal())
      ArgValue = ByValArgs[j++];

    if (VA.isRegLoc()) {
      // Queue up the argument copies and emit them at the end.
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), ArgValue));

      const TargetOptions &Options = DAG.getTarget().Options;
      if (Options.EmitCallSiteInfo)
        CSInfo.ArgRegPairs.emplace_back(VA.getLocReg(), i);
    } else {
      assert(VA.isMemLoc() && "Argument not register or memory");
      assert(!IsTailCall && "Tail call not allowed if stack is used "
                            "for passing parameters");

      // Work out the address of the stack slot.
      if (!StackPtr.getNode())
        StackPtr = DAG.getCopyFromReg(Chain, DL, YSX::X2, PtrVT);
      SDValue Address =
          DAG.getNode(ISD::ADD, DL, PtrVT, StackPtr,
                      DAG.getIntPtrConstant(VA.getLocMemOffset(), DL));

      // Emit the store.
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, ArgValue, Address,
                       MachinePointerInfo::getStack(MF, VA.getLocMemOffset())));
    }
  }

  // Join the stores, which are independent of one another.
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  SDValue Glue;

  // Build a sequence of copy-to-reg nodes, chained and glued together.
  for (auto &Reg : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg.first, Reg.second, Glue);
    Glue = Chain.getValue(1);
  }

  // Validate that none of the argument registers have been marked as
  // reserved, if so report an error. Do the same for the return address if this
  // is not a tailcall.
  validateCCReservedRegs(RegsToPass, MF);
  if (!IsTailCall && MF.getSubtarget().isRegisterReservedByUser(YSX::X1))
    MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
        MF.getFunction(),
        "Return address register required, but has been reserved."});

  // If the callee is a GlobalAddress/ExternalSymbol node, turn it into a
  // TargetGlobalAddress/TargetExternalSymbol node so that legalize won't
  // split it and then direct call can be matched by PseudoCALL.
  bool CalleeIsLargeExternalSymbol = false;
  if (getTargetMachine().getCodeModel() == CodeModel::Large) {
    if (auto *S = dyn_cast<GlobalAddressSDNode>(Callee))
      Callee = getLargeGlobalAddress(S, DL, PtrVT, DAG);
    else if (auto *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
      Callee = getLargeExternalSymbol(S, DL, PtrVT, DAG);
      CalleeIsLargeExternalSymbol = true;
    }
  } else if (GlobalAddressSDNode *S = dyn_cast<GlobalAddressSDNode>(Callee)) {
    const GlobalValue *GV = S->getGlobal();
    Callee = DAG.getTargetGlobalAddress(GV, DL, PtrVT, 0, YSXII::MO_CALL);
  } else if (ExternalSymbolSDNode *S = dyn_cast<ExternalSymbolSDNode>(Callee)) {
    Callee = DAG.getTargetExternalSymbol(S->getSymbol(), PtrVT, YSXII::MO_CALL);
  }

  // The first call operand is the chain and the second is the target address.
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);
  Ops.push_back(Callee);

  // Add argument registers to the end of the list so that they are
  // known live into the call.
  for (auto &Reg : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg.first, Reg.second.getValueType()));

  // Add a register mask operand representing the call-preserved registers.
  const TargetRegisterInfo *TRI = Subtarget.getRegisterInfo();
  const uint32_t *Mask = TRI->getCallPreservedMask(MF, CallConv);
  assert(Mask && "Missing call preserved mask for calling convention");
  Ops.push_back(DAG.getRegisterMask(Mask));

  // Glue the call to the argument copies, if any.
  if (Glue.getNode())
    Ops.push_back(Glue);

  assert((!CLI.CFIType || CLI.CB->isIndirectCall()) &&
         "Unexpected CFI type for a direct call");

  // Emit the call.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);

  // Use software guarded branch for large code model non-indirect calls
  // Tail call to external symbol will have a null CLI.CB and we need another
  // way to determine the callsite type
  bool NeedSWGuarded = false;
  if (getTargetMachine().getCodeModel() == CodeModel::Large &&
      Subtarget.hasStdExtZicfilp() &&
      ((CLI.CB && !CLI.CB->isIndirectCall()) || CalleeIsLargeExternalSymbol))
    NeedSWGuarded = true;

  if (IsTailCall) {
    MF.getFrameInfo().setHasTailCall();
    unsigned CallOpc =
        NeedSWGuarded ? YSXISD::SW_GUARDED_TAIL : YSXISD::TAIL;
    SDValue Ret = DAG.getNode(CallOpc, DL, NodeTys, Ops);
    if (CLI.CFIType)
      Ret.getNode()->setCFIType(CLI.CFIType->getZExtValue());
    DAG.addNoMergeSiteInfo(Ret.getNode(), CLI.NoMerge);
    DAG.addCallSiteInfo(Ret.getNode(), std::move(CSInfo));
    return Ret;
  }

  unsigned CallOpc = NeedSWGuarded ? YSXISD::SW_GUARDED_CALL : YSXISD::CALL;
  Chain = DAG.getNode(CallOpc, DL, NodeTys, Ops);
  if (CLI.CFIType)
    Chain.getNode()->setCFIType(CLI.CFIType->getZExtValue());

  DAG.addNoMergeSiteInfo(Chain.getNode(), CLI.NoMerge);
  DAG.addCallSiteInfo(Chain.getNode(), std::move(CSInfo));
  Glue = Chain.getValue(1);

  // Mark the end of the call, which is glued to the call itself.
  Chain = DAG.getCALLSEQ_END(Chain, NumBytes, 0, Glue, DL);
  Glue = Chain.getValue(1);

  // Assign locations to each value returned by this call.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RetCCInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  analyzeInputArgs(MF, RetCCInfo, Ins, /*IsRet=*/true, CC_YSX);

  // Copy all of the result registers out of their specified physreg.
  for (unsigned i = 0, e = RVLocs.size(); i != e; ++i) {
    auto &VA = RVLocs[i];
    // Copy the value out
    SDValue RetValue =
        DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getLocVT(), Glue);
    // Glue the RetValue to the end of the call sequence
    Chain = RetValue.getValue(1);
    Glue = RetValue.getValue(2);

    RetValue = convertLocVTToValVT(DAG, RetValue, VA, DL, Subtarget);

    InVals.push_back(RetValue);
  }

  return Chain;
}

bool YSXTargetLowering::CanLowerReturn(
    CallingConv::ID CallConv, MachineFunction &MF, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs, LLVMContext &Context,
    const Type *RetTy) const {
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, RVLocs, Context);

  for (unsigned i = 0, e = Outs.size(); i != e; ++i) {
    MVT VT = Outs[i].VT;
    ISD::ArgFlagsTy ArgFlags = Outs[i].Flags;
    if (CC_YSX(i, VT, VT, CCValAssign::Full, ArgFlags, CCInfo,
                 /*IsRet=*/true, Outs[i].OrigTy))
      return false;
  }
  return true;
}

SDValue
YSXTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                 bool IsVarArg,
                                 const SmallVectorImpl<ISD::OutputArg> &Outs,
                                 const SmallVectorImpl<SDValue> &OutVals,
                                 const SDLoc &DL, SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();

  // Stores the assignment of the return value to a location.
  SmallVector<CCValAssign, 16> RVLocs;

  // Info about the registers and stack slot.
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());

  analyzeOutputArgs(DAG.getMachineFunction(), CCInfo, Outs, /*IsRet=*/true,
                    nullptr, CC_YSX);

  if (CallConv == CallingConv::GHC && !RVLocs.empty())
    reportFatalUsageError("GHC functions return void only");

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps(1, Chain);

  // Copy the result values into the output registers.
  for (unsigned i = 0, e = RVLocs.size(), OutIdx = 0; i < e; ++i, ++OutIdx) {
    SDValue Val = OutVals[OutIdx];
    CCValAssign &VA = RVLocs[i];
    assert(VA.isRegLoc() && "Can only return in registers!");

    {
      // Handle a normal return.
      Val = convertValVTToLocVT(DAG, Val, VA, DL, Subtarget);
      Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), Val, Glue);

      if (Subtarget.isRegisterReservedByUser(VA.getLocReg()))
        MF.getFunction().getContext().diagnose(DiagnosticInfoUnsupported{
            MF.getFunction(),
            "Return value register required, but has been reserved."});

      // Guarantee that all emitted copies are stuck together.
      Glue = Chain.getValue(1);
      RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
    }
  }

  RetOps[0] = Chain; // Update chain.

  // Add the glue node if we have it.
  if (Glue.getNode()) {
    RetOps.push_back(Glue);
  }

  if (any_of(RVLocs,
             [](CCValAssign &VA) { return VA.getLocVT().isScalableVector(); }))
    MF.getInfo<YSXMachineFunctionInfo>()->setIsVectorCall();

  unsigned RetOpc = YSXISD::RET_GLUE;
  const Function &Func = DAG.getMachineFunction().getFunction();
  if (Func.hasFnAttribute("interrupt"))
    reportFatalUsageError(
        "YSX rv64ima does not support interrupt handlers");

  return DAG.getNode(RetOpc, DL, MVT::Other, RetOps);
}

void YSXTargetLowering::validateCCReservedRegs(
    const SmallVectorImpl<std::pair<llvm::Register, llvm::SDValue>> &Regs,
    MachineFunction &MF) const {
  const Function &F = MF.getFunction();

  if (llvm::any_of(Regs, [this](auto Reg) {
        return Subtarget.isRegisterReservedByUser(Reg.first);
      }))
    F.getContext().diagnose(DiagnosticInfoUnsupported{
        F, "Argument register required, but has been reserved."});
}

// Check if the result of the node is only used as a return value, as
// otherwise we can't perform a tail-call.
bool YSXTargetLowering::isUsedByReturnOnly(SDNode *N, SDValue &Chain) const {
  if (N->getNumValues() != 1)
    return false;
  if (!N->hasNUsesOfValue(1, 0))
    return false;

  SDNode *Copy = *N->user_begin();

  if (Copy->getOpcode() == ISD::BITCAST) {
    return isUsedByReturnOnly(Copy, Chain);
  }

  // TODO: Handle additional opcodes in order to support tail-calling libcalls
  // with soft float ABIs.
  if (Copy->getOpcode() != ISD::CopyToReg) {
    return false;
  }

  // If the ISD::CopyToReg has a glue operand, we conservatively assume it
  // isn't safe to perform a tail call.
  if (Copy->getOperand(Copy->getNumOperands() - 1).getValueType() == MVT::Glue)
    return false;

  // The copy must be used by a YSXISD::RET_GLUE, and nothing else.
  bool HasRet = false;
  for (SDNode *Node : Copy->users()) {
    if (Node->getOpcode() != YSXISD::RET_GLUE)
      return false;
    HasRet = true;
  }
  if (!HasRet)
    return false;

  Chain = Copy->getOperand(0);
  return true;
}

bool YSXTargetLowering::mayBeEmittedAsTailCall(const CallInst *CI) const {
  return CI->isTailCall();
}

/// getConstraintType - Given a constraint letter, return the type of
/// constraint it is for this target.
YSXTargetLowering::ConstraintType
YSXTargetLowering::getConstraintType(StringRef Constraint) const {
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    default:
      break;
    case 'f':
    case 'R':
      return C_RegisterClass;
    case 'I':
    case 'J':
    case 'K':
      return C_Immediate;
    case 'A':
      return C_Memory;
    case 's':
    case 'S': // A symbolic address
      return C_Other;
    }
  } else {
    if (Constraint == "vr" || Constraint == "vd" || Constraint == "vm")
      return C_RegisterClass;
    if (Constraint == "cr" || Constraint == "cR" || Constraint == "cf")
      return C_RegisterClass;
  }
  return TargetLowering::getConstraintType(Constraint);
}

std::pair<unsigned, const TargetRegisterClass *>
YSXTargetLowering::getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                                                  StringRef Constraint,
                                                  MVT VT) const {
  if (Constraint == "r" && !VT.isVector())
    return std::make_pair(0U, &YSX::GPRNoX0RegClass);
  if (Constraint == "R" &&
      (((VT == MVT::i64 || VT == MVT::f64) && !Subtarget.is64Bit()) ||
       (VT == MVT::i128 && Subtarget.is64Bit())))
    return std::make_pair(0U, &YSX::GPRPairNoX0RegClass);
  if (Constraint == "cr" && !VT.isVector())
    return std::make_pair(0U, &YSX::GPRCRegClass);
  if (Constraint == "cR" &&
      (((VT == MVT::i64 || VT == MVT::f64) && !Subtarget.is64Bit()) ||
       (VT == MVT::i128 && Subtarget.is64Bit())))
    return std::make_pair(0U, &YSX::GPRPairCRegClass);
  if (Constraint == "vr" || Constraint == "vd" || Constraint == "vm" ||
      Constraint == "f" || Constraint == "cf")
    return std::make_pair(0U, nullptr);

  // Clang will correctly decode the usage of register name aliases into their
  // official names. However, other frontends like `rustc` do not. This allows
  // users of these frontends to use the ABI names for registers in LLVM-style
  // register constraints.
  unsigned XRegFromAlias = StringSwitch<unsigned>(Constraint.lower())
                               .Case("{zero}", YSX::X0)
                               .Case("{ra}", YSX::X1)
                               .Case("{sp}", YSX::X2)
                               .Case("{gp}", YSX::X3)
                               .Case("{tp}", YSX::X4)
                               .Case("{t0}", YSX::X5)
                               .Case("{t1}", YSX::X6)
                               .Case("{t2}", YSX::X7)
                               .Cases({"{s0}", "{fp}"}, YSX::X8)
                               .Case("{s1}", YSX::X9)
                               .Case("{a0}", YSX::X10)
                               .Case("{a1}", YSX::X11)
                               .Case("{a2}", YSX::X12)
                               .Case("{a3}", YSX::X13)
                               .Case("{a4}", YSX::X14)
                               .Case("{a5}", YSX::X15)
                               .Case("{a6}", YSX::X16)
                               .Case("{a7}", YSX::X17)
                               .Case("{s2}", YSX::X18)
                               .Case("{s3}", YSX::X19)
                               .Case("{s4}", YSX::X20)
                               .Case("{s5}", YSX::X21)
                               .Case("{s6}", YSX::X22)
                               .Case("{s7}", YSX::X23)
                               .Case("{s8}", YSX::X24)
                               .Case("{s9}", YSX::X25)
                               .Case("{s10}", YSX::X26)
                               .Case("{s11}", YSX::X27)
                               .Case("{t3}", YSX::X28)
                               .Case("{t4}", YSX::X29)
                               .Case("{t5}", YSX::X30)
                               .Case("{t6}", YSX::X31)
                               .Default(YSX::NoRegister);
  if (XRegFromAlias != YSX::NoRegister)
    return std::make_pair(XRegFromAlias, &YSX::GPRRegClass);

  return TargetLowering::getRegForInlineAsmConstraint(TRI, Constraint, VT);
}

InlineAsm::ConstraintCode
YSXTargetLowering::getInlineAsmMemConstraint(StringRef ConstraintCode) const {
  // Currently only support length 1 constraints.
  if (ConstraintCode.size() == 1) {
    switch (ConstraintCode[0]) {
    case 'A':
      return InlineAsm::ConstraintCode::A;
    default:
      break;
    }
  }

  return TargetLowering::getInlineAsmMemConstraint(ConstraintCode);
}

void YSXTargetLowering::LowerAsmOperandForConstraint(
    SDValue Op, StringRef Constraint, std::vector<SDValue> &Ops,
    SelectionDAG &DAG) const {
  // Currently only support length 1 constraints.
  if (Constraint.size() == 1) {
    switch (Constraint[0]) {
    case 'I':
      // Validate & create a 12-bit signed immediate operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
        uint64_t CVal = C->getSExtValue();
        if (isInt<12>(CVal))
          Ops.push_back(DAG.getSignedTargetConstant(CVal, SDLoc(Op),
                                                    Subtarget.getXLenVT()));
      }
      return;
    case 'J':
      // Validate & create an integer zero operand.
      if (isNullConstant(Op))
        Ops.push_back(
            DAG.getTargetConstant(0, SDLoc(Op), Subtarget.getXLenVT()));
      return;
    case 'K':
      // Validate & create a 5-bit unsigned immediate operand.
      if (auto *C = dyn_cast<ConstantSDNode>(Op)) {
        uint64_t CVal = C->getZExtValue();
        if (isUInt<5>(CVal))
          Ops.push_back(
              DAG.getTargetConstant(CVal, SDLoc(Op), Subtarget.getXLenVT()));
      }
      return;
    case 'S':
      TargetLowering::LowerAsmOperandForConstraint(Op, "s", Ops, DAG);
      return;
    default:
      break;
    }
  }
  TargetLowering::LowerAsmOperandForConstraint(Op, Constraint, Ops, DAG);
}

Instruction *YSXTargetLowering::emitLeadingFence(IRBuilderBase &Builder,
                                                   Instruction *Inst,
                                                   AtomicOrdering Ord) const {
  if (Subtarget.hasStdExtZtso()) {
    if (isa<LoadInst>(Inst) && Ord == AtomicOrdering::SequentiallyConsistent)
      return Builder.CreateFence(Ord);
    return nullptr;
  }

  if (isa<LoadInst>(Inst) && Ord == AtomicOrdering::SequentiallyConsistent)
    return Builder.CreateFence(Ord);
  if (isa<StoreInst>(Inst) && isReleaseOrStronger(Ord))
    return Builder.CreateFence(AtomicOrdering::Release);
  return nullptr;
}

Instruction *YSXTargetLowering::emitTrailingFence(IRBuilderBase &Builder,
                                                    Instruction *Inst,
                                                    AtomicOrdering Ord) const {
  if (Subtarget.hasStdExtZtso()) {
    if (isa<StoreInst>(Inst) && Ord == AtomicOrdering::SequentiallyConsistent)
      return Builder.CreateFence(Ord);
    return nullptr;
  }

  if (isa<LoadInst>(Inst) && isAcquireOrStronger(Ord))
    return Builder.CreateFence(AtomicOrdering::Acquire);
  if (Subtarget.enableTrailingSeqCstFence() && isa<StoreInst>(Inst) &&
      Ord == AtomicOrdering::SequentiallyConsistent)
    return Builder.CreateFence(AtomicOrdering::SequentiallyConsistent);
  return nullptr;
}

TargetLowering::AtomicExpansionKind
YSXTargetLowering::shouldExpandAtomicRMWInIR(AtomicRMWInst *AI) const {
  // atomicrmw {fadd,fsub} must be expanded to use compare-exchange, as floating
  // point operations can't be used in an lr/sc sequence without breaking the
  // forward-progress guarantee.
  if (AI->isFloatingPointOperation() ||
      AI->getOperation() == AtomicRMWInst::UIncWrap ||
      AI->getOperation() == AtomicRMWInst::UDecWrap ||
      AI->getOperation() == AtomicRMWInst::USubCond ||
      AI->getOperation() == AtomicRMWInst::USubSat)
    return AtomicExpansionKind::CmpXChg;

  // Don't expand forced atomics, we want to have __sync libcalls instead.
  if (Subtarget.hasForcedAtomics())
    return AtomicExpansionKind::None;

  unsigned Size = AI->getType()->getPrimitiveSizeInBits();
  if (AI->getOperation() == AtomicRMWInst::Nand) {
    if (Subtarget.hasStdExtZacas() &&
        (Size >= 32 || Subtarget.hasStdExtZabha()))
      return AtomicExpansionKind::CmpXChg;
    if (Size < 32)
      return AtomicExpansionKind::MaskedIntrinsic;
  }

  if (Size < 32 && !Subtarget.hasStdExtZabha())
    return AtomicExpansionKind::MaskedIntrinsic;

  return AtomicExpansionKind::None;
}

static Intrinsic::ID
getIntrinsicForMaskedAtomicRMWBinOp(unsigned XLen, AtomicRMWInst::BinOp BinOp) {
  switch (BinOp) {
  default:
    llvm_unreachable("Unexpected AtomicRMW BinOp");
  case AtomicRMWInst::Xchg:
    return Intrinsic::riscv_masked_atomicrmw_xchg;
  case AtomicRMWInst::Add:
    return Intrinsic::riscv_masked_atomicrmw_add;
  case AtomicRMWInst::Sub:
    return Intrinsic::riscv_masked_atomicrmw_sub;
  case AtomicRMWInst::Nand:
    return Intrinsic::riscv_masked_atomicrmw_nand;
  case AtomicRMWInst::Max:
    return Intrinsic::riscv_masked_atomicrmw_max;
  case AtomicRMWInst::Min:
    return Intrinsic::riscv_masked_atomicrmw_min;
  case AtomicRMWInst::UMax:
    return Intrinsic::riscv_masked_atomicrmw_umax;
  case AtomicRMWInst::UMin:
    return Intrinsic::riscv_masked_atomicrmw_umin;
  }
}

Value *YSXTargetLowering::emitMaskedAtomicRMWIntrinsic(
    IRBuilderBase &Builder, AtomicRMWInst *AI, Value *AlignedAddr, Value *Incr,
    Value *Mask, Value *ShiftAmt, AtomicOrdering Ord) const {
  // In the case of an atomicrmw xchg with a constant 0/-1 operand, replace
  // the atomic instruction with an AtomicRMWInst::And/Or with appropriate
  // mask, as this produces better code than the LR/SC loop emitted by
  // int_riscv_masked_atomicrmw_xchg.
  if (AI->getOperation() == AtomicRMWInst::Xchg &&
      isa<ConstantInt>(AI->getValOperand())) {
    ConstantInt *CVal = cast<ConstantInt>(AI->getValOperand());
    if (CVal->isZero())
      return Builder.CreateAtomicRMW(AtomicRMWInst::And, AlignedAddr,
                                     Builder.CreateNot(Mask, "Inv_Mask"),
                                     AI->getAlign(), Ord);
    if (CVal->isMinusOne())
      return Builder.CreateAtomicRMW(AtomicRMWInst::Or, AlignedAddr, Mask,
                                     AI->getAlign(), Ord);
  }

  unsigned XLen = Subtarget.getXLen();
  Value *Ordering =
      Builder.getIntN(XLen, static_cast<uint64_t>(AI->getOrdering()));
  Type *Tys[] = {Builder.getIntNTy(XLen), AlignedAddr->getType()};
  Function *LrwOpScwLoop = Intrinsic::getOrInsertDeclaration(
      AI->getModule(),
      getIntrinsicForMaskedAtomicRMWBinOp(XLen, AI->getOperation()), Tys);

  if (XLen == 64) {
    Incr = Builder.CreateSExt(Incr, Builder.getInt64Ty());
    Mask = Builder.CreateSExt(Mask, Builder.getInt64Ty());
    ShiftAmt = Builder.CreateSExt(ShiftAmt, Builder.getInt64Ty());
  }

  Value *Result;

  // Must pass the shift amount needed to sign extend the loaded value prior
  // to performing a signed comparison for min/max. ShiftAmt is the number of
  // bits to shift the value into position. Pass XLen-ShiftAmt-ValWidth, which
  // is the number of bits to left+right shift the value in order to
  // sign-extend.
  if (AI->getOperation() == AtomicRMWInst::Min ||
      AI->getOperation() == AtomicRMWInst::Max) {
    const DataLayout &DL = AI->getDataLayout();
    unsigned ValWidth =
        DL.getTypeStoreSizeInBits(AI->getValOperand()->getType());
    Value *SextShamt =
        Builder.CreateSub(Builder.getIntN(XLen, XLen - ValWidth), ShiftAmt);
    Result = Builder.CreateCall(LrwOpScwLoop,
                                {AlignedAddr, Incr, Mask, SextShamt, Ordering});
  } else {
    Result =
        Builder.CreateCall(LrwOpScwLoop, {AlignedAddr, Incr, Mask, Ordering});
  }

  if (XLen == 64)
    Result = Builder.CreateTrunc(Result, Builder.getInt32Ty());
  return Result;
}

TargetLowering::AtomicExpansionKind
YSXTargetLowering::shouldExpandAtomicCmpXchgInIR(
    AtomicCmpXchgInst *CI) const {
  // Don't expand forced atomics, we want to have __sync libcalls instead.
  if (Subtarget.hasForcedAtomics())
    return AtomicExpansionKind::None;

  unsigned Size = CI->getCompareOperand()->getType()->getPrimitiveSizeInBits();
  if (!(Subtarget.hasStdExtZabha() && Subtarget.hasStdExtZacas()) &&
      (Size == 8 || Size == 16))
    return AtomicExpansionKind::MaskedIntrinsic;
  return AtomicExpansionKind::None;
}

Value *YSXTargetLowering::emitMaskedAtomicCmpXchgIntrinsic(
    IRBuilderBase &Builder, AtomicCmpXchgInst *CI, Value *AlignedAddr,
    Value *CmpVal, Value *NewVal, Value *Mask, AtomicOrdering Ord) const {
  unsigned XLen = Subtarget.getXLen();
  Value *Ordering = Builder.getIntN(XLen, static_cast<uint64_t>(Ord));
  Intrinsic::ID CmpXchgIntrID = Intrinsic::riscv_masked_cmpxchg;
  if (XLen == 64) {
    CmpVal = Builder.CreateSExt(CmpVal, Builder.getInt64Ty());
    NewVal = Builder.CreateSExt(NewVal, Builder.getInt64Ty());
    Mask = Builder.CreateSExt(Mask, Builder.getInt64Ty());
  }
  Type *Tys[] = {Builder.getIntNTy(XLen), AlignedAddr->getType()};
  Value *Result = Builder.CreateIntrinsic(
      CmpXchgIntrID, Tys, {AlignedAddr, CmpVal, NewVal, Mask, Ordering});
  if (XLen == 64)
    Result = Builder.CreateTrunc(Result, Builder.getInt32Ty());
  return Result;
}

bool YSXTargetLowering::shouldRemoveExtendFromGSIndex(SDValue Extend,
                                                        EVT DataVT) const {
  // We have indexed loads for all supported EEW types. Indices are always
  // zero extended.
  return Extend.getOpcode() == ISD::ZERO_EXTEND &&
         isTypeLegal(Extend.getValueType()) &&
         isTypeLegal(Extend.getOperand(0).getValueType()) &&
         Extend.getOperand(0).getValueType().getVectorElementType() != MVT::i1;
}

bool YSXTargetLowering::shouldConvertFpToSat(unsigned Op, EVT FPVT,
                                               EVT VT) const {
  if (!isOperationLegalOrCustom(Op, VT) || !FPVT.isSimple())
    return false;

  switch (FPVT.getSimpleVT().SimpleTy) {
  case MVT::f16:
    return Subtarget.hasStdExtZfhmin();
  case MVT::f32:
    return Subtarget.hasStdExtF();
  case MVT::f64:
    return Subtarget.hasStdExtD();
  default:
    return false;
  }
}

unsigned YSXTargetLowering::getJumpTableEncoding() const {
  // If we are using the small code model, we can reduce size of jump table
  // entry to 4 bytes.
  if (Subtarget.is64Bit() && !isPositionIndependent() &&
      getTargetMachine().getCodeModel() == CodeModel::Small) {
    return MachineJumpTableInfo::EK_Custom32;
  }
  return TargetLowering::getJumpTableEncoding();
}

const MCExpr *YSXTargetLowering::LowerCustomJumpTableEntry(
    const MachineJumpTableInfo *MJTI, const MachineBasicBlock *MBB,
    unsigned uid, MCContext &Ctx) const {
  assert(Subtarget.is64Bit() && !isPositionIndependent() &&
         getTargetMachine().getCodeModel() == CodeModel::Small);
  return MCSymbolRefExpr::create(MBB->getSymbol(), Ctx);
}

bool YSXTargetLowering::isVScaleKnownToBeAPowerOfTwo() const {
  // We define vscale to be VLEN/YSXVecBitsPerBlock.  VLEN is always a power
  // of two >= 64, and YSXVecBitsPerBlock is 64.  Thus, vscale must be
  // a power of two as well.
  // FIXME: This doesn't work for zve32, but that's already broken
  // elsewhere for the same reason.
  assert(Subtarget.getRealMinVLen() >= 64 && "zve32* unsupported");
  static_assert(YSX::YSXVecBitsPerBlock == 64,
                "YSXVecBitsPerBlock changed, audit needed");
  return true;
}

bool YSXTargetLowering::getIndexedAddressParts(SDNode *Op, SDValue &Base,
                                                 SDValue &Offset,
                                                 ISD::MemIndexedMode &AM,
                                                 SelectionDAG &DAG) const {
  // Target does not support indexed loads.
  if (!Subtarget.hasVendorXRemovedTHeadMemIdx())
    return false;

  if (Op->getOpcode() != ISD::ADD && Op->getOpcode() != ISD::SUB)
    return false;

  Base = Op->getOperand(0);
  if (ConstantSDNode *RHS = dyn_cast<ConstantSDNode>(Op->getOperand(1))) {
    int64_t RHSC = RHS->getSExtValue();
    if (Op->getOpcode() == ISD::SUB)
      RHSC = -(uint64_t)RHSC;

    // The constants that can be encoded in the THeadMemIdx instructions
    // are of the form (sign_extend(imm5) << imm2).
    bool isLegalIndexedOffset = false;
    for (unsigned i = 0; i < 4; i++)
      if (isInt<5>(RHSC >> i) && ((RHSC % (1LL << i)) == 0)) {
        isLegalIndexedOffset = true;
        break;
      }

    if (!isLegalIndexedOffset)
      return false;

    Offset = Op->getOperand(1);
    return true;
  }

  return false;
}

bool YSXTargetLowering::getPreIndexedAddressParts(SDNode *N, SDValue &Base,
                                                    SDValue &Offset,
                                                    ISD::MemIndexedMode &AM,
                                                    SelectionDAG &DAG) const {
  EVT VT;
  SDValue Ptr;
  if (LoadSDNode *LD = dyn_cast<LoadSDNode>(N)) {
    VT = LD->getMemoryVT();
    Ptr = LD->getBasePtr();
  } else if (StoreSDNode *ST = dyn_cast<StoreSDNode>(N)) {
    VT = ST->getMemoryVT();
    Ptr = ST->getBasePtr();
  } else
    return false;

  if (!getIndexedAddressParts(Ptr.getNode(), Base, Offset, AM, DAG))
    return false;

  AM = ISD::PRE_INC;
  return true;
}

bool YSXTargetLowering::getPostIndexedAddressParts(SDNode *N, SDNode *Op,
                                                     SDValue &Base,
                                                     SDValue &Offset,
                                                     ISD::MemIndexedMode &AM,
                                                     SelectionDAG &DAG) const {
  if (Subtarget.hasVendorXCVmem() && !Subtarget.is64Bit()) {
    if (Op->getOpcode() != ISD::ADD)
      return false;

    if (LSBaseSDNode *LS = dyn_cast<LSBaseSDNode>(N))
      Base = LS->getBasePtr();
    else
      return false;

    if (Base == Op->getOperand(0))
      Offset = Op->getOperand(1);
    else if (Base == Op->getOperand(1))
      Offset = Op->getOperand(0);
    else
      return false;

    AM = ISD::POST_INC;
    return true;
  }

  EVT VT;
  SDValue Ptr;
  if (LoadSDNode *LD = dyn_cast<LoadSDNode>(N)) {
    VT = LD->getMemoryVT();
    Ptr = LD->getBasePtr();
  } else if (StoreSDNode *ST = dyn_cast<StoreSDNode>(N)) {
    VT = ST->getMemoryVT();
    Ptr = ST->getBasePtr();
  } else
    return false;

  if (!getIndexedAddressParts(Op, Base, Offset, AM, DAG))
    return false;
  // Post-indexing updates the base, so it's not a valid transform
  // if that's not the same as the load's pointer.
  if (Ptr != Base)
    return false;

  AM = ISD::POST_INC;
  return true;
}

bool YSXTargetLowering::isFMAFasterThanFMulAndFAdd(const MachineFunction &MF,
                                                     EVT VT) const {
  EVT SVT = VT.getScalarType();

  if (!SVT.isSimple())
    return false;

  switch (SVT.getSimpleVT().SimpleTy) {
  case MVT::f16:
    return VT.isVector() ? Subtarget.hasVInstructionsF16()
                         : Subtarget.hasStdExtZfhOrZhinx();
  case MVT::f32:
    return Subtarget.hasStdExtFOrZfinx();
  case MVT::f64:
    return Subtarget.hasStdExtDOrZdinx();
  default:
    break;
  }

  return false;
}

ISD::NodeType YSXTargetLowering::getExtendForAtomicCmpSwapArg() const {
  // Zacas will use amocas.w which does not require extension.
  return Subtarget.hasStdExtZacas() ? ISD::ANY_EXTEND : ISD::SIGN_EXTEND;
}

ISD::NodeType YSXTargetLowering::getExtendForAtomicRMWArg(unsigned Op) const {
  // Zaamo will use amo<op>.w which does not require extension.
  if (Subtarget.hasStdExtZaamo() || Subtarget.hasForcedAtomics())
    return ISD::ANY_EXTEND;

  // Zalrsc pseudo expansions with comparison require sign-extension.
  assert(Subtarget.hasStdExtZalrsc());
  switch (Op) {
  case ISD::ATOMIC_LOAD_MIN:
  case ISD::ATOMIC_LOAD_MAX:
  case ISD::ATOMIC_LOAD_UMIN:
  case ISD::ATOMIC_LOAD_UMAX:
    return ISD::SIGN_EXTEND;
  default:
    break;
  }
  return ISD::ANY_EXTEND;
}

Register YSXTargetLowering::getExceptionPointerRegister(
    const Constant *PersonalityFn) const {
  return YSX::X10;
}

Register YSXTargetLowering::getExceptionSelectorRegister(
    const Constant *PersonalityFn) const {
  return YSX::X11;
}

bool YSXTargetLowering::shouldExtendTypeInLibCall(EVT Type) const {
  // Return false to suppress the unnecessary extensions if the LibCall
  // arguments or return value is a float narrower than XLEN on a soft FP ABI.
  if (Subtarget.isSoftFPABI() && (Type.isFloatingPoint() && !Type.isVector() &&
                                  Type.getSizeInBits() < Subtarget.getXLen()))
    return false;

  return true;
}

bool YSXTargetLowering::shouldSignExtendTypeInLibCall(Type *Ty,
                                                        bool IsSigned) const {
  if (Subtarget.is64Bit() && Ty->isIntegerTy(32))
    return true;

  return IsSigned;
}

bool YSXTargetLowering::decomposeMulByConstant(LLVMContext &Context, EVT VT,
                                                 SDValue C) const {
  // Check integral scalar types.
  if (!VT.isScalarInteger())
    return false;

  // Omit the optimization if the sub target has the M extension and the data
  // size exceeds XLen.
  const bool HasZmmul = Subtarget.hasStdExtZmmul();
  if (HasZmmul && VT.getSizeInBits() > Subtarget.getXLen())
    return false;

  auto *ConstNode = cast<ConstantSDNode>(C);
  const APInt &Imm = ConstNode->getAPIntValue();

  // Don't do this if the XRemovedQciac extension is enabled and the Imm in simm12.
  if (Subtarget.hasVendorXRemovedQciac() && Imm.isSignedIntN(12))
    return false;

  // Break the MUL to a SLLI and an ADD/SUB.
  if ((Imm + 1).isPowerOf2() || (Imm - 1).isPowerOf2() ||
      (1 - Imm).isPowerOf2() || (-1 - Imm).isPowerOf2())
    return true;

  // Optimize the MUL to (SH*ADD x, (SLLI x, bits)) if Imm is not simm12.
  if (Subtarget.hasShlAdd(3) && !Imm.isSignedIntN(12) &&
      ((Imm - 2).isPowerOf2() || (Imm - 4).isPowerOf2() ||
       (Imm - 8).isPowerOf2()))
    return true;

  // Break the MUL to two SLLI instructions and an ADD/SUB, if Imm needs
  // a pair of LUI/ADDI.
  if (!Imm.isSignedIntN(12) && Imm.countr_zero() < 12 &&
      ConstNode->hasOneUse()) {
    APInt ImmS = Imm.ashr(Imm.countr_zero());
    if ((ImmS + 1).isPowerOf2() || (ImmS - 1).isPowerOf2() ||
        (1 - ImmS).isPowerOf2())
      return true;
  }

  return false;
}

bool YSXTargetLowering::isMulAddWithConstProfitable(SDValue AddNode,
                                                      SDValue ConstNode) const {
  // Let the DAGCombiner decide for vectors.
  EVT VT = AddNode.getValueType();
  if (VT.isVector())
    return true;

  // Let the DAGCombiner decide for larger types.
  if (VT.getScalarSizeInBits() > Subtarget.getXLen())
    return true;

  // It is worse if c1 is simm12 while c1*c2 is not.
  ConstantSDNode *C1Node = cast<ConstantSDNode>(AddNode.getOperand(1));
  ConstantSDNode *C2Node = cast<ConstantSDNode>(ConstNode);
  const APInt &C1 = C1Node->getAPIntValue();
  const APInt &C2 = C2Node->getAPIntValue();
  if (C1.isSignedIntN(12) && !(C1 * C2).isSignedIntN(12))
    return false;

  // Default to true and let the DAGCombiner decide.
  return true;
}

bool YSXTargetLowering::allowsMisalignedMemoryAccesses(
    EVT VT, unsigned AddrSpace, Align Alignment, MachineMemOperand::Flags Flags,
    unsigned *Fast) const {
  if (!VT.isVector() || Subtarget.enablePExtSIMDCodeGen()) {
    if (Fast)
      *Fast = Subtarget.enableUnalignedScalarMem();
    return Subtarget.enableUnalignedScalarMem();
  }

  // All vector implementations must support element alignment
  EVT ElemVT = VT.getVectorElementType();
  if (Alignment >= ElemVT.getStoreSize()) {
    if (Fast)
      *Fast = 1;
    return true;
  }

  // Note: We lower an unmasked unaligned vector access to an equally sized
  // e8 element type access.  Given this, we effectively support all unmasked
  // misaligned accesses.  TODO: Work through the codegen implications of
  // allowing such accesses to be formed, and considered fast.
  if (Fast)
    *Fast = Subtarget.enableUnalignedVectorMem();
  return Subtarget.enableUnalignedVectorMem();
}

EVT YSXTargetLowering::getOptimalMemOpType(
    LLVMContext &Context, const MemOp &Op,
    const AttributeList &FuncAttributes) const {
  if (!Subtarget.hasVInstructions())
    return MVT::Other;

  if (FuncAttributes.hasFnAttr(Attribute::NoImplicitFloat))
    return MVT::Other;

  // We use LMUL1 memory operations here for a non-obvious reason.  Our caller
  // has an expansion threshold, and we want the number of hardware memory
  // operations to correspond roughly to that threshold.  LMUL>1 operations
  // are typically expanded linearly internally, and thus correspond to more
  // than one actual memory operation.  Note that store merging and load
  // combining will typically form larger LMUL operations from the LMUL1
  // operations emitted here, and that's okay because combining isn't
  // introducing new memory operations; it's just merging existing ones.
  // NOTE: We limit to 1024 bytes to avoid creating an invalid MVT.
  const unsigned MinVLenInBytes =
      std::min(Subtarget.getRealMinVLen() / 8, 1024U);

  if (Op.size() < MinVLenInBytes)
    // TODO: Figure out short memops.  For the moment, do the default thing
    // which ends up using scalar sequences.
    return MVT::Other;

  // If the minimum VLEN is less than YSX::YSXVecBitsPerBlock we don't support
  // fixed vectors.
  if (MinVLenInBytes <= YSX::YSXVecBytesPerBlock)
    return MVT::Other;

  // Prefer i8 for non-zero memset as it allows us to avoid materializing
  // a large scalar constant and instead use vmv.v.x/i to do the
  // broadcast.  For everything else, prefer ELenVT to minimize VL and thus
  // maximize the chance we can encode the size in the vsetvli.
  MVT ELenVT = MVT::getIntegerVT(Subtarget.getELen());
  MVT PreferredVT = (Op.isMemset() && !Op.isZeroMemset()) ? MVT::i8 : ELenVT;

  // Do we have sufficient alignment for our preferred VT?  If not, revert
  // to largest size allowed by our alignment criteria.
  if (PreferredVT != MVT::i8 && !Subtarget.enableUnalignedVectorMem()) {
    Align RequiredAlign(PreferredVT.getStoreSize());
    if (Op.isFixedDstAlign())
      RequiredAlign = std::min(RequiredAlign, Op.getDstAlign());
    if (Op.isMemcpy())
      RequiredAlign = std::min(RequiredAlign, Op.getSrcAlign());
    PreferredVT = MVT::getIntegerVT(RequiredAlign.value() * 8);
  }
  return MVT::getVectorVT(PreferredVT, MinVLenInBytes/PreferredVT.getStoreSize());
}

bool YSXTargetLowering::splitValueIntoRegisterParts(
    SelectionDAG &DAG, const SDLoc &DL, SDValue Val, SDValue *Parts,
    unsigned NumParts, MVT PartVT, std::optional<CallingConv::ID> CC) const {
  EVT ValueVT = Val.getValueType();
  if (ValueVT == MVT::i128 && NumParts == 1 && PartVT == MVT::Untyped) {
    auto [Lo, Hi] = DAG.SplitScalar(Val, DL, MVT::i64, MVT::i64);
    Parts[0] = DAG.getNode(YSXISD::BuildGPRPair, DL, PartVT, Lo, Hi);
    return true;
  }
  return false;
}

SDValue YSXTargetLowering::joinRegisterPartsIntoValue(
    SelectionDAG &DAG, const SDLoc &DL, const SDValue *Parts, unsigned NumParts,
    MVT PartVT, EVT ValueVT, std::optional<CallingConv::ID> CC) const {
  if (ValueVT == MVT::i128 && NumParts == 1 && PartVT == MVT::Untyped) {
    SDValue Val = DAG.getNode(YSXISD::SplitGPRPair, DL,
                              DAG.getVTList(MVT::i64, MVT::i64), Parts[0]);
    return DAG.getNode(ISD::BUILD_PAIR, DL, MVT::i128, Val.getValue(0),
                       Val.getValue(1));
  }
  return SDValue();
}

bool YSXTargetLowering::isIntDivCheap(EVT VT, AttributeList Attr) const {
  // When aggressively optimizing for code size, we prefer to use a div
  // instruction, as it is usually smaller than the alternative sequence.
  // TODO: Add vector division?
  bool OptSize = Attr.hasFnAttr(Attribute::MinSize);
  return OptSize && !VT.isVector() &&
         VT.getSizeInBits() <= getMaxDivRemBitWidthSupported();
}

bool YSXTargetLowering::preferScalarizeSplat(SDNode *N) const {
  // Scalarize zero_ext and sign_ext might stop match to widening instruction in
  // some situation.
  unsigned Opc = N->getOpcode();
  if (Opc == ISD::ZERO_EXTEND || Opc == ISD::SIGN_EXTEND)
    return false;
  return true;
}

static Value *useTpOffset(IRBuilderBase &IRB, unsigned Offset) {
  Module *M = IRB.GetInsertBlock()->getModule();
  Function *ThreadPointerFunc = Intrinsic::getOrInsertDeclaration(
      M, Intrinsic::thread_pointer, IRB.getPtrTy());
  return IRB.CreateConstGEP1_32(IRB.getInt8Ty(),
                                IRB.CreateCall(ThreadPointerFunc), Offset);
}

Value *YSXTargetLowering::getIRStackGuard(IRBuilderBase &IRB) const {
  // Fuchsia provides a fixed TLS slot for the stack cookie.
  // <zircon/tls.h> defines ZX_TLS_STACK_GUARD_OFFSET with this value.
  if (Subtarget.isTargetFuchsia())
    return useTpOffset(IRB, -0x10);

  // Android provides a fixed TLS slot for the stack cookie. See the definition
  // of TLS_SLOT_STACK_GUARD in
  // https://android.googlesource.com/platform/bionic/+/main/libc/platform/bionic/tls_defines.h
  if (Subtarget.isTargetAndroid())
    return useTpOffset(IRB, -0x18);

  Module *M = IRB.GetInsertBlock()->getModule();

  if (M->getStackProtectorGuard() == "tls") {
    // Users must specify the offset explicitly
    int Offset = M->getStackProtectorGuardOffset();
    return useTpOffset(IRB, Offset);
  }

  return TargetLowering::getIRStackGuard(IRB);
}

MachineInstr *
YSXTargetLowering::EmitKCFICheck(MachineBasicBlock &MBB,
                                   MachineBasicBlock::instr_iterator &MBBI,
                                   const TargetInstrInfo *TII) const {
  assert(MBBI->isCall() && MBBI->getCFIType() &&
         "Invalid call instruction for a KCFI check");
  assert(is_contained({YSX::PseudoCALLIndirect, YSX::PseudoTAILIndirect},
                      MBBI->getOpcode()));

  MachineOperand &Target = MBBI->getOperand(0);
  Target.setIsRenamable(false);

  return BuildMI(MBB, MBBI, MBBI->getDebugLoc(), TII->get(YSX::KCFI_CHECK))
      .addReg(Target.getReg())
      .addImm(MBBI->getCFIType())
      .getInstr();
}

#define GET_REGISTER_MATCHER
#include "YSXGenAsmMatcher.inc"

Register
YSXTargetLowering::getRegisterByName(const char *RegName, LLT VT,
                                       const MachineFunction &MF) const {
  Register Reg = MatchRegisterAltName(RegName);
  if (!Reg)
    Reg = MatchRegisterName(RegName);
  if (!Reg)
    return Reg;

  BitVector ReservedRegs = Subtarget.getRegisterInfo()->getReservedRegs(MF);
  if (!ReservedRegs.test(Reg) && !Subtarget.isRegisterReservedByUser(Reg))
    reportFatalUsageError(Twine("Trying to obtain non-reserved register \"" +
                                StringRef(RegName) + "\"."));
  return Reg;
}

MachineMemOperand::Flags
YSXTargetLowering::getTargetMMOFlags(const Instruction &I) const {
  const MDNode *NontemporalInfo = I.getMetadata(LLVMContext::MD_nontemporal);

  if (NontemporalInfo == nullptr)
    return MachineMemOperand::MONone;

  // 1 for default value work as __YSX_NTLH_ALL
  // 2 -> __YSX_NTLH_INNERMOST_PRIVATE
  // 3 -> __YSX_NTLH_ALL_PRIVATE
  // 4 -> __YSX_NTLH_INNERMOST_SHARED
  // 5 -> __YSX_NTLH_ALL
  int NontemporalLevel = 5;
  const MDNode *YSXNontemporalInfo =
      I.getMetadata("ysx-nontemporal-domain");
  if (YSXNontemporalInfo != nullptr)
    NontemporalLevel =
        cast<ConstantInt>(
            cast<ConstantAsMetadata>(YSXNontemporalInfo->getOperand(0))
                ->getValue())
            ->getZExtValue();

  assert((1 <= NontemporalLevel && NontemporalLevel <= 5) &&
         "RISC-V target doesn't support this non-temporal domain.");

  NontemporalLevel -= 2;
  MachineMemOperand::Flags Flags = MachineMemOperand::MONone;
  if (NontemporalLevel & 0b1)
    Flags |= MONontemporalBit0;
  if (NontemporalLevel & 0b10)
    Flags |= MONontemporalBit1;

  return Flags;
}

MachineMemOperand::Flags
YSXTargetLowering::getTargetMMOFlags(const MemSDNode &Node) const {

  MachineMemOperand::Flags NodeFlags = Node.getMemOperand()->getFlags();
  MachineMemOperand::Flags TargetFlags = MachineMemOperand::MONone;
  TargetFlags |= (NodeFlags & MONontemporalBit0);
  TargetFlags |= (NodeFlags & MONontemporalBit1);
  return TargetFlags;
}

bool YSXTargetLowering::areTwoSDNodeTargetMMOFlagsMergeable(
    const MemSDNode &NodeX, const MemSDNode &NodeY) const {
  return getTargetMMOFlags(NodeX) == getTargetMMOFlags(NodeY);
}

bool YSXTargetLowering::isCtpopFast(EVT VT) const {
  if (VT.isVector())
    return false;

  return Subtarget.hasCPOPLike() && (VT == MVT::i32 || VT == MVT::i64);
}

unsigned YSXTargetLowering::getCustomCtpopCost(EVT VT,
                                                 ISD::CondCode Cond) const {
  return isCtpopFast(VT) ? 0 : 1;
}

bool YSXTargetLowering::shouldInsertFencesForAtomic(
    const Instruction *I) const {
  if (Subtarget.hasStdExtZalasr()) {
    if (Subtarget.hasStdExtZtso()) {
      // Zalasr + TSO means that atomic_load_acquire and atomic_store_release
      // should be lowered to plain load/store. The easiest way to do this is
      // to say we should insert fences for them, and the fence insertion code
      // will just not insert any fences
      auto *LI = dyn_cast<LoadInst>(I);
      auto *SI = dyn_cast<StoreInst>(I);
      if ((LI &&
           (LI->getOrdering() == AtomicOrdering::SequentiallyConsistent)) ||
          (SI &&
           (SI->getOrdering() == AtomicOrdering::SequentiallyConsistent))) {
        // Here, this is a load or store which is seq_cst, and needs a .aq or
        // .rl therefore we shouldn't try to insert fences
        return false;
      }
      // Here, we are a TSO inst that isn't a seq_cst load/store
      return isa<LoadInst>(I) || isa<StoreInst>(I);
    }
    return false;
  }
  // Note that one specific case requires fence insertion for an
  // AtomicCmpXchgInst but is handled via the YSXZacasABIFix pass rather
  // than this hook due to limitations in the interface here.
  return isa<LoadInst>(I) || isa<StoreInst>(I);
}

bool YSXTargetLowering::fallBackToDAGISel(const Instruction &Inst) const {

  // GISel support is in progress or complete for these opcodes.
  unsigned Op = Inst.getOpcode();
  if (Op == Instruction::Add || Op == Instruction::Sub ||
      Op == Instruction::And || Op == Instruction::Or ||
      Op == Instruction::Xor || Op == Instruction::InsertElement ||
      Op == Instruction::ShuffleVector || Op == Instruction::Load ||
      Op == Instruction::Freeze || Op == Instruction::Store)
    return false;

  if (auto *II = dyn_cast<IntrinsicInst>(&Inst)) {
    // Mark YSXVec intrinsic as supported.
    if (YSXVIntrinsicsTable::getYSXVIntrinsicInfo(II->getIntrinsicID())) {
      // GISel doesn't support tuple types yet. It also doesn't suport returning
      // a struct containing a scalable vector like vleff.
      if (Inst.getType()->isRISCVVectorTupleTy() ||
          Inst.getType()->isStructTy())
        return true;

      for (unsigned i = 0; i < II->arg_size(); ++i)
        if (II->getArgOperand(i)->getType()->isRISCVVectorTupleTy())
          return true;

      return false;
    }
    if (II->getIntrinsicID() == Intrinsic::vector_extract)
      return false;
  }

  if (Inst.getType()->isScalableTy())
    return true;

  for (unsigned i = 0; i < Inst.getNumOperands(); ++i)
    if (Inst.getOperand(i)->getType()->isScalableTy() &&
        !isa<ReturnInst>(&Inst))
      return true;

  if (const AllocaInst *AI = dyn_cast<AllocaInst>(&Inst)) {
    if (AI->getAllocatedType()->isScalableTy())
      return true;
  }

  return false;
}

SDValue
YSXTargetLowering::BuildSDIVPow2(SDNode *N, const APInt &Divisor,
                                   SelectionDAG &DAG,
                                   SmallVectorImpl<SDNode *> &Created) const {
  AttributeList Attr = DAG.getMachineFunction().getFunction().getAttributes();
  if (isIntDivCheap(N->getValueType(0), Attr))
    return SDValue(N, 0); // Lower SDIV as SDIV

  // Only perform this transform if short forward branch opt is supported.
  if (!Subtarget.hasShortForwardBranchIALU())
    return SDValue();
  EVT VT = N->getValueType(0);
  if (!(VT == MVT::i32 || (VT == MVT::i64 && Subtarget.is64Bit())))
    return SDValue();

  // Ensure 2**k-1 < 2048 so that we can just emit a single addi/addiw.
  if (Divisor.sgt(2048) || Divisor.slt(-2048))
    return SDValue();
  return TargetLowering::buildSDIVPow2WithCMov(N, Divisor, DAG, Created);
}

bool YSXTargetLowering::shouldFoldSelectWithSingleBitTest(
    EVT VT, const APInt &AndMask) const {
  if (Subtarget.hasCZEROLike() || Subtarget.hasVendorXRemovedTHeadCondMov())
    return !Subtarget.hasBEXTILike() && AndMask.ugt(1024);
  return TargetLowering::shouldFoldSelectWithSingleBitTest(VT, AndMask);
}

unsigned YSXTargetLowering::getMinimumJumpTableEntries() const {
  return Subtarget.getMinimumJumpTableEntries();
}

SDValue YSXTargetLowering::expandIndirectJTBranch(const SDLoc &dl,
                                                    SDValue Value, SDValue Addr,
                                                    int JTI,
                                                    SelectionDAG &DAG) const {
  if (Subtarget.hasStdExtZicfilp()) {
    // When Zicfilp enabled, we need to use software guarded branch for jump
    // table branch.
    SDValue Chain = Value;
    // Jump table debug info is only needed if CodeView is enabled.
    if (DAG.getTarget().getTargetTriple().isOSBinFormatCOFF())
      Chain = DAG.getJumpTableDebugInfo(JTI, Chain, dl);
    return DAG.getNode(YSXISD::SW_GUARDED_BRIND, dl, MVT::Other, Chain, Addr);
  }
  return TargetLowering::expandIndirectJTBranch(dl, Value, Addr, JTI, DAG);
}

// If an output pattern produces multiple instructions tablegen may pick an
// arbitrary type from an instructions destination register class to use for the
// VT of that MachineSDNode. This VT may be used to look up the representative
// register class. If the type isn't legal, the default implementation will
// not find a register class.
//
// Some integer types smaller than XLen are listed in the GPR register class to
// support isel patterns for GISel, but are not legal in SelectionDAG. The
// arbitrary type tablegen picks may be one of these smaller types.
//
// f16 and bf16 are both valid for the FPR16 or GPRF16 register class. It's
// possible for tablegen to pick bf16 as the arbitrary type for an f16 pattern.
std::pair<const TargetRegisterClass *, uint8_t>
YSXTargetLowering::findRepresentativeClass(const TargetRegisterInfo *TRI,
                                             MVT VT) const {
  switch (VT.SimpleTy) {
  default:
    break;
  case MVT::i8:
  case MVT::i16:
  case MVT::i32:
    return TargetLowering::findRepresentativeClass(TRI, Subtarget.getXLenVT());
  case MVT::bf16:
  case MVT::f16:
    return TargetLowering::findRepresentativeClass(TRI, MVT::f32);
  }

  return TargetLowering::findRepresentativeClass(TRI, VT);
}

namespace llvm::YSXVIntrinsicsTable {

#define GET_YSXVIntrinsicsTable_IMPL
#include "YSXGenSearchableTables.inc"

} // namespace llvm::YSXVIntrinsicsTable

bool YSXTargetLowering::hasInlineStackProbe(const MachineFunction &MF) const {

  // If the function specifically requests inline stack probes, emit them.
  if (MF.getFunction().hasFnAttribute("probe-stack"))
    return MF.getFunction().getFnAttribute("probe-stack").getValueAsString() ==
           "inline-asm";

  return false;
}

unsigned YSXTargetLowering::getStackProbeSize(const MachineFunction &MF,
                                                Align StackAlign) const {
  // The default stack probe size is 4096 if the function has no
  // stack-probe-size attribute.
  const Function &Fn = MF.getFunction();
  unsigned StackProbeSize =
      Fn.getFnAttributeAsParsedInteger("stack-probe-size", 4096);
  // Round down to the stack alignment.
  StackProbeSize = alignDown(StackProbeSize, StackAlign.value());
  return StackProbeSize ? StackProbeSize : StackAlign.value();
}

SDValue YSXTargetLowering::lowerDYNAMIC_STACKALLOC(SDValue Op,
                                                     SelectionDAG &DAG) const {
  MachineFunction &MF = DAG.getMachineFunction();
  if (!hasInlineStackProbe(MF))
    return SDValue();

  MVT XLenVT = Subtarget.getXLenVT();
  // Get the inputs.
  SDValue Chain = Op.getOperand(0);
  SDValue Size = Op.getOperand(1);

  MaybeAlign Align =
      cast<ConstantSDNode>(Op.getOperand(2))->getMaybeAlignValue();
  SDLoc dl(Op);
  EVT VT = Op.getValueType();

  // Construct the new SP value in a GPR.
  SDValue SP = DAG.getCopyFromReg(Chain, dl, YSX::X2, XLenVT);
  Chain = SP.getValue(1);
  SP = DAG.getNode(ISD::SUB, dl, XLenVT, SP, Size);
  if (Align)
    SP = DAG.getNode(ISD::AND, dl, VT, SP.getValue(0),
                     DAG.getSignedConstant(-Align->value(), dl, VT));

  // Set the real SP to the new value with a probing loop.
  Chain = DAG.getNode(YSXISD::PROBED_ALLOCA, dl, MVT::Other, Chain, SP);
  return DAG.getMergeValues({SP, Chain}, dl);
}

MachineBasicBlock *
YSXTargetLowering::emitDynamicProbedAlloc(MachineInstr &MI,
                                            MachineBasicBlock *MBB) const {
  MachineFunction &MF = *MBB->getParent();
  MachineBasicBlock::iterator MBBI = MI.getIterator();
  DebugLoc DL = MBB->findDebugLoc(MBBI);
  Register TargetReg = MI.getOperand(0).getReg();

  const YSXInstrInfo *TII = Subtarget.getInstrInfo();
  bool IsRV64 = Subtarget.is64Bit();
  Align StackAlign = Subtarget.getFrameLowering()->getStackAlign();
  const YSXTargetLowering *TLI = Subtarget.getTargetLowering();
  uint64_t ProbeSize = TLI->getStackProbeSize(MF, StackAlign);

  MachineFunction::iterator MBBInsertPoint = std::next(MBB->getIterator());
  MachineBasicBlock *LoopTestMBB =
      MF.CreateMachineBasicBlock(MBB->getBasicBlock());
  MF.insert(MBBInsertPoint, LoopTestMBB);
  MachineBasicBlock *ExitMBB = MF.CreateMachineBasicBlock(MBB->getBasicBlock());
  MF.insert(MBBInsertPoint, ExitMBB);
  Register SPReg = YSX::X2;
  Register ScratchReg =
      MF.getRegInfo().createVirtualRegister(&YSX::GPRRegClass);

  // ScratchReg = ProbeSize
  TII->movImm(*MBB, MBBI, DL, ScratchReg, ProbeSize, MachineInstr::NoFlags);

  // LoopTest:
  //   SUB SP, SP, ProbeSize
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL, TII->get(YSX::SUB), SPReg)
      .addReg(SPReg)
      .addReg(ScratchReg);

  //   s[d|w] zero, 0(sp)
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL,
          TII->get(IsRV64 ? YSX::SD : YSX::SW))
      .addReg(YSX::X0)
      .addReg(SPReg)
      .addImm(0);

  //  BLT TargetReg, SP, LoopTest
  BuildMI(*LoopTestMBB, LoopTestMBB->end(), DL, TII->get(YSX::BLT))
      .addReg(TargetReg)
      .addReg(SPReg)
      .addMBB(LoopTestMBB);

  // Adjust with: MV SP, TargetReg.
  BuildMI(*ExitMBB, ExitMBB->end(), DL, TII->get(YSX::ADDI), SPReg)
      .addReg(TargetReg)
      .addImm(0);

  ExitMBB->splice(ExitMBB->end(), MBB, std::next(MBBI), MBB->end());
  ExitMBB->transferSuccessorsAndUpdatePHIs(MBB);

  LoopTestMBB->addSuccessor(ExitMBB);
  LoopTestMBB->addSuccessor(LoopTestMBB);
  MBB->addSuccessor(LoopTestMBB);

  MI.eraseFromParent();
  MF.getInfo<YSXMachineFunctionInfo>()->setDynamicAllocation();
  return ExitMBB->begin()->getParent();
}

ArrayRef<MCPhysReg> YSXTargetLowering::getRoundingControlRegisters() const {
  return {};
}

bool YSXTargetLowering::shouldFoldMaskToVariableShiftPair(SDValue Y) const {
  EVT VT = Y.getValueType();

  if (VT.isVector())
    return false;

  return VT.getSizeInBits() <= Subtarget.getXLen();
}

bool YSXTargetLowering::isReassocProfitable(SelectionDAG &DAG, SDValue N0,
                                              SDValue N1) const {
  if (!N0.hasOneUse())
    return false;

  // Avoid reassociating expressions that can be lowered to vector
  // multiply accumulate (i.e. add (mul x, y), z)
  if (N0.getOpcode() == ISD::ADD && N1.getOpcode() == ISD::MUL &&
      (N0.getValueType().isVector() && Subtarget.hasVInstructions()))
    return false;

  return true;
}
