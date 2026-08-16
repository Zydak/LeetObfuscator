message(STATUS "LeetObfuscator CMake file included!")

add_llvm_component_library(LLVMLeetObfuscator
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AAMBAPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AnnotationPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AntiAliasingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AntiAnalysisPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/BlockSplitterPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/DispatcherPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/MBAPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/Logger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/NanomitesPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/PermutationHelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/SettingsParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/StringEncryptionPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/VariableSplittingPass.cpp

    LINK_COMPONENTS
    Analysis
    Core
    TransformUtils
    CodeGen
)

find_program(CLANGXX_EXE NAMES clang++ REQUIRED)
set(HELPER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/StringEncryptionHelper.cpp)
set(HELPER_BC  ${CMAKE_CURRENT_BINARY_DIR}/leet_string_encryption_template.bc)
set(HELPER_INC ${CMAKE_CURRENT_BINARY_DIR}/leet_string_encryption_template.inc)

add_custom_command(
    OUTPUT ${HELPER_BC} ${HELPER_INC}
    COMMAND ${CLANGXX_EXE} -std=c++17 -O2 -emit-llvm -c ${HELPER_SRC} -o ${HELPER_BC}
    COMMAND xxd -i leet_string_encryption_template.bc > ${HELPER_INC}
    DEPENDS ${HELPER_SRC}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Compiling obfuscation runtime helper to embedded bitcode"
    VERBATIM
)
add_custom_target(LeetRuntimeBitcode DEPENDS ${HELPER_INC})
add_dependencies(LLVMLeetObfuscator LeetRuntimeBitcode)