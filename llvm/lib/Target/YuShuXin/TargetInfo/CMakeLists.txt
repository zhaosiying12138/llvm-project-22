add_llvm_component_library(LLVMYSXInfo
  YSXTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  YSX
  )
