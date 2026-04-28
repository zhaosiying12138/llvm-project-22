# RUN: llvm-mca -mtriple=riscv64 -mcpu=rvv-war-demo -iterations=1 -riscv-rvv-war-hazard-model < %s | FileCheck %s

# CHECK:      RVV WAR Hazard
# CHECK-NEXT: Total hazards: 0
# CHECK-NEXT: Blocked issue events: 0
# CHECK-NEXT: Blocked issue cycles: 0

ld t0, 0(a0)
add t1, s0, t0
add s0, t2, t3
