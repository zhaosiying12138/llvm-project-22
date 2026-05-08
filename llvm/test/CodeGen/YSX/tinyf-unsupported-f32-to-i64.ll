; RUN: not llc -mtriple=ysx64 -mattr=+xtinyf < %s 2>&1 | FileCheck %s

; CHECK: LLVM ERROR: YSX tiny-F unsupported operation: i64 result conversion from f32

define i64 @f32_to_i64(float %x) {
entry:
  %r = fptosi float %x to i64
  ret i64 %r
}
