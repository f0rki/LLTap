# for building out of tree
add_library(LLTap MODULE LLTap.cpp)

# we'll need this line for building inside the llvm source tree
#add_llvm_loadable_module( LLTap
#    LLTap.cpp
#  )
