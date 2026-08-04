// #pragma once

// #include "llvm/Passes/PassBuilder.h"
// #include "SettingsParser.h"

// namespace LeetObfuscator
// {
//     class StringEncryptionPass : public llvm::PassInfoMixin<StringEncryptionPass>
//     {
//     public:
//         explicit StringEncryptionPass(SettingsParser::PassArguments arguments) : m_Arguments(std::move(arguments)) {}
//         llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

//     private:
//         struct StringGlobalInfo
//         {
//             llvm::GlobalVariable* globalVar;
//             uint32_t key;
//         };

//         struct EmittedTemplate
//         {
//             llvm::Function* decryptFunction = nullptr;
//             llvm::Function* getKeyFunction = nullptr;
//             std::unique_ptr<llvm::Module> module = nullptr;
//         };

//         bool IsEncryptableStringGlobal(llvm::GlobalVariable* global);
//         void EncryptGlobalAndPatchAllUses(StringGlobalInfo& stringInfo, const EmittedTemplate& templates);
//         llvm::Function* GetDecryptFunction(llvm::Module& module, uint32_t key, const EmittedTemplate& templates);

//         EmittedTemplate GetTemplateFunctions(llvm::Module& module);
//         SettingsParser::PassArguments m_Arguments;
//     };
// }
