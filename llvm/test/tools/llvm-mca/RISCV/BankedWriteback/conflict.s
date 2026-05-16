# RUN: not llvm-mca -mtriple=riscv64 -mcpu=riscv-wb-bank-poc --dispatch=1 -iterations=1 < %s 2>&1 | FileCheck %s

vsetvli zero, zero, e32, m1, ta, ma
vfmacc.vf v5, ft0, v8
vfadd.vf v7, v10, ft1

# CHECK: LLVM ERROR: RISC-V banked VRF writeback conflict
# CHECK-SAME: bank 0
# CHECK-SAME: multiple vector writes
# CHECK-SAME: static writeback cycle 5
# CHECK-SAME: v7
# CHECK-SAME: v5
