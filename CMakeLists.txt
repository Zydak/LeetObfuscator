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
    Linker
    Core
    TransformUtils
)

find_program(CLANGXX_EXE NAMES clang++ REQUIRED)

set(STRING_HELPER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/StringEncryptionHelper.cpp
)

set(ANTI_ANALYSIS_HELPER_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AntiAnalysisHelper.cpp
)

function(add_runtime_bitcode NAME SRC)
    set(X86_BC  ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.x86.bc)
    set(X64_BC  ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.x64.bc)
    set(X86_INC ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.x86.inc)
    set(X64_INC ${CMAKE_CURRENT_BINARY_DIR}/${NAME}.x64.inc)

    add_custom_command(
        OUTPUT ${X86_BC} ${X86_INC} ${X64_BC} ${X64_INC}

        COMMAND ${CLANGXX_EXE}
            --target=i386-pc-linux-gnu
            -std=c++17 -O3 -emit-llvm -c
            ${SRC}
            -o ${X86_BC}

        COMMAND ${CLANGXX_EXE}
            --target=x86_64-pc-linux-gnu
            -std=c++17 -O3 -emit-llvm -c
            ${SRC}
            -o ${X64_BC}

        COMMAND ${CMAKE_COMMAND} -E env
            bash -c
            "cd '${CMAKE_CURRENT_BINARY_DIR}' &&
             xxd -i '${NAME}.x86.bc' > '${NAME}.x86.inc' &&
             xxd -i '${NAME}.x64.bc' > '${NAME}.x64.inc'"

        DEPENDS ${SRC}
        COMMENT "Building ${NAME} runtime bitcode (x86/x64)"
        VERBATIM
    )

    add_custom_target(${NAME}_Bitcode
        DEPENDS ${X86_BC} ${X86_INC} ${X64_BC} ${X64_INC}
    )

    add_dependencies(LLVMLeetObfuscator ${NAME}_Bitcode)
endfunction()

add_runtime_bitcode(
    LeetRuntimeString
    ${STRING_HELPER_SRC}
)

add_runtime_bitcode(
    LeetRuntimeAntiAnalysis
    ${ANTI_ANALYSIS_HELPER_SRC}
)
