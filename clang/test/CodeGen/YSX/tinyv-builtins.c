// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -emit-llvm -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -E -dM %s | FileCheck %s --check-prefix=YSX

#include <ysx_vector.h>

ysx_vint32m1_t test_vadd(ysx_vint32m1_t a, ysx_vint32m1_t b,
                         unsigned long vl) {
  return ysx_vadd_vv_i32m1(a, b, vl);
}

// IR-LABEL: define {{.*}}test_vadd
// IR: call <4 x i32> @llvm.ysx.vadd

ysx_vint32m1_t test_vsub(ysx_vint32m1_t a, ysx_vint32m1_t b,
                         unsigned long vl) {
  return ysx_vsub_vv_i32m1(a, b, vl);
}

// IR-LABEL: define {{.*}}test_vsub
// IR: call <4 x i32> @llvm.ysx.vsub

ysx_vint32m1_t test_vmul(ysx_vint32m1_t a, ysx_vint32m1_t b,
                         unsigned long vl) {
  return ysx_vmul_vv_i32m1(a, b, vl);
}

// IR-LABEL: define {{.*}}test_vmul
// IR: call <4 x i32> @llvm.ysx.vmul

ysx_vint32m1_t test_vredsum(ysx_vint32m1_t vector,
                            ysx_vint32m1_t scalar_seed, unsigned long vl) {
  return ysx_vredsum_vs_i32m1(vector, scalar_seed, vl);
}

// IR-LABEL: define {{.*}}test_vredsum
// IR: call <4 x i32> @llvm.ysx.vredsum

ysx_vfloat32m1_t test_vfredsum(ysx_vfloat32m1_t vector,
                               ysx_vfloat32m1_t scalar_seed,
                               unsigned long vl) {
  return ysx_vfredsum_vs_f32m1(vector, scalar_seed, vl);
}

// IR-LABEL: define {{.*}}test_vfredsum
// IR: call <4 x float> @llvm.ysx.vfredsum

ysx_vint32m1_t test_vrgather(ysx_vint32m1_t vector, ysx_vint32m1_t indices,
                             unsigned long vl) {
  return ysx_vrgather_vv_i32m1(vector, indices, vl);
}

// IR-LABEL: define {{.*}}test_vrgather
// IR: call <4 x i32> @llvm.ysx.vrgather

ysx_vint32m1_t test_vslideup(ysx_vint32m1_t vector, unsigned long offset,
                             unsigned long vl) {
  return ysx_vslideup_vx_i32m1(vector, offset, vl);
}

// IR-LABEL: define {{.*}}test_vslideup
// IR: call <4 x i32> @llvm.ysx.vslideup

// YSX-NOT: __riscv_vector
// YSX-NOT: __riscv_v_intrinsic
// YSX: #define __YSX_TINY_VECTOR__ 1
// YSX-NOT: __riscv_vector
// YSX-NOT: __riscv_v_intrinsic
// YSX: #define __riscv_xtinyv 1000000
// YSX-NOT: __riscv_vector
// YSX-NOT: __riscv_v_intrinsic
