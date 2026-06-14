add_llvm_component_group(YSX)

set(LLVM_TARGET_DEFINITIONS YSX.td)

set(YSX_AUTO_TD_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/auto-td)
set(YSX_AUTO_TD_TINYF_INSTRINFO ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyFInstrInfo.inc)
set(YSX_AUTO_TD_TINYV_INSTRINFO ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVInstrInfo.inc)
set(YSX_AUTO_TD_TINYV_PSEUDOS ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVPseudos.inc)
set(YSX_AUTO_TD_TINYV_PATTERNS ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVPatterns.inc)
set(YSX_AUTO_TD_TINYV_BUILTINS ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVBuiltins.inc)
set(YSX_AUTO_TD_COVERAGE ${YSX_AUTO_TD_OUT_DIR}/coverage.md)

set(YSX_AUTO_TD_OUTPUTS
  ${YSX_AUTO_TD_TINYF_INSTRINFO}
  ${YSX_AUTO_TD_TINYV_INSTRINFO}
  ${YSX_AUTO_TD_TINYV_PSEUDOS}
  ${YSX_AUTO_TD_TINYV_PATTERNS}
  ${YSX_AUTO_TD_TINYV_BUILTINS}
  )

file(GLOB_RECURSE YSX_AUTO_TD_PY_DEPS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/tools/*.py
  ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/tools/*/*.py
  )
file(GLOB_RECURSE YSX_AUTO_TD_YAML_DEPS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/instructions/*.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/instructions/*/*.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/schema/*.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/taxonomy/*.yaml
  )
file(GLOB YSX_AUTO_TD_OPCODE_DEPS CONFIGURE_DEPENDS
  ${LLVM_MAIN_SRC_DIR}/../third_party/riscv-opcodes/extensions/*
  ${LLVM_MAIN_SRC_DIR}/../third_party/ysx-opcodes/extensions/*
  )
file(GLOB YSX_AUTO_TD_ARG_LUT_DEPS CONFIGURE_DEPENDS
  ${LLVM_MAIN_SRC_DIR}/../third_party/riscv-opcodes/arg_lut.csv
  ${LLVM_MAIN_SRC_DIR}/../third_party/ysx-opcodes/arg_lut.csv
  )

add_custom_command(
  OUTPUT ${YSX_AUTO_TD_OUTPUTS} ${YSX_AUTO_TD_COVERAGE}
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/auto-td/tools/ysx_auto_td_gen.py
          --ysx-root ${CMAKE_CURRENT_SOURCE_DIR}
          --riscv-opcodes ${LLVM_MAIN_SRC_DIR}/../third_party/riscv-opcodes
          --ysx-opcodes ${LLVM_MAIN_SRC_DIR}/../third_party/ysx-opcodes
          --out-dir ${YSX_AUTO_TD_OUT_DIR}
          --coverage ${YSX_AUTO_TD_COVERAGE}
  DEPENDS
          ${YSX_AUTO_TD_PY_DEPS}
          ${YSX_AUTO_TD_YAML_DEPS}
          ${YSX_AUTO_TD_OPCODE_DEPS}
          ${YSX_AUTO_TD_ARG_LUT_DEPS}
  VERBATIM
  COMMENT "Generating YSX auto TableGen fragments"
  )

include_directories(${YSX_AUTO_TD_OUT_DIR})

list(APPEND LLVM_TARGET_DEPENDS
  ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyFInstrInfo.inc
  ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVInstrInfo.inc
  ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVPseudos.inc
  ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVPatterns.inc
  ${YSX_AUTO_TD_OUT_DIR}/YSXGenAutoTinyVBuiltins.inc
  )

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
  YSXInstrInfo.cpp
  YSXISelDAGToDAG.cpp
  YSXISelLowering.cpp
  YSXLateBranchOpt.cpp
  YSXMachineFunctionInfo.cpp
  YSXPostRAExpandPseudoInsts.cpp
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
