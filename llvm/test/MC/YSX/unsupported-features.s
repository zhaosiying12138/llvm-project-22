# RUN: not llvm-mc -triple=ysx64 -mattr=+f %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+c %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+zbb %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 %s 2>&1 | FileCheck %s --check-prefix=ARCH
# RUN: printf ".option arch, +f\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option arch, +c\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option arch, +zbb\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option arch, +v\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option rvc\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option push\n.option arch, +f\n.option pop\nadd a0, a0, a1\n" | not llvm-mc -triple=ysx64 - 2>&1 | FileCheck %s --check-prefix=OPTION
# RUN: printf ".option norvc\nadd a0, a0, a1\n" | llvm-mc -triple=ysx64 -

# CHECK: LLVM ERROR: YSX only supports the rv64ima ISA

.option arch, rv64gc
# ARCH: error: YSX only supports arch string rv64ima
# OPTION: error: YSX only supports arch string rv64ima
