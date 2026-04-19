; RUN: llc -mtriple=ysx64 -verify-machineinstrs < %s | FileCheck %s

declare void @vararg_i128(i32, ...)
declare i64 @take_i192(i192)

define i64 @use_i192(i192 %x) {
; CHECK-LABEL: use_i192:
; CHECK:       # %bb.0:
; CHECK-NEXT:    ld a0, 0(a0)
; CHECK-NEXT:    ret
  %lo = trunc i192 %x to i64
  ret i64 %lo
}

define i192 @ret_i192() {
; CHECK-LABEL: ret_i192:
; CHECK:       # %bb.0:
; CHECK-NEXT:    li a1, 1
; CHECK-NEXT:    sd a1, 0(a0)
; CHECK-NEXT:    sd zero, 8(a0)
; CHECK-NEXT:    sd zero, 16(a0)
; CHECK-NEXT:    ret
  ret i192 1
}

define i64 @call_i192() {
; CHECK-LABEL: call_i192:
; CHECK:       li a1, 1
; CHECK-NEXT:  mv a0, sp
; CHECK-NEXT:  sd a1, 0(sp)
; CHECK-NEXT:  sd zero, 8(sp)
; CHECK-NEXT:  sd zero, 16(sp)
; CHECK-NEXT:  call take_i192
  %r = call i64 @take_i192(i192 1)
  ret i64 %r
}

define void @call_vararg_i128() {
; CHECK-LABEL: call_vararg_i128:
; CHECK:       li a0, 1
; CHECK-NOT:   li a1,
; CHECK:       li a2, 1
; CHECK-NEXT:  li a3, 2
; CHECK-NEXT:  call vararg_i128
  call void (i32, ...) @vararg_i128(i32 1, i128 36893488147419103233)
  ret void
}
