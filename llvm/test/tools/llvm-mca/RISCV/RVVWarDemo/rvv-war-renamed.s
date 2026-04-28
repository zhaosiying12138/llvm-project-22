# RUN: llvm-mca -mtriple=riscv64 -mcpu=rvv-war-demo -iterations=1 -riscv-rvv-war-hazard-model < %s | FileCheck %s

# CHECK:      Total Cycles:      6
# CHECK:      RVV WAR Hazard
# CHECK-NEXT: Total hazards: 0
# CHECK-NEXT: Blocked issue events: 0
# CHECK-NEXT: Blocked issue cycles: 0

vsetvli t0, zero, e32, m1, ta, ma
vle32.v v12, (a0)
vadd.vv v10, v8, v12
vadd.vv v20, v14, v15
vadd.vv v18, v20, v19
