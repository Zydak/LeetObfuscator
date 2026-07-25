cmake_minimum_required(VERSION 3.20)

project(LeetObfuscator LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LeetBuildLLVMPass "Build LLVM pass plugin" ON)
if (LeetBuildLLVMPass)
    find_package(LLVM CONFIG QUIET)

    if (LLVM_FOUND)
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
        message(STATUS "LLVM build type: ${LLVM_BUILD_TYPE}")
        message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")

        file(GLOB_RECURSE LeetLLVMPassSources Src/Passes/*.cpp)

        set(HELPER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/StringObfuscateHelper.cpp)
        set(HELPER_BC  ${CMAKE_CURRENT_BINARY_DIR}/leet_string_obf_runtime.bc)
        set(HELPER_INC ${CMAKE_CURRENT_BINARY_DIR}/leet_string_obf_runtime_bc.inc)

        find_program(
            CLANGXX_EXE
            NAMES clang++
            HINTS "${LLVM_TOOLS_BINARY_DIR}"
            REQUIRED
        )

        message(STATUS "clang++ = ${CLANGXX_EXE}")

        # Compiling the bitcode for string obfuscation template
        set(HELPER_INCLUDE_FLAGS "")
        foreach(dir ${LLVM_INCLUDE_DIRS})
            list(APPEND HELPER_INCLUDE_FLAGS "-I${dir}")
        endforeach()
        add_custom_command(
            OUTPUT ${HELPER_BC} ${HELPER_INC}
            COMMAND ${CLANGXX_EXE} -std=c++17 -O2 -emit-llvm -c ${HELPER_SRC}
                    -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
                    ${HELPER_INCLUDE_FLAGS}
                    -o ${HELPER_BC}
            COMMAND ${CMAKE_COMMAND} -DIN_FILE=${HELPER_BC} -DOUT_FILE=${HELPER_INC}
                    -DVAR_NAME=leet_string_obf_runtime_bc
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/Cmake/bin2c.cmake
            DEPENDS ${HELPER_SRC}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Compiling obfuscation runtime helper to embedded bitcode"
            VERBATIM
        )
        add_custom_target(LeetRuntimeBitcode DEPENDS ${HELPER_INC})

        add_library(LeetObfuscator MODULE ${LeetLLVMPassSources})
        add_dependencies(LeetObfuscator LeetRuntimeBitcode)

        llvm_map_components_to_libnames(LeetLLVMLibs
            core
            support
            irreader
            bitreader
            linker
            transformutils
            analysis
            passes
        )

        target_link_libraries(LeetObfuscator PRIVATE ${LeetLLVMLibs})

        target_include_directories(LeetObfuscator SYSTEM PRIVATE
            ${LLVM_INCLUDE_DIRS}
        )
        target_include_directories(LeetObfuscator PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}   # keep your own generated headers as non-system
        )

        separate_arguments(LeetLLVMDefinitions NATIVE_COMMAND "${LLVM_DEFINITIONS}")
        target_compile_options(LeetObfuscator PRIVATE ${LeetLLVMDefinitions})

        if (MSVC)
            target_compile_options(LeetObfuscator PRIVATE /W4)
        else()
            target_compile_options(LeetObfuscator PRIVATE
                -Wall
                -Wextra
                -Wpedantic
            )
        endif()

        # What's windows even for?
        if (WIN32)
            target_link_options(LeetObfuscator PRIVATE "/EXPORT:llvmGetPassPluginInfo")
        endif()

        # Don't use the default lib prefix
        set_target_properties(LeetObfuscator PROPERTIES PREFIX "")
    else()
        message(WARNING "LLVM not found!")
    endif()
endif()