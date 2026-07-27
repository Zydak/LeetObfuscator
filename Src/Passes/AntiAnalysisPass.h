#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/IR/BasicBlock.h"

namespace LeetObfuscator
{
    class AntiAnalysisPass : public llvm::PassInfoMixin<AntiAnalysisPass>
    {
    public:
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);
    private:
        void ObfuscateFunction(llvm::Function& function);
        bool ObfuscateBlock(llvm::BasicBlock* block, bool randomPos = false);
    };
}