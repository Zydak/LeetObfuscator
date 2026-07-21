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

        set(HELPER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/StringObfuscateHelper.cpp)
        set(HELPER_BC  ${CMAKE_CURRENT_BINARY_DIR}/leet_string_obf_runtime.bc)
        set(HELPER_INC ${CMAKE_CURRENT_BINARY_DIR}/leet_string_obf_runtime_bc.inc)

        find_program(CLANGXX_EXE NAMES clang++ REQUIRED)

        # Compiling the bitcode for string obfuscation template
        add_custom_command(
            OUTPUT ${HELPER_BC} ${HELPER_INC}
            COMMAND ${CLANGXX_EXE} -std=c++17 -O2 -emit-llvm -c ${HELPER_SRC} -o ${HELPER_BC}
            COMMAND xxd -i leet_string_obf_runtime.bc > ${HELPER_INC}
            DEPENDS ${HELPER_SRC}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Compiling obfuscation runtime helper to embedded bitcode"
            VERBATIM
        )
        add_custom_target(LeetRuntimeBitcode DEPENDS ${HELPER_INC})

        add_library(LeetObfuscator MODULE ${LeetLLVMPassSources})
        add_dependencies(LeetObfuscator LeetRuntimeBitcode)

        target_include_directories(LeetObfuscator PRIVATE
            ${LLVM_INCLUDE_DIRS}
            ${CMAKE_CURRENT_BINARY_DIR}
        )
        target_compile_definitions(LeetObfuscator PRIVATE ${LLVM_DEFINITIONS})
        target_compile_options(LeetObfuscator PRIVATE -Wall -Wextra -Wpedantic)

        # Don't use the default lib prefix
        set_target_properties(LeetObfuscator PROPERTIES PREFIX "")
    else()
        message(WARNING "LLVM not found!")
    endif()
endif()