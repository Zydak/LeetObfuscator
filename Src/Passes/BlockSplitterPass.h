#pragma once

#include "llvm/IR/PassManager.h"
#include "SettingsParser.h"
#include "Logger.h"

namespace LeetObfuscator
{
    class BlockSplitterPass : public llvm::PassInfoMixin<BlockSplitterPass>
    {
    public:
        explicit BlockSplitterPass(SettingsParser::PassArguments arguments)
            : m_Arguments(std::move(arguments)), m_Logger("BlockSplitterPass") {}
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);
    private:
        void SplitFunction(llvm::Function& function);
        SettingsParser::PassArguments m_Arguments;
        Logger m_Logger;
    };
}
