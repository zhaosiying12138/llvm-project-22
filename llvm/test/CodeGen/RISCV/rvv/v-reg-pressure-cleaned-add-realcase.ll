; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -debug-only=riscv-prera-sched-strategy < %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=POLICY

; Cleaned IR derived from realtest/add.ll. The original wrapper/debug/global
; inputs are removed so this test focuses on the element-wise RVV scheduling
; shape: many independent loads followed by pure vector operations and stores.

; CHECK-LABEL: cleaned_add_realcase_8:
; CHECK-NOT: vs{{[1248]}}r.v
; CHECK-NOT: vl{{[1248]}}r.v
; CHECK: ret
; CHECK-LABEL: cleaned_fused_realcase_8:
; CHECK-NOT: vs{{[1248]}}r.v
; CHECK-NOT: vl{{[1248]}}r.v
; CHECK: ret

; POLICY-DAG: RISCV RVV pressure DAG sched: function=cleaned_add_realcase_8 pressure-region=1 clean-region=1 reduce-region=0 track-pressure=1 top-down=1
; POLICY-DAG: RISCV RVV pressure DAG sched: function=cleaned_add_realcase_8 cluster-clean-region=1
; POLICY-DAG: RISCV RVV pressure DAG sched: function=cleaned_fused_realcase_8 pressure-region=1 clean-region=1 reduce-region=0 track-pressure=1 top-down=1
; POLICY-DAG: RISCV RVV pressure DAG sched: function=cleaned_fused_realcase_8 cluster-clean-region=1

define void @cleaned_add_realcase_8(ptr noalias %a, ptr noalias %b,
                                    ptr noalias %out) {
  %ap0 = getelementptr <128 x float>, ptr %a, i64 0
  %bp0 = getelementptr <128 x float>, ptr %b, i64 0
  %av0 = load <128 x float>, ptr %ap0, align 4
  %bv0 = load <128 x float>, ptr %bp0, align 4
  %sum0 = fadd <128 x float> %av0, %bv0
  %op0 = getelementptr <128 x float>, ptr %out, i64 0
  store <128 x float> %sum0, ptr %op0, align 4

  %ap1 = getelementptr <128 x float>, ptr %a, i64 1
  %bp1 = getelementptr <128 x float>, ptr %b, i64 1
  %av1 = load <128 x float>, ptr %ap1, align 4
  %bv1 = load <128 x float>, ptr %bp1, align 4
  %sum1 = fadd <128 x float> %av1, %bv1
  %op1 = getelementptr <128 x float>, ptr %out, i64 1
  store <128 x float> %sum1, ptr %op1, align 4

  %ap2 = getelementptr <128 x float>, ptr %a, i64 2
  %bp2 = getelementptr <128 x float>, ptr %b, i64 2
  %av2 = load <128 x float>, ptr %ap2, align 4
  %bv2 = load <128 x float>, ptr %bp2, align 4
  %sum2 = fadd <128 x float> %av2, %bv2
  %op2 = getelementptr <128 x float>, ptr %out, i64 2
  store <128 x float> %sum2, ptr %op2, align 4

  %ap3 = getelementptr <128 x float>, ptr %a, i64 3
  %bp3 = getelementptr <128 x float>, ptr %b, i64 3
  %av3 = load <128 x float>, ptr %ap3, align 4
  %bv3 = load <128 x float>, ptr %bp3, align 4
  %sum3 = fadd <128 x float> %av3, %bv3
  %op3 = getelementptr <128 x float>, ptr %out, i64 3
  store <128 x float> %sum3, ptr %op3, align 4

  %ap4 = getelementptr <128 x float>, ptr %a, i64 4
  %bp4 = getelementptr <128 x float>, ptr %b, i64 4
  %av4 = load <128 x float>, ptr %ap4, align 4
  %bv4 = load <128 x float>, ptr %bp4, align 4
  %sum4 = fadd <128 x float> %av4, %bv4
  %op4 = getelementptr <128 x float>, ptr %out, i64 4
  store <128 x float> %sum4, ptr %op4, align 4

  %ap5 = getelementptr <128 x float>, ptr %a, i64 5
  %bp5 = getelementptr <128 x float>, ptr %b, i64 5
  %av5 = load <128 x float>, ptr %ap5, align 4
  %bv5 = load <128 x float>, ptr %bp5, align 4
  %sum5 = fadd <128 x float> %av5, %bv5
  %op5 = getelementptr <128 x float>, ptr %out, i64 5
  store <128 x float> %sum5, ptr %op5, align 4

  %ap6 = getelementptr <128 x float>, ptr %a, i64 6
  %bp6 = getelementptr <128 x float>, ptr %b, i64 6
  %av6 = load <128 x float>, ptr %ap6, align 4
  %bv6 = load <128 x float>, ptr %bp6, align 4
  %sum6 = fadd <128 x float> %av6, %bv6
  %op6 = getelementptr <128 x float>, ptr %out, i64 6
  store <128 x float> %sum6, ptr %op6, align 4

  %ap7 = getelementptr <128 x float>, ptr %a, i64 7
  %bp7 = getelementptr <128 x float>, ptr %b, i64 7
  %av7 = load <128 x float>, ptr %ap7, align 4
  %bv7 = load <128 x float>, ptr %bp7, align 4
  %sum7 = fadd <128 x float> %av7, %bv7
  %op7 = getelementptr <128 x float>, ptr %out, i64 7
  store <128 x float> %sum7, ptr %op7, align 4

  ret void
}

