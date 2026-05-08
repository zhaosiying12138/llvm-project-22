/*===---- ysx_vector.h - YuShuXin tiny vector intrinsics ------------------===
 *
 * Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://llvm.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 *
 *===-----------------------------------------------------------------------===
 */

#ifndef __YSX_VECTOR_H
#define __YSX_VECTOR_H

#ifndef __YSX_TINY_VECTOR__
#error "YSX tiny vector intrinsics require xtinyv and zvl128b"
#endif

typedef int ysx_vint32m1_t __attribute__((ext_vector_type(4)));
typedef float ysx_vfloat32m1_t __attribute__((ext_vector_type(4)));

static __inline__ ysx_vint32m1_t __attribute__((__always_inline__, __nodebug__))
ysx_vadd_vv_i32m1(ysx_vint32m1_t __a, ysx_vint32m1_t __b,
                  unsigned long __vl) {
  return __builtin_ysx_vadd_vv_i32m1(__a, __b, __vl);
}

static __inline__ ysx_vint32m1_t __attribute__((__always_inline__, __nodebug__))
ysx_vsub_vv_i32m1(ysx_vint32m1_t __a, ysx_vint32m1_t __b,
                  unsigned long __vl) {
  return __builtin_ysx_vsub_vv_i32m1(__a, __b, __vl);
}

static __inline__ ysx_vint32m1_t __attribute__((__always_inline__, __nodebug__))
ysx_vmul_vv_i32m1(ysx_vint32m1_t __a, ysx_vint32m1_t __b,
                  unsigned long __vl) {
  return __builtin_ysx_vmul_vv_i32m1(__a, __b, __vl);
}

static __inline__ ysx_vint32m1_t __attribute__((__always_inline__, __nodebug__))
ysx_vredsum_vs_i32m1(ysx_vint32m1_t __vector, ysx_vint32m1_t __scalar_seed,
                     unsigned long __vl) {
  return __builtin_ysx_vredsum_vs_i32m1(__vector, __scalar_seed, __vl);
}

static __inline__ ysx_vfloat32m1_t __attribute__((__always_inline__,
                                                  __nodebug__))
ysx_vfexp_v_f32m1(ysx_vfloat32m1_t __x, unsigned long __vl) {
  return __builtin_ysx_vfexp_v_f32m1(__x, __vl);
}

static __inline__ ysx_vfloat32m1_t __attribute__((__always_inline__,
                                                  __nodebug__))
ysx_vfredsum_vs_f32m1(ysx_vfloat32m1_t __vector,
                      ysx_vfloat32m1_t __scalar_seed, unsigned long __vl) {
  return __builtin_ysx_vfredsum_vs_f32m1(__vector, __scalar_seed, __vl);
}

static __inline__ ysx_vint32m1_t __attribute__((__always_inline__, __nodebug__))
ysx_vrgather_vv_i32m1(ysx_vint32m1_t __vector, ysx_vint32m1_t __indices,
                      unsigned long __vl) {
  return __builtin_ysx_vrgather_vv_i32m1(__vector, __indices, __vl);
}

static __inline__ ysx_vint32m1_t __attribute__((__always_inline__, __nodebug__))
ysx_vslideup_vx_i32m1(ysx_vint32m1_t __vector, unsigned long __offset,
                      unsigned long __vl) {
  return __builtin_ysx_vslideup_vx_i32m1(__vector, __offset, __vl);
}

#endif /* __YSX_VECTOR_H */
