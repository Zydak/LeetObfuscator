#pragma once

#include "llvm/IR/PassManager.h"
#include "SettingsParser.h"

namespace LeetObfuscator
{
    class AntiAliasingPass : public llvm::PassInfoMixin<AntiAliasingPass>
    {
    public:
        explicit AntiAliasingPass(SettingsParser::PassArguments arguments) : m_Arguments(std::move(arguments)) {}
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);
    private:
        void ObfuscateFunction(llvm::Function& function);
        SettingsParser::PassArguments m_Arguments;
    };
}
