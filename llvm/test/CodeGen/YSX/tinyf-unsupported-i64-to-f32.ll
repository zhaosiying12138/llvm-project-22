; RUN: not llc -mtriple=ysx64 -mattr=+xtinyf < %s 2>&1 | FileCheck %s

; CHECK: LLVM ERROR: YSX tiny-F unsupported operation: i64 signed conversion to f32

define float @i64_to_f32(i64 %x) {
entry:
  %r = sitofp i64 %x to float
  ret float %r
}
