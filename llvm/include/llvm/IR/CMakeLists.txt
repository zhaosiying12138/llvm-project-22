set(LLVM_TARGET_DEFINITIONS Attributes.td)
tablegen(LLVM Attributes.inc -gen-attrs)

set(LLVM_TARGET_DEFINITIONS RuntimeLibcalls.td)
tablegen(LLVM RuntimeLibcalls.inc -gen-runtime-libcalls)

set(YSX_AUTO_TD_ROOT ${LLVM_MAIN_SRC_DIR}/lib/Target/YuShuXin)
set(YSX_AUTO_TD_INTRINSICS_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/ysx-auto-td)
set(YSX_AUTO_TD_INTRINSICS_TD
  ${CMAKE_CURRENT_BINARY_DIR}/YSXGenAutoTinyVIntrinsics.td)
set(YSX_AUTO_TD_INTRINSICS_COVERAGE
  ${YSX_AUTO_TD_INTRINSICS_OUT_DIR}/coverage.md)

file(GLOB_RECURSE YSX_AUTO_TD_INTRINSICS_PY_DEPS CONFIGURE_DEPENDS
  ${YSX_AUTO_TD_ROOT}/auto-td/tools/*.py
  ${YSX_AUTO_TD_ROOT}/auto-td/tools/*/*.py
  )
file(GLOB_RECURSE YSX_AUTO_TD_INTRINSICS_YAML_DEPS CONFIGURE_DEPENDS
  ${YSX_AUTO_TD_ROOT}/auto-td/instructions/*.yaml
  ${YSX_AUTO_TD_ROOT}/auto-td/instructions/*/*.yaml
  ${YSX_AUTO_TD_ROOT}/auto-td/schema/*.yaml
  ${YSX_AUTO_TD_ROOT}/auto-td/taxonomy/*.yaml
  )
file(GLOB YSX_AUTO_TD_INTRINSICS_OPCODE_DEPS CONFIGURE_DEPENDS
  ${LLVM_MAIN_SRC_DIR}/../third_party/riscv-opcodes/extensions/*
  ${LLVM_MAIN_SRC_DIR}/../third_party/ysx-opcodes/extensions/*
  )
file(GLOB YSX_AUTO_TD_INTRINSICS_ARG_LUT_DEPS CONFIGURE_DEPENDS
  ${LLVM_MAIN_SRC_DIR}/../third_party/riscv-opcodes/arg_lut.csv
  ${LLVM_MAIN_SRC_DIR}/../third_party/ysx-opcodes/arg_lut.csv
  )

add_custom_command(
  OUTPUT
    ${YSX_AUTO_TD_INTRINSICS_TD}
    ${YSX_AUTO_TD_INTRINSICS_COVERAGE}
  COMMAND ${Python3_EXECUTABLE}
          ${YSX_AUTO_TD_ROOT}/auto-td/tools/ysx_auto_td_gen.py
          --ysx-root ${YSX_AUTO_TD_ROOT}
          --riscv-opcodes ${LLVM_MAIN_SRC_DIR}/../third_party/riscv-opcodes
          --ysx-opcodes ${LLVM_MAIN_SRC_DIR}/../third_party/ysx-opcodes
          --out-dir ${YSX_AUTO_TD_INTRINSICS_OUT_DIR}
          --coverage ${YSX_AUTO_TD_INTRINSICS_COVERAGE}
          --llvm-intrinsics-td ${YSX_AUTO_TD_INTRINSICS_TD}
  DEPENDS
          ${YSX_AUTO_TD_INTRINSICS_PY_DEPS}
          ${YSX_AUTO_TD_INTRINSICS_YAML_DEPS}
          ${YSX_AUTO_TD_INTRINSICS_OPCODE_DEPS}
          ${YSX_AUTO_TD_INTRINSICS_ARG_LUT_DEPS}
  VERBATIM
  COMMENT "Generating YSX LLVM intrinsic auto-td fragments"
  )

add_custom_target(LLVMYSXAutoTDIntrinsics
  DEPENDS
    ${YSX_AUTO_TD_INTRINSICS_TD}
    ${YSX_AUTO_TD_INTRINSICS_COVERAGE}
  )

list(APPEND LLVM_TARGET_DEPENDS ${YSX_AUTO_TD_INTRINSICS_TD})

set(LLVM_TARGET_DEFINITIONS Intrinsics.td)
tablegen(LLVM IntrinsicImpl.inc -gen-intrinsic-impl)
tablegen(LLVM IntrinsicEnums.inc -gen-intrinsic-enums)
tablegen(LLVM IntrinsicsAArch64.h -gen-intrinsic-enums -intrinsic-prefix=aarch64)
tablegen(LLVM IntrinsicsAMDGPU.h -gen-intrinsic-enums -intrinsic-prefix=amdgcn)
tablegen(LLVM IntrinsicsARM.h -gen-intrinsic-enums -intrinsic-prefix=arm)
tablegen(LLVM IntrinsicsBPF.h -gen-intrinsic-enums -intrinsic-prefix=bpf)
tablegen(LLVM IntrinsicsDirectX.h -gen-intrinsic-enums -intrinsic-prefix=dx)
tablegen(LLVM IntrinsicsHexagon.h -gen-intrinsic-enums -intrinsic-prefix=hexagon)
tablegen(LLVM IntrinsicsLoongArch.h -gen-intrinsic-enums -intrinsic-prefix=loongarch)
tablegen(LLVM IntrinsicsMips.h -gen-intrinsic-enums -intrinsic-prefix=mips)
tablegen(LLVM IntrinsicsNVPTX.h -gen-intrinsic-enums -intrinsic-prefix=nvvm)
tablegen(LLVM IntrinsicsPowerPC.h -gen-intrinsic-enums -intrinsic-prefix=ppc)
tablegen(LLVM IntrinsicsR600.h -gen-intrinsic-enums -intrinsic-prefix=r600)
tablegen(LLVM IntrinsicsRISCV.h -gen-intrinsic-enums -intrinsic-prefix=riscv)
tablegen(LLVM IntrinsicsSPIRV.h -gen-intrinsic-enums -intrinsic-prefix=spv)
tablegen(LLVM IntrinsicsS390.h -gen-intrinsic-enums -intrinsic-prefix=s390)
tablegen(LLVM IntrinsicsWebAssembly.h -gen-intrinsic-enums -intrinsic-prefix=wasm)
tablegen(LLVM IntrinsicsX86.h -gen-intrinsic-enums -intrinsic-prefix=x86)
tablegen(LLVM IntrinsicsXCore.h -gen-intrinsic-enums -intrinsic-prefix=xcore)
tablegen(LLVM IntrinsicsVE.h -gen-intrinsic-enums -intrinsic-prefix=ve)
tablegen(LLVM IntrinsicsYSX.h -gen-intrinsic-enums -intrinsic-prefix=ysx)
add_public_tablegen_target(intrinsics_gen)
