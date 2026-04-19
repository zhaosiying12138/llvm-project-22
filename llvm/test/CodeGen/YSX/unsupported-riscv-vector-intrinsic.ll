; RUN: not llc -mtriple=ysx64-unknown-elf < %s 2>&1 | FileCheck %s

define i64 @vsetvli(i64 %a) {
; CHECK: YuShuXin only supports rv64ima and does not support RISC-V target intrinsics other than masked atomics
  %r = call i64 @llvm.riscv.vsetvli.i64(i64 %a, i64 1, i64 1)
  ret i64 %r
}
