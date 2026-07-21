#pragma once

#include "llvm/IR/PassManager.h"

namespace LeetObfuscator
{
    class BlockSplitterPass : public llvm::PassInfoMixin<BlockSplitterPass>
    {
    public:
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);
    private:
    };
}