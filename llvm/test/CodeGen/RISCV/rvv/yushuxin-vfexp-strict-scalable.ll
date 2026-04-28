; RUN: not llc -mtriple=riscv64 -mattr=+v,+experimental-yushuxin-vfexp,+zvl1024b -riscv-v-vector-bits-min=1024 -verify-machineinstrs < %s 2>&1 | FileCheck %s

; CHECK: error: strict scalable-vector exp is not supported by RISC-V
; CHECK-NOT: yushuxin.vfexp

declare i64 @llvm.riscv.vsetvli.i64(i64, i64, i64)
declare <vscale x 4 x float> @llvm.riscv.vle.nxv4f32.i64(<vscale x 4 x float>, ptr, i64)
declare void @llvm.riscv.vse.nxv4f32.i64(<vscale x 4 x float>, ptr, i64)
declare <vscale x 4 x float> @llvm.experimental.constrained.exp.nxv4f32(<vscale x 4 x float>, metadata, metadata)

define void @strict_exp_nxv4f32(ptr noalias %in, ptr noalias %out,
                                i64 %n) strictfp {
  %vl = call i64 @llvm.riscv.vsetvli.i64(i64 %n, i64 2, i64 0)
  %v = call <vscale x 4 x float> @llvm.riscv.vle.nxv4f32.i64(
      <vscale x 4 x float> poison, ptr %in, i64 %vl)
  %e = call <vscale x 4 x float> @llvm.experimental.constrained.exp.nxv4f32(
      <vscale x 4 x float> %v,
      metadata !"round.dynamic",
      metadata !"fpexcept.strict") strictfp
  call void @llvm.riscv.vse.nxv4f32.i64(<vscale x 4 x float> %e,
                                        ptr %out, i64 %vl)
  ret void
}
