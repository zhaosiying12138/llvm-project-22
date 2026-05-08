// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -O2 -S -o - %s | FileCheck %s --check-prefix=ASM
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -S -o /dev/null %s
// RUN: %clang_cc1 -triple ysx64-unknown-elf -target-feature +xtinyv -target-feature +zvl128b -I%S/../../../lib/Headers -O2 -emit-obj -o %t.o %s
// RUN: llvm-objdump --triple=ysx64 --mattr=+xtinyv --no-print-imm-hex -d %t.o | FileCheck %s --check-prefix=OBJ

#include <ysx_vector.h>

void test_vadd_store(int *dst, const int *a, const int *b, unsigned long vl) {
  ysx_vint32m1_t va = *(const ysx_vint32m1_t *)a;
  ysx_vint32m1_t vb = *(const ysx_vint32m1_t *)b;
  *(ysx_vint32m1_t *)dst = ysx_vadd_vv_i32m1(va, vb, vl);
}

// ASM-LABEL: test_vadd_store:
// ASM: vsetivli {{[a-z0-9]+}}, 4, 208
// ASM: vle32.v
// ASM: vsetivli {{[a-z0-9]+}}, 4, 208
// ASM: vle32.v
// ASM: vsetvli {{[a-z0-9]+}}, a3, 208
// ASM: vadd.vv
// ASM: vsetivli {{[a-z0-9]+}}, 4, 208
// ASM: vse32.v

// OBJ-LABEL: <test_vadd_store>:
// OBJ: vsetivli {{[a-z0-9]+}}, 4, 208
// OBJ: vle32.v
// OBJ: vsetivli {{[a-z0-9]+}}, 4, 208
// OBJ: vle32.v
// OBJ: vsetvli {{[a-z0-9]+}}, a3, 208
// OBJ: vadd.vv
// OBJ: vsetivli {{[a-z0-9]+}}, 4, 208
// OBJ: vse32.v

void test_vfexp_store(float *dst, const float *x, unsigned long vl) {
  ysx_vfloat32m1_t vx = *(const ysx_vfloat32m1_t *)x;
  *(ysx_vfloat32m1_t *)dst = ysx_vfexp_v_f32m1(vx, vl);
}

// ASM-LABEL: test_vfexp_store:
// ASM: vsetivli {{[a-z0-9]+}}, 4, 208
// ASM: vle32.v
// ASM: vsetvli {{[a-z0-9]+}}, a2, 208
// ASM: yushuxin.vfexp
// ASM: vsetivli {{[a-z0-9]+}}, 4, 208
// ASM: vse32.v

// OBJ-LABEL: <test_vfexp_store>:
// OBJ: vsetivli {{[a-z0-9]+}}, 4, 208
// OBJ: vle32.v
// OBJ: vsetvli {{[a-z0-9]+}}, a2, 208
// OBJ: yushuxin.vfexp
// OBJ: vsetivli {{[a-z0-9]+}}, 4, 208
// OBJ: vse32.v
