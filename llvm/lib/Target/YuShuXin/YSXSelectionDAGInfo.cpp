//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "YSXSelectionDAGInfo.h"
#include "YSXSubtarget.h"
#include "llvm/CodeGen/SelectionDAG.h"

#define GET_SDNODE_DESC
#include "YSXGenSDNodeInfo.inc"

using namespace llvm;

YSXSelectionDAGInfo::YSXSelectionDAGInfo()
    : SelectionDAGGenTargetInfo(YSXGenSDNodeInfo) {}

YSXSelectionDAGInfo::~YSXSelectionDAGInfo() = default;

void YSXSelectionDAGInfo::verifyTargetNode(const SelectionDAG &DAG,
                                             const SDNode *N) const {
  SelectionDAGGenTargetInfo::verifyTargetNode(DAG, N);
}

SDValue YSXSelectionDAGInfo::EmitTargetCodeForMemset(
    SelectionDAG &DAG, const SDLoc &dl, SDValue Chain, SDValue Dst, SDValue Src,
    SDValue Size, Align Alignment, bool isVolatile, bool AlwaysInline,
    MachinePointerInfo DstPtrInfo) const {
  return SDValue();
}
