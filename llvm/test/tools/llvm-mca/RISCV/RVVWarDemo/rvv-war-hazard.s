# RUN: llvm-mca -mtriple=riscv64 -mcpu=rvv-war-demo -iterations=1 < %s | FileCheck %s --check-prefix=DISABLED
# RUN: llvm-mca -mtriple=riscv64 -mcpu=rvv-war-demo -iterations=1 -riscv-rvv-war-hazard-model < %s | FileCheck %s --check-prefix=ENABLED

# DISABLED:     Total Cycles:      6
# DISABLED-NOT: RVV WAR Hazard

# ENABLED:      Total Cycles:      7
# ENABLED:      RVV WAR Hazard
# ENABLED-NEXT: Total hazards: {{[1-9][0-9]*}}
# ENABLED-NEXT: Blocked issue events: {{[1-9][0-9]*}}
# ENABLED-NEXT: Blocked issue cycles: {{[1-9][0-9]*}}
# ENABLED:      v8
# ENABLED:      waits for

vsetvli t0, zero, e32, m1, ta, ma
vle32.v v12, (a0)
vadd.vv v10, v8, v12
vadd.vv v8, v14, v15
vadd.vv v18, v8, v19
