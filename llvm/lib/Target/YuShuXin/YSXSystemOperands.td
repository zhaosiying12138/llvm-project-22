//===- YSXSystemOperands.td ------------------------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "llvm/TableGen/SearchableTable.td"
//
// YuShuXin v1 is restricted to rv64ima.  The profile intentionally exposes no
// symbolic CSR operands because Zicsr, privileged, debug, counter/timer, vector,
// and floating-point CSR surfaces are outside the supported ISA subset.
//
//===----------------------------------------------------------------------===//
