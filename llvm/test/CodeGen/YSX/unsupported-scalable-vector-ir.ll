; RUN: not llc -mtriple=ysx64-unknown-elf < %s 2>&1 | FileCheck %s

define <vscale x 2 x i64> @svadd(<vscale x 2 x i64> %a,
                                  <vscale x 2 x i64> %b) {
; CHECK: YuShuXin only supports rv64ima and does not support scalable vector IR
  %r = add <vscale x 2 x i64> %a, %b
  ret <vscale x 2 x i64> %r
}
