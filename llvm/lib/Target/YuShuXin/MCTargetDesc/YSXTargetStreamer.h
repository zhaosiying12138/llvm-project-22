//===-- YSXTargetStreamer.h - RISC-V Target Streamer ---------*- C++ -*--===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_YSX_MCTARGETDESC_YSXTARGETSTREAMER_H
#define LLVM_LIB_TARGET_YSX_MCTARGETDESC_YSXTARGETSTREAMER_H

#include "YSX.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"

namespace llvm {

class formatted_raw_ostream;

enum class YSXOptionArchArgType {
  Full,
  Plus,
  Minus,
};

struct YSXOptionArchArg {
  YSXOptionArchArgType Type;
  std::string Value;

  YSXOptionArchArg(YSXOptionArchArgType Type, std::string Value)
      : Type(Type), Value(Value) {}
};

class YSXTargetStreamer : public MCTargetStreamer {
  YSXABI::ABI TargetABI = YSXABI::ABI_Unknown;
  bool HasTSO = false;

public:
  YSXTargetStreamer(MCStreamer &S);
  void finish() override;
  virtual void reset();

  virtual void emitDirectiveOptionArch(ArrayRef<YSXOptionArchArg> Args);
  virtual void emitDirectiveOptionExact();
  virtual void emitDirectiveOptionNoExact();
  virtual void emitDirectiveOptionPIC();
  virtual void emitDirectiveOptionNoPIC();
  virtual void emitDirectiveOptionPop();
  virtual void emitDirectiveOptionPush();
  virtual void emitDirectiveOptionRelax();
  virtual void emitDirectiveOptionNoRelax();
  virtual void emitAttribute(unsigned Attribute, unsigned Value);
  virtual void finishAttributeSection();
  virtual void emitTextAttribute(unsigned Attribute, StringRef String);
  virtual void emitIntTextAttribute(unsigned Attribute, unsigned IntValue,
                                    StringRef StringValue);

  void emitTargetAttributes(const MCSubtargetInfo &STI, bool EmitStackAlign);
  void setTargetABI(YSXABI::ABI ABI);
  YSXABI::ABI getTargetABI() const { return TargetABI; }
  void setFlagsFromFeatures(const MCSubtargetInfo &STI);
  bool hasTSO() const { return HasTSO; }
};

// This part is for ascii assembly output
class YSXTargetAsmStreamer : public YSXTargetStreamer {
  formatted_raw_ostream &OS;

  void finishAttributeSection() override;
  void emitAttribute(unsigned Attribute, unsigned Value) override;
  void emitTextAttribute(unsigned Attribute, StringRef String) override;
  void emitIntTextAttribute(unsigned Attribute, unsigned IntValue,
                            StringRef StringValue) override;

public:
  YSXTargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS);

  void emitDirectiveOptionArch(ArrayRef<YSXOptionArchArg> Args) override;
  void emitDirectiveOptionExact() override;
  void emitDirectiveOptionNoExact() override;
  void emitDirectiveOptionPIC() override;
  void emitDirectiveOptionNoPIC() override;
  void emitDirectiveOptionPop() override;
  void emitDirectiveOptionPush() override;
  void emitDirectiveOptionRelax() override;
  void emitDirectiveOptionNoRelax() override;
};

}
#endif
