# RUN: not llvm-mc -triple=ysx64 -mattr=+f %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+c %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 -mattr=+zbb %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple=ysx64 %s 2>&1 | FileCheck %s --check-prefix=ARCH

# CHECK: LLVM ERROR: YSX only supports the rv64ima ISA

.option arch, rv64gc
# ARCH: error: YSX only supports arch string rv64ima
