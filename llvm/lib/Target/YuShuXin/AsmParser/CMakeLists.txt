add_llvm_component_library(LLVMYSXAsmParser
  YSXAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  YSXDesc
  YSXInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  YSX
  )
