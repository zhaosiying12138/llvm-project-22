add_llvm_component_library(LLVMYSXTargetMCA
  YSXCustomBehaviour.cpp

  LINK_COMPONENTS
  CodeGenTypes
  MC
  MCA
  MCParser
  YSXDesc
  YSXInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  YSX
  )
