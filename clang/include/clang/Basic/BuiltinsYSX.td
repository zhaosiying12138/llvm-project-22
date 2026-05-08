//==- BuiltinsYSX.td - YuShuXin Builtin function database -*- tablegen -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "clang/Basic/BuiltinsBase.td"

class YSXBuiltin<string prototype, string features = ""> : TargetBuiltin {
  let Spellings = ["__builtin_ysx_" # NAME];
  let Prototype = prototype;
  let Features = features;
}

let Attributes = [NoThrow, Const] in {
def vadd_vv_i32m1
    : YSXBuiltin<"_ExtVector<4, int>(_ExtVector<4, int>, _ExtVector<4, int>, unsigned long int)",
                 "xtinyv,zvl128b">;
def vsub_vv_i32m1
    : YSXBuiltin<"_ExtVector<4, int>(_ExtVector<4, int>, _ExtVector<4, int>, unsigned long int)",
                 "xtinyv,zvl128b">;
def vmul_vv_i32m1
    : YSXBuiltin<"_ExtVector<4, int>(_ExtVector<4, int>, _ExtVector<4, int>, unsigned long int)",
                 "xtinyv,zvl128b">;
def vredsum_vs_i32m1
    : YSXBuiltin<"_ExtVector<4, int>(_ExtVector<4, int>, _ExtVector<4, int>, unsigned long int)",
                 "xtinyv,zvl128b">;
def vfexp_v_f32m1
    : YSXBuiltin<"_ExtVector<4, float>(_ExtVector<4, float>, unsigned long int)",
                 "xtinyv,zvl128b">;
def vfredsum_vs_f32m1
    : YSXBuiltin<"_ExtVector<4, float>(_ExtVector<4, float>, _ExtVector<4, float>, unsigned long int)",
                 "xtinyv,zvl128b">;
def vrgather_vv_i32m1
    : YSXBuiltin<"_ExtVector<4, int>(_ExtVector<4, int>, _ExtVector<4, int>, unsigned long int)",
                 "xtinyv,zvl128b">;
def vslideup_vx_i32m1
    : YSXBuiltin<"_ExtVector<4, int>(_ExtVector<4, int>, unsigned long int, unsigned long int)",
                 "xtinyv,zvl128b">;
} // Attributes = [Const, NoThrow]
