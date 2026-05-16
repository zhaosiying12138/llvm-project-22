# RUN: llvm-mca -mtriple=riscv64 -mcpu=riscv-wb-bank-poc --dispatch=1 -iterations=1 < %s | FileCheck %s

vfmacc.vf v5, ft0, v8
vadd.vv v6, v12, v13
vfadd.vf v7, v10, ft1
vadd.vv v0, v16, v18
addi zero, zero, 0
vmv.v.v v11, v20
vmv.v.v v14, v0

# CHECK: Total Cycles:      9
# CHECK: {{^ 1 +4 +1.00 +vfmacc.vf}}
# CHECK: {{^ 1 +2 +1.00 +vadd.vv}}
# CHECK: {{^ 1 +3 +1.00 +vfadd.vf}}
# CHECK: {{^ 1 +2 +1.00 +vadd.vv}}
# CHECK: {{^ 1 +1 +1.00 +nop}}
# CHECK: {{^ 1 +1 +1.00 +vmv.v.v}}
# CHECK: Resources:
# CHECK-NEXT: [0]   - WBPOC_EXEC
# CHECK-NOT: SMX60_
# CHECK-NOT: RISC-V banked VRF writeback conflict
