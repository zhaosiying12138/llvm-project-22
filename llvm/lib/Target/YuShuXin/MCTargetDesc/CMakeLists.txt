add_llvm_component_library(LLVMYSXDesc
  YSXAsmBackend.cpp
  YSXBaseInfo.cpp
  YSXELFObjectWriter.cpp
  YSXInstPrinter.cpp
  YSXMCAsmInfo.cpp
  YSXMCCodeEmitter.cpp
  YSXMCExpr.cpp
  YSXMCObjectFileInfo.cpp
  YSXMCTargetDesc.cpp
  YSXMachObjectWriter.cpp
  YSXMatInt.cpp
  YSXTargetStreamer.cpp
  YSXELFStreamer.cpp

  LINK_COMPONENTS
  BinaryFormat
  MC
  YSXInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  YSX
)
