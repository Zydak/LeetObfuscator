#pragma once

#include "llvm/IR/PassManager.h"

namespace LeetObfuscator
{
    class AnnotationPass : public llvm::PassInfoMixin<AnnotationPass>
    {
    public:
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);
    private:
    };
}