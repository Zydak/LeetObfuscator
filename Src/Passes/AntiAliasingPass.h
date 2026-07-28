#pragma once

#include "llvm/IR/PassManager.h"

namespace LeetObfuscator
{
    class AntiAliasingPass : public llvm::PassInfoMixin<AntiAliasingPass>
    {
    public:
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);
    private:
        void ObfuscateFunction(llvm::Function& function);
    };
}