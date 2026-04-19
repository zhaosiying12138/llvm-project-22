# RUN: not llvm-mc -triple=ysx64 -mattr=+f %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+c %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+zbb %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+vxrm-pipeline-flush %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+log-vrgather %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+single-element-vec-fp64 %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+prefer-vsetvli-over-read-vlenb %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+andes45 %s 2>&1 | FileCheck %s
# RUN: printf "add a0, a0, a1\n" | llvm-mc -triple=ysx64 -mattr=-f,-v,-zbb -
# RUN: printf "add a0, a0, a1\n" | llvm-mc -triple=ysx64 -mattr=+reserve-x5 -
# RUN: llvm-mc -triple=ysx64 -mattr=help 2>&1 | FileCheck %s --check-prefix=HELP
# RUN: not llvm-mc -triple=ysx64 %s 2>&1 | FileCheck %s --check-prefix=ARCH
# RUN: printf ".option arch, rv64ima\nadd a0, a0, a1\n" | llvm-mc -triple=ysx64 -
# RUN: printf ".option arch, rv32ima\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=FULLARCH
# RUN: printf ".option arch, rv64ima_zbb\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=FULLARCH
# RUN: printf ".option arch, +f\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option arch, +c\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option arch, +zbb\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option arch, +v\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option rvc\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option push\n.option arch, +f\n.option pop\nadd a0, a0, a1\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option norvc\nadd a0, a0, a1\n" | llvm-mc -triple=ysx64 -
# RUN: printf "csrr a0, fflags\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, frm\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, fcsr\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, vtype\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, vl\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, vlenb\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, vxsat\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, vxrm\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "fence.i\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, 0\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, mstatus\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, ssp\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrr a0, seed\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "csrrw a0, 0, a1\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "rdcycle a0\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "rdtime a1\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "rdinstret a2\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "mret\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "sret\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "wfi\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "dret\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "sfence.vma\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf "hfence.vvma\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=UNSUP-INST
# RUN: printf ".insn 0x2, 0x0001\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN16
# RUN: printf ".insn r OP_FP, 0, 0, x1, x2, x3\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-OPCODE
# RUN: printf ".insn r OP_V, 0, 0, x1, x2, x3\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-OPCODE
# RUN: printf ".insn r MADD, 0, 0, x1, x2, x3\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-OPCODE
# RUN: printf ".insn r CUSTOM_0, 0, 0, x1, x2, x3\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-OPCODE
# RUN: printf ".insn r 83, 0, 0, x1, x2, x3\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-OPCODE
# RUN: printf ".insn r 87, 0, 0, x1, x2, x3\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-OPCODE
# RUN: printf ".insn r 67, 0, 0, x1, x2, x3\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-OPCODE
# RUN: printf ".insn r 11, 0, 0, x1, x2, x3\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-OPCODE
# RUN: printf ".insn r4 MADD, 0, 0, x1, x2, x3, x4\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=INSN-FORMAT
# RUN: printf ".reloc ., R_RISCV_RVC_BRANCH, sym\n" | not llvm-mc -triple=ysx64 -filetype=obj -o /dev/null - 2>&1 | FileCheck %s --check-prefix=RELOC
# RUN: printf ".reloc ., R_RISCV_RVC_JUMP, sym\n" | not llvm-mc -triple=ysx64 -filetype=obj -o /dev/null - 2>&1 | FileCheck %s --check-prefix=RELOC
# RUN: printf ".reloc ., R_RISCV_VENDOR, sym\n" | not llvm-mc -triple=ysx64 -filetype=obj -o /dev/null - 2>&1 | FileCheck %s --check-prefix=RELOC
# RUN: printf ".reloc ., R_RISCV_CUSTOM192, sym\n" | not llvm-mc -triple=ysx64 -filetype=obj -o /dev/null - 2>&1 | FileCheck %s --check-prefix=RELOC
# RUN: printf ".reloc ., R_RISCV_QC_ABS20_U, sym\n" | not llvm-mc -triple=ysx64 -filetype=obj -o /dev/null - 2>&1 | FileCheck %s --check-prefix=RELOC
# RUN: printf ".reloc ., R_RISCV_NDS_BRANCH_10, sym\n" | not llvm-mc -triple=ysx64 -filetype=obj -o /dev/null - 2>&1 | FileCheck %s --check-prefix=RELOC
# RUN: printf ".reloc ., R_RISCV_CHERIOT1_COMPARTMENT_HI, sym\n" | not llvm-mc -triple=ysx64 -filetype=obj -o /dev/null - 2>&1 | FileCheck %s --check-prefix=RELOC

# CHECK: LLVM ERROR: YSX only supports the rv64ima ISA

.option arch, rv64gc
# ARCH: error: invalid arch name 'rv64gc', YSX only supports arch string rv64ima
# OPTION: error: YSX only supports arch string rv64ima
# FULLARCH: error: invalid arch name
# UNSUP-INST: error: unrecognized instruction mnemonic
# INSN16: error: 16-bit instruction encodings are not allowed
# INSN-OPCODE: error: opcode must be a retained rv64ima major opcode name or value in the range
# INSN-FORMAT: error: invalid instruction format
# RELOC: error: unknown relocation name
# HELP: Available features for this target:
# HELP-NOT: 32bit
# HELP-NOT: log-vrgather
# HELP: 64bit
# HELP: zmmul
# HELP: Use +feature to enable a feature
