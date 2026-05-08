//===- IntrinsicsYSX.td - Defines YuShuXin intrinsics ------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "ysx" in {
  def int_ysx_vadd
      : DefaultAttrsIntrinsic<[llvm_anyvector_ty],
                              [LLVMMatchType<0>, LLVMMatchType<0>,
                               llvm_anyint_ty],
                              [IntrNoMem, IntrSpeculatable]>;
}
