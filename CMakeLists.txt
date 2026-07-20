cmake_minimum_required(VERSION 3.20)

project(LeetObfuscator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LeetBuildLLVMPass "Build LLVM pass plugin" ON)
if (LeetBuildLLVMPass)
    find_package(LLVM CONFIG QUIET)

    if (LLVM_FOUND)
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")

        file(GLOB_RECURSE LeetLLVMPassSources Src/Passes/*.cpp)

        add_library(LeetObfuscator MODULE ${LeetLLVMPassSources})
        target_include_directories(LeetObfuscator PRIVATE ${LLVM_INCLUDE_DIRS})
        target_compile_definitions(LeetObfuscator PRIVATE ${LLVM_DEFINITIONS})
        target_compile_options(LeetObfuscator PRIVATE -Wall -Wextra -Wpedantic)

        # Don't use the default lib prefix
        set_target_properties(LeetObfuscator PROPERTIES PREFIX "")
    else()
        message(WARNING "LLVM not found!")
    endif()
endif()