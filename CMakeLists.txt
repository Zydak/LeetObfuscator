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

set(STRING_HELPER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/StringEncryptionHelper.cpp)
set(STRING_HELPER_BC  ${CMAKE_CURRENT_BINARY_DIR}/leet_string_encryption_template.bc)
set(STRING_HELPER_INC ${CMAKE_CURRENT_BINARY_DIR}/leet_string_encryption_template.inc)

add_custom_command(
    OUTPUT ${STRING_HELPER_BC} ${STRING_HELPER_INC}
    COMMAND ${CLANGXX_EXE} -std=c++17 -O3 -emit-llvm -c ${STRING_HELPER_SRC} -o ${STRING_HELPER_BC}
    COMMAND xxd -i leet_string_encryption_template.bc > ${STRING_HELPER_INC}
    DEPENDS ${STRING_HELPER_SRC}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Compiling string obfuscation runtime helper to embedded bitcode"
    VERBATIM
)
add_custom_target(LeetRuntimeStringBitcode DEPENDS ${STRING_HELPER_INC})
add_dependencies(LLVMLeetObfuscator LeetRuntimeStringBitcode)

set(ANTI_ANALYSIS_HELPER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AntiAnalysisHelper.cpp)
set(ANTI_ANALYSIS_HELPER_BC  ${CMAKE_CURRENT_BINARY_DIR}/leet_anti_analysis_template.bc)
set(ANTI_ANALYSIS_HELPER_INC ${CMAKE_CURRENT_BINARY_DIR}/leet_anti_analysis_template.inc)

add_custom_command(
    OUTPUT ${ANTI_ANALYSIS_HELPER_BC} ${ANTI_ANALYSIS_HELPER_INC}
    COMMAND ${CLANGXX_EXE} -std=c++17 -O3 -emit-llvm -c ${ANTI_ANALYSIS_HELPER_SRC} -o ${ANTI_ANALYSIS_HELPER_BC}
    COMMAND xxd -i leet_anti_analysis_template.bc > ${ANTI_ANALYSIS_HELPER_INC}
    DEPENDS ${ANTI_ANALYSIS_HELPER_SRC}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Compiling anti analysis obfuscation runtime helper to embedded bitcode"
    VERBATIM
)
add_custom_target(LeetRuntimeAntiAnalysisBitcode DEPENDS ${ANTI_ANALYSIS_HELPER_INC})
add_dependencies(LLVMLeetObfuscator LeetRuntimeAntiAnalysisBitcode)
