#pragma once

#include "llvm/Passes/PassBuilder.h"
#include "SettingsParser.h"
#include "Logger.h"

namespace LeetObfuscator
{
    class AAMBAPass : public llvm::PassInfoMixin<AAMBAPass>
    {
    public:
        explicit AAMBAPass(SettingsParser::PassArguments arguments)
            : m_Arguments(std::move(arguments)), m_Logger("AAMBAPass") {}
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        void ObfuscateFunction(llvm::Function& function);
        void ObfuscateInstruction(llvm::Instruction* instruction, std::shared_ptr<RandomNumberGenerator> generator);
        SettingsParser::PassArguments m_Arguments;
        Logger m_Logger;
    };
}
