add_llvm_component_library(LLVMLeetObfuscator
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AAMBAPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AnnotationPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AntiAliasingPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/AntiAnalysisPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/BlockSplitterPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/DispatcherPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/MBAPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/NanomitesPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/PermutationHelper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/SettingsParser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/StringEncryptionPass.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Src/Passes/StringObfuscateHelper.cpp

    ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Obfuscation
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Obfuscation/compat

    LINK_COMPONENTS
    Analysis
    Core
    TransformUtils
)