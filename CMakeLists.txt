cmake_minimum_required(VERSION 3.0)
project(LLTap)

set(CMAKE_C_FLAGS "-Wall -pedantic -std=gnu99")
set(CMAKE_CXX_FLAGS "-Wall -pedantic -std=c++11 -fno-rtti")

# change this to use another llvm build location
#set(LLVM_DIR "~/src/llvm/build/share/llvm/cmake/")
find_package(LLVM REQUIRED CONFIG)
#
#list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
#include(AddLLVM)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_definitions(${LLVM_DEFINITIONS})

add_subdirectory(llvmpass)
add_subdirectory(lib)
