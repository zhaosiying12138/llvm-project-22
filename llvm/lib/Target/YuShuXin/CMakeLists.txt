add_llvm_component_group(YSX)

set(LLVM_TARGET_DEFINITIONS YSX.td)

tablegen(LLVM YSXGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM YSXGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM YSXGenMacroFusion.inc -gen-macro-fusion-pred)
tablegen(LLVM YSXGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM YSXGenDisassemblerTables.inc -gen-disassembler
              --specialize-decoders-per-bitwidth)
tablegen(LLVM YSXGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM YSXGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM YSXGenMCPseudoLowering.inc -gen-pseudo-lowering)
tablegen(LLVM YSXGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM YSXGenSearchableTables.inc -gen-searchable-tables)
tablegen(LLVM YSXGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM YSXGenExegesis.inc -gen-exegesis)
tablegen(LLVM YSXGenSDNodeInfo.inc -gen-sd-node-info)

add_public_tablegen_target(YSXCommonTableGen)

add_llvm_target(YSXCodeGen
  YSXAsmPrinter.cpp
  YSXCallingConv.cpp
  YSXCodeGenPrepare.cpp
  YSXConstantPoolValue.cpp
  YSXDeadRegisterDefinitions.cpp
  YSXExpandAtomicPseudoInsts.cpp
  YSXExpandPseudoInsts.cpp
  YSXFrameLowering.cpp
  YSXInsertReadWriteCSR.cpp
  YSXInstrInfo.cpp
  YSXISelDAGToDAG.cpp
  YSXISelLowering.cpp
  YSXIndirectBranchTracking.cpp
  YSXLandingPadSetup.cpp
  YSXLateBranchOpt.cpp
  YSXMachineFunctionInfo.cpp
  YSXPostRAExpandPseudoInsts.cpp
  YSXPromoteConstant.cpp
  YSXRegisterInfo.cpp
  YSXSelectionDAGInfo.cpp
  YSXSubtarget.cpp
  YSXTargetMachine.cpp
  YSXTargetObjectFile.cpp
  PARTIAL_SOURCES_INTENDED

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  IPO
  MC
  Passes
  YSXDesc
  YSXInfo
  Scalar
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils
  Vectorize

  ADD_TO_COMPONENT
  YSX
  )

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(MCA)
add_subdirectory(TargetInfo)
