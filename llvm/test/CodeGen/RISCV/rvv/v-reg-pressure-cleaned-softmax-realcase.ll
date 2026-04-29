; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O3 -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -debug-only=riscv-prera-sched-strategy < %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=POLICY

; Cleaned IR derived from realtest/softmax.ll. Debug intrinsics, kernel
; wrappers, unsupported metadata IDs, and non-essential address-space scaffolding
; are removed; the remaining tiny case preserves the reduce -> exp2(log2e*x) ->
; normalize shape needed by the RVV pressure tests.

declare float @llvm.vector.reduce.fmax.v128f32(<128 x float>)
declare float @llvm.vector.reduce.fadd.v128f32(float, <128 x float>)
declare <128 x float> @llvm.exp2.v128f32(<128 x float>)

; CHECK-LABEL: cleaned_softmax_tiny_realcase:
; CHECK: vfredmax.vs
; CHECK: yushuxin.vfexp
; CHECK: vfredusum.vs
; CHECK-NOT: exp2f
; CHECK: ret

; POLICY: RISCV RVV pressure DAG sched: function=cleaned_softmax_tiny_realcase pressure-region=0 clean-region=0 reduce-region=0

define void @cleaned_softmax_tiny_realcase(ptr noalias %in, ptr noalias %out,
                                           ptr noalias %sum_out) {
  %v = load <128 x float>, ptr %in, align 4
  %max = call fast float @llvm.vector.reduce.fmax.v128f32(<128 x float> %v)
  %max0 = insertelement <128 x float> poison, float %max, i64 0
  %maxv = shufflevector <128 x float> %max0, <128 x float> poison,
      <128 x i32> zeroinitializer
  %centered = fsub fast <128 x float> %v, %maxv
  %c0 = insertelement <128 x float> poison, float 0x3FF7154760000000, i64 0
  %log2e = shufflevector <128 x float> %c0, <128 x float> poison,
      <128 x i32> zeroinitializer
  %scaled = fmul fast <128 x float> %centered, %log2e
  %expv = call fast <128 x float> @llvm.exp2.v128f32(<128 x float> %scaled)
  %sum = call fast float @llvm.vector.reduce.fadd.v128f32(
      float 0.0, <128 x float> %expv)
  store float %sum, ptr %sum_out, align 4
  %sum0 = insertelement <128 x float> poison, float %sum, i64 0
  %sumv = shufflevector <128 x float> %sum0, <128 x float> poison,
      <128 x i32> zeroinitializer
  %norm = fdiv fast <128 x float> %expv, %sumv
  store <128 x float> %norm, ptr %out, align 4
  ret void
}
