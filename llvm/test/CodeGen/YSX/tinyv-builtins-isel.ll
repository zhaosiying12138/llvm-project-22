; RUN: llc -mtriple=ysx64 -mattr=+xtinyv,+zvl128b -stop-after=finalize-isel -o - %s | FileCheck %s --check-prefix=ISEL

declare <4 x i32> @llvm.ysx.vadd.v4i32.i64(<4 x i32>, <4 x i32>, i64)
declare <4 x i32> @llvm.ysx.vsub.v4i32.i64(<4 x i32>, <4 x i32>, i64)
declare <4 x i32> @llvm.ysx.vmul.v4i32.i64(<4 x i32>, <4 x i32>, i64)
declare <4 x i32> @llvm.ysx.vredsum.v4i32.i64(<4 x i32>, <4 x i32>, i64)
declare <4 x float> @llvm.ysx.vfexp.v4f32.i64(<4 x float>, i64)
declare <4 x float> @llvm.ysx.vfredsum.v4f32.i64(<4 x float>, <4 x float>, i64)
declare <4 x i32> @llvm.ysx.vrgather.v4i32.i64(<4 x i32>, <4 x i32>, i64)
declare <4 x i32> @llvm.ysx.vslideup.v4i32.i64.i64(<4 x i32>, i64, i64)

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

define void @vsub_store(ptr %dst, ptr %a, ptr %b, i64 %vl) {
; ISEL-LABEL: name: vsub_store
; ISEL: YSX_AUTO_VSETVLI %3, 208
; ISEL: YSX_AUTO_VSUB_VV
; ISEL: YSX_AUTO_VSE32_V
entry:
  %va = load <4 x i32>, ptr %a, align 16
  %vb = load <4 x i32>, ptr %b, align 16
  %r = call <4 x i32> @llvm.ysx.vsub.v4i32.i64(<4 x i32> %va, <4 x i32> %vb, i64 %vl)
  store <4 x i32> %r, ptr %dst, align 16
  ret void
}

define void @vmul_store(ptr %dst, ptr %a, ptr %b, i64 %vl) {
; ISEL-LABEL: name: vmul_store
; ISEL: YSX_AUTO_VSETVLI %3, 208
; ISEL: YSX_AUTO_VMUL_VV
; ISEL: YSX_AUTO_VSE32_V
entry:
  %va = load <4 x i32>, ptr %a, align 16
  %vb = load <4 x i32>, ptr %b, align 16
  %r = call <4 x i32> @llvm.ysx.vmul.v4i32.i64(<4 x i32> %va, <4 x i32> %vb, i64 %vl)
  store <4 x i32> %r, ptr %dst, align 16
  ret void
}

define void @vredsum_store(ptr %dst, ptr %vector, ptr %scalar_seed, i64 %vl) {
; ISEL-LABEL: name: vredsum_store
; ISEL: YSX_AUTO_VSETVLI %3, 208
; ISEL: YSX_AUTO_VREDSUM_VS
; ISEL: YSX_AUTO_VSE32_V
entry:
  %vv = load <4 x i32>, ptr %vector, align 16
  %seed = load <4 x i32>, ptr %scalar_seed, align 16
  %r = call <4 x i32> @llvm.ysx.vredsum.v4i32.i64(<4 x i32> %vv, <4 x i32> %seed, i64 %vl)
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

define void @vfredsum_store(ptr %dst, ptr %vector, ptr %scalar_seed, i64 %vl) {
; ISEL-LABEL: name: vfredsum_store
; ISEL: YSX_AUTO_VSETVLI %3, 208
; ISEL: YSX_AUTO_VFREDUSUM_VS
; ISEL: YSX_AUTO_VSE32_V
entry:
  %vv = load <4 x float>, ptr %vector, align 16
  %seed = load <4 x float>, ptr %scalar_seed, align 16
  %r = call <4 x float> @llvm.ysx.vfredsum.v4f32.i64(<4 x float> %vv, <4 x float> %seed, i64 %vl)
  store <4 x float> %r, ptr %dst, align 16
  ret void
}

define void @vrgather_store(ptr %dst, ptr %vector, ptr %indices, i64 %vl) {
; ISEL-LABEL: name: vrgather_store
; ISEL: YSX_AUTO_VSETVLI %3, 208
; ISEL: YSX_AUTO_VRGATHER_VV
; ISEL: YSX_AUTO_VSE32_V
entry:
  %vv = load <4 x i32>, ptr %vector, align 16
  %vi = load <4 x i32>, ptr %indices, align 16
  %r = call <4 x i32> @llvm.ysx.vrgather.v4i32.i64(<4 x i32> %vv, <4 x i32> %vi, i64 %vl)
  store <4 x i32> %r, ptr %dst, align 16
  ret void
}

define void @vslideup_store(ptr %dst, ptr %vector, i64 %offset, i64 %vl) {
; ISEL-LABEL: name: vslideup_store
; ISEL: YSX_AUTO_VSETVLI %3, 208
; ISEL: YSX_AUTO_VSLIDEUP_VX
; ISEL: YSX_AUTO_VSE32_V
entry:
  %vv = load <4 x i32>, ptr %vector, align 16
  %r = call <4 x i32> @llvm.ysx.vslideup.v4i32.i64.i64(<4 x i32> %vv, i64 %offset, i64 %vl)
  store <4 x i32> %r, ptr %dst, align 16
  ret void
}
