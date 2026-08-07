#pragma once

#include "llvm/IR/PassManager.h"
#include "SettingsParser.h"
#include "Logger.h"

#include "llvm/IR/BasicBlock.h"

namespace LeetObfuscator
{
    class DispatcherPass : public llvm::PassInfoMixin<DispatcherPass>
    {
    public:
        explicit DispatcherPass(SettingsParser::PassArguments arguments)
            : m_Arguments(std::move(arguments)), m_Logger("DispatcherPass") {}
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        void CreateDispatcherInAFunction(llvm::Function* function, llvm::ModuleAnalysisManager& mam);
        SettingsParser::PassArguments m_Arguments;
        Logger m_Logger;
    };
}
