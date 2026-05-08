; RUN: llc -mtriple=ysx64 -mattr=+xtinyf < %s | FileCheck %s

define float @ret_load(ptr %p) {
entry:
  %x = load float, ptr %p, align 4
  ret float %x
}

; CHECK-LABEL: ret_load:
; CHECK: flw
; CHECK: fmv.x.w a0

define void @arg_store(float %x, ptr %p) {
entry:
  store float %x, ptr %p, align 4
  ret void
}

; CHECK-LABEL: arg_store:
; CHECK: fmv.w.x
; CHECK: fsw
