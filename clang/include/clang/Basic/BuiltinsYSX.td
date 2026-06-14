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

include "clang/Basic/YSXGenAutoTinyVClangBuiltins.td"
