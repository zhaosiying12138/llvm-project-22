; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -debug-only=riscv-prera-sched-strategy < %s -o /dev/null 2>&1 | FileCheck %s

; CHECK: RISCV RVV pressure DAG sched: function=addrspace_alias_vector_add pressure-region=0 clean-region=0 reduce-region=0
; CHECK-NOT: RISCV RVV pressure DAG sched: function=addrspace_alias_vector_add cluster-clean-region=1

define void @addrspace_alias_vector_add(ptr addrspace(1) %a,
                                        ptr noalias %b) {
  %a0 = addrspacecast ptr addrspace(1) %a to ptr

  %ap0 = getelementptr <128 x float>, ptr addrspace(1) %a, i64 0
  %ap1 = getelementptr <128 x float>, ptr addrspace(1) %a, i64 1
  %ap2 = getelementptr <128 x float>, ptr addrspace(1) %a, i64 2
  %ap3 = getelementptr <128 x float>, ptr addrspace(1) %a, i64 3
  %ap4 = getelementptr <128 x float>, ptr addrspace(1) %a, i64 4
  %ap5 = getelementptr <128 x float>, ptr addrspace(1) %a, i64 5
  %ap6 = getelementptr <128 x float>, ptr addrspace(1) %a, i64 6
  %ap7 = getelementptr <128 x float>, ptr addrspace(1) %a, i64 7

  %op0 = getelementptr <128 x float>, ptr %a0, i64 0
  %op1 = getelementptr <128 x float>, ptr %a0, i64 1
  %op2 = getelementptr <128 x float>, ptr %a0, i64 2
  %op3 = getelementptr <128 x float>, ptr %a0, i64 3
  %op4 = getelementptr <128 x float>, ptr %a0, i64 4
  %op5 = getelementptr <128 x float>, ptr %a0, i64 5
  %op6 = getelementptr <128 x float>, ptr %a0, i64 6
  %op7 = getelementptr <128 x float>, ptr %a0, i64 7

  %bp0 = getelementptr <128 x float>, ptr %b, i64 0
  %bp1 = getelementptr <128 x float>, ptr %b, i64 1
  %bp2 = getelementptr <128 x float>, ptr %b, i64 2
  %bp3 = getelementptr <128 x float>, ptr %b, i64 3
  %bp4 = getelementptr <128 x float>, ptr %b, i64 4
  %bp5 = getelementptr <128 x float>, ptr %b, i64 5
  %bp6 = getelementptr <128 x float>, ptr %b, i64 6
  %bp7 = getelementptr <128 x float>, ptr %b, i64 7

  %av0 = load <128 x float>, ptr addrspace(1) %ap0, align 4
  %av1 = load <128 x float>, ptr addrspace(1) %ap1, align 4
  %av2 = load <128 x float>, ptr addrspace(1) %ap2, align 4
  %av3 = load <128 x float>, ptr addrspace(1) %ap3, align 4
  %av4 = load <128 x float>, ptr addrspace(1) %ap4, align 4
  %av5 = load <128 x float>, ptr addrspace(1) %ap5, align 4
  %av6 = load <128 x float>, ptr addrspace(1) %ap6, align 4
  %av7 = load <128 x float>, ptr addrspace(1) %ap7, align 4

  %bv0 = load <128 x float>, ptr %bp0, align 4
  %bv1 = load <128 x float>, ptr %bp1, align 4
  %bv2 = load <128 x float>, ptr %bp2, align 4
  %bv3 = load <128 x float>, ptr %bp3, align 4
  %bv4 = load <128 x float>, ptr %bp4, align 4
  %bv5 = load <128 x float>, ptr %bp5, align 4
  %bv6 = load <128 x float>, ptr %bp6, align 4
  %bv7 = load <128 x float>, ptr %bp7, align 4

  %sum0 = fadd <128 x float> %av0, %bv0
  %sum1 = fadd <128 x float> %av1, %bv1
  %sum2 = fadd <128 x float> %av2, %bv2
  %sum3 = fadd <128 x float> %av3, %bv3
  %sum4 = fadd <128 x float> %av4, %bv4
  %sum5 = fadd <128 x float> %av5, %bv5
  %sum6 = fadd <128 x float> %av6, %bv6
  %sum7 = fadd <128 x float> %av7, %bv7

  store <128 x float> %sum0, ptr %op0, align 4
  store <128 x float> %sum1, ptr %op1, align 4
  store <128 x float> %sum2, ptr %op2, align 4
  store <128 x float> %sum3, ptr %op3, align 4
  store <128 x float> %sum4, ptr %op4, align 4
  store <128 x float> %sum5, ptr %op5, align 4
  store <128 x float> %sum6, ptr %op6, align 4
  store <128 x float> %sum7, ptr %op7, align 4
  ret void
}
