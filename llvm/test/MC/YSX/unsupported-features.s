# RUN: not llvm-mc -triple=ysx64 -mattr=+f %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+c %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+zbb %s 2>&1 | FileCheck %s
# RUN: printf "add a0, a0, a1\n" | llvm-mc -triple=ysx64 -mattr=-f,-v,-zbb -
# RUN: llvm-mc -triple=ysx64 -mattr=help 2>&1 | FileCheck %s --check-prefix=HELP
# RUN: not llvm-mc -triple=ysx64 %s 2>&1 | FileCheck %s --check-prefix=ARCH
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

# CHECK: LLVM ERROR: YSX only supports the rv64ima ISA

.option arch, rv64gc
# ARCH: error: YSX only supports arch string rv64ima
# OPTION: error: YSX only supports arch string rv64ima
# UNSUP-INST: error: unrecognized instruction mnemonic
# INSN16: error: compressed instructions are not allowed
# HELP: Available features for this target:
# HELP-NOT: 32bit
# HELP-NOT: log-vrgather
# HELP: 64bit
# HELP: zmmul
# HELP: Use +feature to enable a feature
