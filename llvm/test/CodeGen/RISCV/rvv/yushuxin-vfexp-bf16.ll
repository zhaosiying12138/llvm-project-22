; RUN: llc -mtriple=riscv64 -mattr=+v,+zvfbfmin,+experimental-yushuxin-vfexp,+zvl128b -riscv-v-vector-bits-min=128 -verify-machineinstrs < %s | FileCheck %s --check-prefix=ZVFBFMIN
; RUN: llc -mtriple=riscv64 -mattr=+v,+experimental-zvfbfa,+experimental-yushuxin-vfexp,+zvl128b -riscv-v-vector-bits-min=128 -verify-machineinstrs < %s | FileCheck %s --check-prefix=ZVFBFA

declare <vscale x 1 x bfloat> @llvm.exp.nxv1bf16(<vscale x 1 x bfloat>)
declare <4 x bfloat> @llvm.exp.v4bf16(<4 x bfloat>)

define <vscale x 1 x bfloat> @bf16_exp(<vscale x 1 x bfloat> %x) {
; ZVFBFMIN-LABEL: bf16_exp:
; ZVFBFMIN:       vsetvli {{.*}}, e16, mf4, ta, ma
; ZVFBFMIN:       vfwcvtbf16.f.f.v
; ZVFBFMIN:       vsetvli {{.*}}, e32, mf2, ta, ma
; ZVFBFMIN:       yushuxin.vfexp
; ZVFBFMIN:       vsetvli {{.*}}, e16, mf4, ta, ma
; ZVFBFMIN:       vfncvtbf16.f.f.w
; ZVFBFMIN:       ret
;
; ZVFBFA-LABEL: bf16_exp:
; ZVFBFA:       vsetvli {{.*}}, e16alt, mf4, ta, ma
; ZVFBFA:       vfwcvt.f.f.v
; ZVFBFA:       vsetvli {{.*}}, e32, mf2, ta, ma
; ZVFBFA:       yushuxin.vfexp
; ZVFBFA:       vsetvli {{.*}}, e16alt, mf4, ta, ma
; ZVFBFA:       vfncvt.f.f.w
; ZVFBFA:       ret
  %r = call <vscale x 1 x bfloat> @llvm.exp.nxv1bf16(<vscale x 1 x bfloat> %x)
  ret <vscale x 1 x bfloat> %r
}

define <4 x bfloat> @bf16_exp_fixed(<4 x bfloat> %x) {
; ZVFBFMIN-LABEL: bf16_exp_fixed:
; ZVFBFMIN:       vsetivli {{.*}}, 4, e16, mf2, ta, ma
; ZVFBFMIN:       vfwcvtbf16.f.f.v
; ZVFBFMIN:       vsetvli {{.*}}, e32, m1, ta, ma
; ZVFBFMIN:       yushuxin.vfexp
; ZVFBFMIN:       vsetvli {{.*}}, e16, mf2, ta, ma
; ZVFBFMIN:       vfncvtbf16.f.f.w
; ZVFBFMIN:       ret
;
; ZVFBFA-LABEL: bf16_exp_fixed:
; ZVFBFA:       vsetivli {{.*}}, 4, e16alt, mf2, ta, ma
; ZVFBFA:       vfwcvt.f.f.v
; ZVFBFA:       vsetvli {{.*}}, e32, m1, ta, ma
; ZVFBFA:       yushuxin.vfexp
; ZVFBFA:       vsetvli {{.*}}, e16alt, mf2, ta, ma
; ZVFBFA:       vfncvt.f.f.w
; ZVFBFA:       ret
  %r = call <4 x bfloat> @llvm.exp.v4bf16(<4 x bfloat> %x)
  ret <4 x bfloat> %r
}
