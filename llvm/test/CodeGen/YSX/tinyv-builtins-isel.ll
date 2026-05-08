; RUN: llc -mtriple=ysx64 -mattr=+xtinyv,+zvl128b -stop-after=finalize-isel -o - %s | FileCheck %s --check-prefix=ISEL

declare <4 x i32> @llvm.ysx.vadd.v4i32.i64(<4 x i32>, <4 x i32>, i64)
declare <4 x float> @llvm.ysx.vfexp.v4f32.i64(<4 x float>, i64)

define void @vadd_store(ptr %dst, ptr %a, ptr %b, i64 %vl) {
; ISEL-LABEL: name: vadd_store
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VLE32_V
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VLE32_V
; ISEL: YSX_AUTO_VSETVLI %3, 208
; ISEL: YSX_AUTO_VADD_VV
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VSE32_V
entry:
  %va = load <4 x i32>, ptr %a, align 16
  %vb = load <4 x i32>, ptr %b, align 16
  %r = call <4 x i32> @llvm.ysx.vadd.v4i32.i64(<4 x i32> %va, <4 x i32> %vb, i64 %vl)
  store <4 x i32> %r, ptr %dst, align 16
  ret void
}

define void @vfexp_store(ptr %dst, ptr %x, i64 %vl) {
; ISEL-LABEL: name: vfexp_store
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VLE32_V
; ISEL: YSX_AUTO_VSETVLI %2, 208
; ISEL: YSX_AUTO_YUSHUXIN_VFEXP
; ISEL: YSX_AUTO_VSETIVLI 4, 208
; ISEL: YSX_AUTO_VSE32_V
entry:
  %vx = load <4 x float>, ptr %x, align 16
  %r = call <4 x float> @llvm.ysx.vfexp.v4f32.i64(<4 x float> %vx, i64 %vl)
  store <4 x float> %r, ptr %dst, align 16
  ret void
}
