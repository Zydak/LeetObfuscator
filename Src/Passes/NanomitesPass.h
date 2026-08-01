#pragma once
#include "llvm/Passes/PassBuilder.h"
#include "SettingsParser.h"
#include <vector>

namespace LeetObfuscator
{
    class NanomitesPass : public llvm::PassInfoMixin<NanomitesPass>
    {
    public:
        explicit NanomitesPass(SettingsParser::PassArguments arguments) : m_Arguments(std::move(arguments)) {}

        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        void ObfuscateFunction(llvm::Function* function, std::vector<llvm::Constant*>& nanomitesEntries);
        void CreateGlobalNanomitesTable(llvm::Module& module, std::vector<llvm::Constant*>& nanomitesEntries);

        uint32_t GenerateUniqueNanomiteId(RandomNumberGenerator& generator);
        std::string MakeIdTrailer(uint32_t nanomiteId);
        bool CanObfuscateCallSignature(llvm::CallInst* callInst);
        llvm::Function* CreateTrampoline(llvm::Module& module, llvm::Function* realFunc);

        SettingsParser::PassArguments m_Arguments;
    };
}