; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat -stop-after=riscv-v-reg-pressure-remat < %s | FileCheck %s
; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-dag-sched -riscv-rvv-pressure-remat -debug-pass=Structure < %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=PIPELINE
; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-rvv-pressure-remat < %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=REMAT-ALONE
; RUN: not llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-v-reg-pressure-aware-sched < %s -o /dev/null 2>&1 | FileCheck %s --check-prefix=OLD-FLAG

; PIPELINE: RISC-V RVV register pressure rematerialization
; PIPELINE: Machine Instruction Scheduler
; PIPELINE: Greedy Register Allocator
; REMAT-ALONE: warning: -riscv-rvv-pressure-remat requires -riscv-rvv-pressure-dag-sched; ignoring remat
; OLD-FLAG: Unknown command line argument '-riscv-v-reg-pressure-aware-sched'

declare float @llvm.vector.reduce.fadd.v128f32(float, <128 x float>)
declare void @side_effect()

; CHECK-LABEL: name: remat_safe
; CHECK: PseudoVLE32_V_M4
; CHECK-NOT: PseudoVLE32_V_M4
define void @remat_safe(ptr noalias %in, ptr noalias %out, ptr noalias %sum) {
  %v = load <128 x float>, ptr %in, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  %x = fadd <128 x float> %v, %v
  store <128 x float> %x, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}

; CHECK-LABEL: name: remat_volatile_late_store
; CHECK: PseudoVLE32_V_M4
; CHECK-NOT: PseudoVLE32_V_M4
define void @remat_volatile_late_store(ptr noalias %in, ptr noalias %out,
                                        ptr noalias %sum) {
  %v = load <128 x float>, ptr %in, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  store volatile <128 x float> %v, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}

; CHECK-LABEL: name: remat_volatile
; CHECK: PseudoVLE32_V_M4
; CHECK-NOT: PseudoVLE32_V_M4
define void @remat_volatile(ptr noalias %in, ptr noalias %out,
                             ptr noalias %sum) {
  %v = load volatile <128 x float>, ptr %in, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  %x = fadd <128 x float> %v, %v
  store <128 x float> %x, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}

; CHECK-LABEL: name: remat_may_alias_store
; CHECK: PseudoVLE32_V_M4
; CHECK-NOT: PseudoVLE32_V_M4
define void @remat_may_alias_store(ptr %p, ptr %out, ptr %sum) {
  %v = load <128 x float>, ptr %p, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  store <128 x float> zeroinitializer, ptr %p, align 4
  %x = fadd <128 x float> %v, %v
  store <128 x float> %x, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}

; CHECK-LABEL: name: remat_call_intervened
; CHECK: PseudoVLE32_V_M4
; CHECK-NOT: PseudoVLE32_V_M4
define void @remat_call_intervened(ptr noalias %in, ptr noalias %out,
                                    ptr noalias %sum) {
  %v = load <128 x float>, ptr %in, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  call void @side_effect()
  %x = fadd <128 x float> %v, %v
  store <128 x float> %x, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}
