// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -emit-llvm -o - %s | FileCheck %s --check-prefix=IR

#include <ysx_vector.h>

ysx_vfloat32m1_t test_vfexp(ysx_vfloat32m1_t x, unsigned long vl) {
  return ysx_vfexp_v_f32m1(x, vl);
}

// IR-LABEL: define {{.*}}test_vfexp
// IR: call <4 x float> @llvm.ysx.vfexp
