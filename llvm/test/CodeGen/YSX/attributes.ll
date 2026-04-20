; RUN: llc -mtriple=ysx64 -mattr=+m,+a < %s | FileCheck %s --check-prefix=ASM
; RUN: llc -mtriple=ysx64 -mattr=+m,+a < %s -o %t.s
; RUN: llc -filetype=obj -mtriple=ysx64 -mattr=+m,+a < %s -o %t.o
; RUN: llvm-readelf -A %t.o | FileCheck %s --check-prefix=ATTR
; RUN: llvm-objdump --triple=ysx64 -d --no-show-raw-insn %t.o | FileCheck %s --check-prefix=DIS
; RUN: llvm-mc -triple=ysx64 -filetype=obj %t.s -o %t-roundtrip.o
; RUN: llvm-readelf -A %t-roundtrip.o | FileCheck %s --check-prefix=ATTR
; RUN: llvm-objdump --triple=ysx64 -d --no-show-raw-insn %t-roundtrip.o | FileCheck %s --check-prefix=DIS
; RUN: printf '.attribute 5, "rv64ima"\nadd a0, a0, a1\n' | llvm-mc -triple=ysx64 -filetype=obj - -o %t-unversioned.o
; RUN: llvm-readelf -A %t-unversioned.o | FileCheck %s --check-prefix=ATTR

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
