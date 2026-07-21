#pragma once

#include "llvm/Passes/PassBuilder.h"

namespace LeetObfuscator
{
    class StringEncryptionPass : public llvm::PassInfoMixin<StringEncryptionPass>
    {
    public:
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        struct StringGlobalInfo
        {
            llvm::GlobalVariable* globalVar;
            uint32_t key;
        };

        struct EmittedTemplate
        {
            llvm::Function* decryptFunction = nullptr;
            llvm::Function* getKeyFunction = nullptr;
            std::unique_ptr<llvm::Module> module = nullptr;
        };

        bool IsEncryptableStringGlobal(llvm::GlobalVariable* global);
        void EncryptGlobalAndPatchAllUses(StringGlobalInfo& stringInfo, const EmittedTemplate& templates);
        llvm::Function* GetDecryptFunction(llvm::Module& module, uint32_t key, const EmittedTemplate& templates);

        EmittedTemplate GetTemplateFunctions(llvm::Module& module);
    };
}