# RUN: llvm-mca -mtriple=riscv64 -mcpu=riscv-wb-bank-poc --dispatch=1 -iterations=1 < %s | FileCheck %s

vsetvli zero, zero, e32, m1, ta, ma
vfmacc.vf v5, ft0, v8
vfadd.vf v6, v10, ft1

# CHECK: Total Cycles:      6
# CHECK: Resources:
# CHECK-NEXT: [0]   - WBPOC_EXEC
# CHECK-NOT: SMX60_
# CHECK-NOT: RISC-V banked VRF writeback conflict
