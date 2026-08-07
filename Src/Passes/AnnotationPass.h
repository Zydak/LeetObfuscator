#pragma once

#include "llvm/IR/PassManager.h"
#include "Logger.h"

namespace LeetObfuscator
{
    class AnnotationPass : public llvm::PassInfoMixin<AnnotationPass>
    {
    public:
        AnnotationPass() : m_Logger("AnnotationPass") {}
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);
    private:
        Logger m_Logger;
    };
}