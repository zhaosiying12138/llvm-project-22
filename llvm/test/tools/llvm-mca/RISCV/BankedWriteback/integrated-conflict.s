# RUN: not llvm-mca -mtriple=riscv64 -mcpu=riscv-wb-bank-poc --dispatch=1 -iterations=1 < %s 2>&1 | FileCheck %s

vfmacc.vf v5, ft0, v8
vfadd.vf v7, v10, ft1
vadd.vv v6, v12, v13
vadd.vv v9, v16, v18
vmv.v.v v11, v20
vmv.v.v v14, v9

# CHECK: LLVM ERROR: RISC-V banked VRF writeback conflict
# CHECK-SAME: bank 0
# CHECK-SAME: static writeback cycle 4
# CHECK-SAME: v7
# CHECK-SAME: v5
