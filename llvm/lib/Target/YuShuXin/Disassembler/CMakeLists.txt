add_llvm_component_library(LLVMYSXDisassembler
  YSXDisassembler.cpp

  LINK_COMPONENTS
  MC
  MCDisassembler
  YSXDesc
  YSXInfo
  Support

  ADD_TO_COMPONENT
  YSX
  )
