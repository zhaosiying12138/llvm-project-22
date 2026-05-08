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

void test_vsub_store(int *dst, const int *a, const int *b, unsigned long vl) {
  ysx_vint32m1_t va = *(const ysx_vint32m1_t *)a;
  ysx_vint32m1_t vb = *(const ysx_vint32m1_t *)b;
  *(ysx_vint32m1_t *)dst = ysx_vsub_vv_i32m1(va, vb, vl);
}

// ASM-LABEL: test_vsub_store:
// ASM: vsetvli {{[a-z0-9]+}}, a3, 208
// ASM: vsub.vv
// ASM: vse32.v

// OBJ-LABEL: <test_vsub_store>:
// OBJ: vsetvli {{[a-z0-9]+}}, a3, 208
// OBJ: vsub.vv
// OBJ: vse32.v

void test_vmul_store(int *dst, const int *a, const int *b, unsigned long vl) {
  ysx_vint32m1_t va = *(const ysx_vint32m1_t *)a;
  ysx_vint32m1_t vb = *(const ysx_vint32m1_t *)b;
  *(ysx_vint32m1_t *)dst = ysx_vmul_vv_i32m1(va, vb, vl);
}

// ASM-LABEL: test_vmul_store:
// ASM: vsetvli {{[a-z0-9]+}}, a3, 208
// ASM: vmul.vv
// ASM: vse32.v

// OBJ-LABEL: <test_vmul_store>:
// OBJ: vsetvli {{[a-z0-9]+}}, a3, 208
// OBJ: vmul.vv
// OBJ: vse32.v

void test_vredsum_store(int *dst, const int *vector, const int *scalar_seed,
                        unsigned long vl) {
  ysx_vint32m1_t vv = *(const ysx_vint32m1_t *)vector;
  ysx_vint32m1_t seed = *(const ysx_vint32m1_t *)scalar_seed;
  *(ysx_vint32m1_t *)dst = ysx_vredsum_vs_i32m1(vv, seed, vl);
}

// ASM-LABEL: test_vredsum_store:
// ASM: vsetvli {{[a-z0-9]+}}, a3, 208
// ASM: vredsum.vs
// ASM: vse32.v

// OBJ-LABEL: <test_vredsum_store>:
// OBJ: vsetvli {{[a-z0-9]+}}, a3, 208
// OBJ: vredsum.vs
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

void test_vfredsum_store(float *dst, const float *vector,
                         const float *scalar_seed, unsigned long vl) {
  ysx_vfloat32m1_t vv = *(const ysx_vfloat32m1_t *)vector;
  ysx_vfloat32m1_t seed = *(const ysx_vfloat32m1_t *)scalar_seed;
  *(ysx_vfloat32m1_t *)dst = ysx_vfredsum_vs_f32m1(vv, seed, vl);
}

// ASM-LABEL: test_vfredsum_store:
// ASM: vsetvli {{[a-z0-9]+}}, a3, 208
// ASM: vfredusum.vs
// ASM: vse32.v

// OBJ-LABEL: <test_vfredsum_store>:
// OBJ: vsetvli {{[a-z0-9]+}}, a3, 208
// OBJ: vfredusum.vs
// OBJ: vse32.v

void test_vrgather_store(int *dst, const int *vector, const int *indices,
                         unsigned long vl) {
  ysx_vint32m1_t vv = *(const ysx_vint32m1_t *)vector;
  ysx_vint32m1_t vi = *(const ysx_vint32m1_t *)indices;
  *(ysx_vint32m1_t *)dst = ysx_vrgather_vv_i32m1(vv, vi, vl);
}

// ASM-LABEL: test_vrgather_store:
// ASM: vsetvli {{[a-z0-9]+}}, a3, 208
// ASM: vrgather.vv
// ASM: vse32.v

// OBJ-LABEL: <test_vrgather_store>:
// OBJ: vsetvli {{[a-z0-9]+}}, a3, 208
// OBJ: vrgather.vv
// OBJ: vse32.v

void test_vslideup_store(int *dst, const int *vector, unsigned long offset,
                         unsigned long vl) {
  ysx_vint32m1_t vv = *(const ysx_vint32m1_t *)vector;
  *(ysx_vint32m1_t *)dst = ysx_vslideup_vx_i32m1(vv, offset, vl);
}

// ASM-LABEL: test_vslideup_store:
// ASM: vsetvli {{[a-z0-9]+}}, a3, 208
// ASM: vslideup.vx
// ASM: vse32.v

// OBJ-LABEL: <test_vslideup_store>:
// OBJ: vsetvli {{[a-z0-9]+}}, a3, 208
// OBJ: vslideup.vx
// OBJ: vse32.v
