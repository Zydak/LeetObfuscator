#pragma once

#include "llvm/IR/PassManager.h"

namespace LeetObfuscator
{
    class DispatcherPass : public llvm::PassInfoMixin<DispatcherPass>
    {
    public:
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        bool DoesFunctionQualify(llvm::Function* function);
        void ReplaceTerminator(llvm::BasicBlock* block);
        void CreateDispatcherInAFunction(llvm::Function* function, llvm::ModuleAnalysisManager& mam);
    };
}