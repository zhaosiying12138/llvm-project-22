; RUN: llc -mtriple=ysx64 -mattr=+m,+a < %s | FileCheck %s --check-prefix=ASM
; RUN: llc -filetype=obj -mtriple=ysx64 -mattr=+m,+a < %s -o %t.o
; RUN: llvm-readelf -A %t.o | FileCheck %s --check-prefix=ATTR
; RUN: llvm-objdump --triple=ysx64 -d --no-show-raw-insn %t.o | FileCheck %s --check-prefix=DIS

; ASM: .attribute 5, "rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0"

; ATTR: Attribute {
; ATTR: TagName: arch
; ATTR-NEXT: Value: rv64i2p1_m2p0_a2p1_zmmul1p0_zaamo1p0_zalrsc1p0

; DIS-LABEL: <add1>:
; DIS: addi

define i64 @add1(i64 %x) nounwind {
entry:
  %y = add i64 %x, 1
  ret i64 %y
}
