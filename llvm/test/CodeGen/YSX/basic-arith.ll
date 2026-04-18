; RUN: llc -mtriple=ysx64 %s -o - | FileCheck %s

define i64 @add(i64 %a, i64 %b) {
; CHECK-LABEL: add:
; CHECK: add a0, a0, a1
; CHECK: ret
entry:
  %c = add i64 %a, %b
  ret i64 %c
}

define i64 @mul(i64 %a, i64 %b) {
; CHECK-LABEL: mul:
; CHECK: mul a0, a0, a1
; CHECK: ret
entry:
  %c = mul i64 %a, %b
  ret i64 %c
}
