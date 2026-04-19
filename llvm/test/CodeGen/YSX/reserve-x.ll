; RUN: llc -mtriple=ysx64 -mattr=+reserve-x5 -verify-machineinstrs < %s | FileCheck %s

define i64 @reserve_x5_smoke(i64 %x, i64 %y) {
; CHECK-LABEL: reserve_x5_smoke:
; CHECK:       add
; CHECK:       ret
  %sum = add i64 %x, %y
  ret i64 %sum
}
