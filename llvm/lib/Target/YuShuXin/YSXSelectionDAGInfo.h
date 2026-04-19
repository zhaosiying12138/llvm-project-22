//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_YSX_YSXSELECTIONDAGINFO_H
#define LLVM_LIB_TARGET_YSX_YSXSELECTIONDAGINFO_H

#include "llvm/CodeGen/SDNodeInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"

#define GET_SDNODE_ENUM
#include "YSXGenSDNodeInfo.inc"

namespace llvm {

namespace YSXISD {
// YSXISD Node TSFlags
enum : llvm::SDNodeTSFlags {
  HasPassthruOpMask = 1 << 0,
  HasMaskOpMask = 1 << 1,
};
} // namespace YSXISD

class YSXSelectionDAGInfo : public SelectionDAGGenTargetInfo {
public:
  YSXSelectionDAGInfo();

  ~YSXSelectionDAGInfo() override;

  void verifyTargetNode(const SelectionDAG &DAG,
                        const SDNode *N) const override;

  SDValue EmitTargetCodeForMemset(SelectionDAG &DAG, const SDLoc &dl,
                                  SDValue Chain, SDValue Dst, SDValue Src,
                                  SDValue Size, Align Alignment,
                                  bool isVolatile, bool AlwaysInline,
                                  MachinePointerInfo DstPtrInfo) const override;

  bool hasPassthruOp(unsigned Opcode) const {
    return GenNodeInfo.getDesc(Opcode).TSFlags & YSXISD::HasPassthruOpMask;
  }

  bool hasMaskOp(unsigned Opcode) const {
    return GenNodeInfo.getDesc(Opcode).TSFlags & YSXISD::HasMaskOpMask;
  }

  unsigned getMAccOpcode(unsigned MulOpcode) const {
    llvm_unreachable("YSX rv64ima does not define vector multiply-add nodes");
  }
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_YSX_YSXSELECTIONDAGINFO_H
