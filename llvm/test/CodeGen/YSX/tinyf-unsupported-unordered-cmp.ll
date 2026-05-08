; RUN: not llc -mtriple=ysx64 -mattr=+xtinyf < %s 2>&1 | FileCheck %s

; CHECK: LLVM ERROR: YSX tiny-F unsupported operation: f32 compare outside ordered eq/lt/le/gt/ge subset

define i32 @unordered_cmp(float %a, float %b) {
entry:
  %cmp = fcmp ugt float %a, %b
  %z = zext i1 %cmp to i32
  ret i32 %z
}
