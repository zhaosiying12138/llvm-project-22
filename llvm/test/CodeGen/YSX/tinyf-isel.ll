; RUN: llc -mtriple=ysx64 -mattr=+xtinyf < %s | FileCheck %s

define void @tinyf_arith(ptr %pa, ptr %pb, ptr %pc) {
entry:
  %a = load float, ptr %pa, align 4
  %b = load float, ptr %pb, align 4
  %sum = fadd float %a, %b
  %diff = fsub float %sum, %a
  %prod = fmul float %diff, %b
  store float %prod, ptr %pc, align 4
  ret void
}

; CHECK-LABEL: tinyf_arith:
; CHECK: flw
; CHECK: flw
; CHECK: fadd.s
; CHECK: fsub.s
; CHECK: fmul.s
; CHECK: fsw
; CHECK-NOT: __addsf3
; CHECK-NOT: __subsf3
; CHECK-NOT: __mulsf3

define void @tinyf_convert(ptr %pf, ptr %psi, ptr %pui, ptr %psf, ptr %puf,
                           i32 %x, i32 %ux) {
entry:
  %f = load float, ptr %pf, align 4
  %si = fptosi float %f to i32
  %ui = fptoui float %f to i32
  store i32 %si, ptr %psi, align 4
  store i32 %ui, ptr %pui, align 4
  %sf = sitofp i32 %x to float
  %uf = uitofp i32 %ux to float
  store float %sf, ptr %psf, align 4
  store float %uf, ptr %puf, align 4
  ret void
}

; CHECK-LABEL: tinyf_convert:
; CHECK: flw
; CHECK-DAG: fcvt.w.s{{[ \t]}}
; CHECK-DAG: fcvt.wu.s{{[ \t]}}
; CHECK-DAG: fcvt.s.w{{[ \t]}}
; CHECK-DAG: fcvt.s.wu{{[ \t]}}
; CHECK: fsw
; CHECK-NOT: __fixsfdi
; CHECK-NOT: __fixunssfdi
; CHECK-NOT: __floatsisf
; CHECK-NOT: __floatunsisf

define i64 @tinyf_fptoui_zext(ptr %pf) {
entry:
  %f = load float, ptr %pf, align 4
  %ui = fptoui float %f to i32
  %z = zext i32 %ui to i64
  ret i64 %z
}

; CHECK-LABEL: tinyf_fptoui_zext:
; CHECK: fcvt.wu.s
; CHECK: slli
; CHECK: srli

define void @tinyf_compare(ptr %pa, ptr %pb, ptr %peq, ptr %plt, ptr %ple) {
entry:
  %a = load float, ptr %pa, align 4
  %b = load float, ptr %pb, align 4
  %eq = fcmp oeq float %a, %b
  %lt = fcmp olt float %a, %b
  %le = fcmp ole float %a, %b
  %eqi = zext i1 %eq to i32
  %lti = zext i1 %lt to i32
  %lei = zext i1 %le to i32
  store i32 %eqi, ptr %peq, align 4
  store i32 %lti, ptr %plt, align 4
  store i32 %lei, ptr %ple, align 4
  ret void
}

; CHECK-LABEL: tinyf_compare:
; CHECK: flw
; CHECK: flw
; CHECK: feq.s
; CHECK: flt.s
; CHECK: fle.s
