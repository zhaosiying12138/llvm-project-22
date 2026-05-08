// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -emit-llvm -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -E -dM %s | FileCheck %s --check-prefix=YSX

#include <ysx_vector.h>

ysx_vint32m1_t test_vadd(ysx_vint32m1_t a, ysx_vint32m1_t b,
                         unsigned long vl) {
  return ysx_vadd_vv_i32m1(a, b, vl);
}

// IR-LABEL: define {{.*}}test_vadd
// IR: call <4 x i32> @llvm.ysx.vadd

// YSX-NOT: __riscv_vector
// YSX-NOT: __riscv_v_intrinsic
// YSX: #define __YSX_TINY_VECTOR__ 1
// YSX-NOT: __riscv_vector
// YSX-NOT: __riscv_v_intrinsic
// YSX: #define __riscv_xtinyv 1000000
// YSX-NOT: __riscv_vector
// YSX-NOT: __riscv_v_intrinsic
