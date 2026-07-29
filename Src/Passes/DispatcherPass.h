#pragma once

#include "llvm/IR/PassManager.h"

#include "llvm/IR/BasicBlock.h"

namespace LeetObfuscator
{
    class DispatcherPass : public llvm::PassInfoMixin<DispatcherPass>
    {
    public:
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        void CreateDispatcherInAFunction(llvm::Function* function, llvm::ModuleAnalysisManager& mam);
    };
}