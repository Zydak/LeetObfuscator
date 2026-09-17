#pragma once

#include "llvm/IR/PassManager.h"
#include "SettingsParser.h"
#include "Logger.h"

namespace LeetObfuscator
{
    class AntiAliasingPass : public llvm::PassInfoMixin<AntiAliasingPass>
    {
    public:
        explicit AntiAliasingPass(SettingsParser::PassArguments arguments)
            : m_Arguments(std::move(arguments)), m_Logger("AntiAliasingPass")
        {
        }

        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& moduleAnalysisManager);
    private:
        void ObfuscateFunction(llvm::Function& function);
        SettingsParser::PassArguments m_Arguments;
        Logger m_Logger;

        struct PassStatistics
        {
            uint32_t obfuscatedFunctions = 0;
            uint32_t replacedAllocas = 0;
            uint32_t patchedUses = 0;
            uint32_t freshCalculations = 0;
            uint32_t reusedCalculations = 0;
        };
        PassStatistics m_Statistics;
    };
}
