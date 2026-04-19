; RUN: not llc -mtriple=ysx64-unknown-elf < %s 2>&1 | FileCheck %s

define void @fp_clobber() {
; CHECK: YuShuXin only supports rv64ima and does not support removed inline asm clobber {f8}
  call void asm sideeffect "", "~{f8}"()
  ret void
}

define void @vector_csr_clobber(ptr %p) {
; CHECK: YuShuXin only supports rv64ima and does not support removed inline asm clobber {vtype}
  %r = call ptr asm sideeffect "ecall", "=&{x10},0,~{vtype},~{memory}"(ptr %p)
  ret void
}