define void @cleaned_fused_realcase_8(ptr noalias %a, ptr noalias %b,
                                      ptr noalias %c, ptr noalias %out) {
  %ap0 = getelementptr <128 x float>, ptr %a, i64 0
  %bp0 = getelementptr <128 x float>, ptr %b, i64 0
  %cp0 = getelementptr <128 x float>, ptr %c, i64 0
  %av0 = load <128 x float>, ptr %ap0, align 4
  %bv0 = load <128 x float>, ptr %bp0, align 4
  %cv0 = load <128 x float>, ptr %cp0, align 4
  %mul0 = fmul <128 x float> %av0, %bv0
  %sum0 = fadd <128 x float> %mul0, %cv0
  %op0 = getelementptr <128 x float>, ptr %out, i64 0
  store <128 x float> %sum0, ptr %op0, align 4

  %ap1 = getelementptr <128 x float>, ptr %a, i64 1
  %bp1 = getelementptr <128 x float>, ptr %b, i64 1
  %cp1 = getelementptr <128 x float>, ptr %c, i64 1
  %av1 = load <128 x float>, ptr %ap1, align 4
  %bv1 = load <128 x float>, ptr %bp1, align 4
  %cv1 = load <128 x float>, ptr %cp1, align 4
  %mul1 = fmul <128 x float> %av1, %bv1
  %sum1 = fadd <128 x float> %mul1, %cv1
  %op1 = getelementptr <128 x float>, ptr %out, i64 1
  store <128 x float> %sum1, ptr %op1, align 4

  %ap2 = getelementptr <128 x float>, ptr %a, i64 2
  %bp2 = getelementptr <128 x float>, ptr %b, i64 2
  %cp2 = getelementptr <128 x float>, ptr %c, i64 2
  %av2 = load <128 x float>, ptr %ap2, align 4
  %bv2 = load <128 x float>, ptr %bp2, align 4
  %cv2 = load <128 x float>, ptr %cp2, align 4
  %mul2 = fmul <128 x float> %av2, %bv2
  %sum2 = fadd <128 x float> %mul2, %cv2
  %op2 = getelementptr <128 x float>, ptr %out, i64 2
  store <128 x float> %sum2, ptr %op2, align 4

  %ap3 = getelementptr <128 x float>, ptr %a, i64 3
  %bp3 = getelementptr <128 x float>, ptr %b, i64 3
  %cp3 = getelementptr <128 x float>, ptr %c, i64 3
  %av3 = load <128 x float>, ptr %ap3, align 4
  %bv3 = load <128 x float>, ptr %bp3, align 4
  %cv3 = load <128 x float>, ptr %cp3, align 4
  %mul3 = fmul <128 x float> %av3, %bv3
  %sum3 = fadd <128 x float> %mul3, %cv3
  %op3 = getelementptr <128 x float>, ptr %out, i64 3
  store <128 x float> %sum3, ptr %op3, align 4

  %ap4 = getelementptr <128 x float>, ptr %a, i64 4
  %bp4 = getelementptr <128 x float>, ptr %b, i64 4
  %cp4 = getelementptr <128 x float>, ptr %c, i64 4
  %av4 = load <128 x float>, ptr %ap4, align 4
  %bv4 = load <128 x float>, ptr %bp4, align 4
  %cv4 = load <128 x float>, ptr %cp4, align 4
  %mul4 = fmul <128 x float> %av4, %bv4
  %sum4 = fadd <128 x float> %mul4, %cv4
  %op4 = getelementptr <128 x float>, ptr %out, i64 4
  store <128 x float> %sum4, ptr %op4, align 4

  %ap5 = getelementptr <128 x float>, ptr %a, i64 5
  %bp5 = getelementptr <128 x float>, ptr %b, i64 5
  %cp5 = getelementptr <128 x float>, ptr %c, i64 5
  %av5 = load <128 x float>, ptr %ap5, align 4
  %bv5 = load <128 x float>, ptr %bp5, align 4
  %cv5 = load <128 x float>, ptr %cp5, align 4
  %mul5 = fmul <128 x float> %av5, %bv5
  %sum5 = fadd <128 x float> %mul5, %cv5
  %op5 = getelementptr <128 x float>, ptr %out, i64 5
  store <128 x float> %sum5, ptr %op5, align 4

  %ap6 = getelementptr <128 x float>, ptr %a, i64 6
  %bp6 = getelementptr <128 x float>, ptr %b, i64 6
  %cp6 = getelementptr <128 x float>, ptr %c, i64 6
  %av6 = load <128 x float>, ptr %ap6, align 4
  %bv6 = load <128 x float>, ptr %bp6, align 4
  %cv6 = load <128 x float>, ptr %cp6, align 4
  %mul6 = fmul <128 x float> %av6, %bv6
  %sum6 = fadd <128 x float> %mul6, %cv6
  %op6 = getelementptr <128 x float>, ptr %out, i64 6
  store <128 x float> %sum6, ptr %op6, align 4

  %ap7 = getelementptr <128 x float>, ptr %a, i64 7
  %bp7 = getelementptr <128 x float>, ptr %b, i64 7
  %cp7 = getelementptr <128 x float>, ptr %c, i64 7
  %av7 = load <128 x float>, ptr %ap7, align 4
  %bv7 = load <128 x float>, ptr %bp7, align 4
  %cv7 = load <128 x float>, ptr %cp7, align 4
  %mul7 = fmul <128 x float> %av7, %bv7
  %sum7 = fadd <128 x float> %mul7, %cv7
  %op7 = getelementptr <128 x float>, ptr %out, i64 7
  store <128 x float> %sum7, ptr %op7, align 4

  ret void
}
