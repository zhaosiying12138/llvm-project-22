; RUN: llc -O2 -mtriple=riscv64 -mattr=+v,+zvl1024b -riscv-v-vector-bits-min=1024 -riscv-v-reg-pressure-aware-sched -stop-after=riscv-v-reg-pressure-reload < %s | FileCheck %s

declare float @llvm.vector.reduce.fadd.v128f32(float, <128 x float>)
declare void @side_effect()

; CHECK-LABEL: name: reload_safe
; CHECK-COUNT-2: PseudoVLE32_V_M4
define void @reload_safe(ptr noalias %in, ptr noalias %out, ptr noalias %sum) {
  %v = load <128 x float>, ptr %in, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  %x = fadd <128 x float> %v, %v
  store <128 x float> %x, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}

; CHECK-LABEL: name: reload_volatile
; CHECK: PseudoVLE32_V_M4
; CHECK-NOT: PseudoVLE32_V_M4
define void @reload_volatile(ptr noalias %in, ptr noalias %out,
                             ptr noalias %sum) {
  %v = load volatile <128 x float>, ptr %in, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  %x = fadd <128 x float> %v, %v
  store <128 x float> %x, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}

; CHECK-LABEL: name: reload_may_alias_store
; CHECK: PseudoVLE32_V_M4
; CHECK-NOT: PseudoVLE32_V_M4
define void @reload_may_alias_store(ptr %p, ptr %out, ptr %sum) {
  %v = load <128 x float>, ptr %p, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  store <128 x float> zeroinitializer, ptr %p, align 4
  %x = fadd <128 x float> %v, %v
  store <128 x float> %x, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}

; CHECK-LABEL: name: reload_call_intervened
; CHECK: PseudoVLE32_V_M4
; CHECK-NOT: PseudoVLE32_V_M4
define void @reload_call_intervened(ptr noalias %in, ptr noalias %out,
                                    ptr noalias %sum) {
  %v = load <128 x float>, ptr %in, align 4
  %r = call float @llvm.vector.reduce.fadd.v128f32(float 0.0, <128 x float> %v)
  call void @side_effect()
  %x = fadd <128 x float> %v, %v
  store <128 x float> %x, ptr %out, align 4
  store float %r, ptr %sum, align 4
  ret void
}
