#pragma once

#include "llvm/Passes/PassBuilder.h"

namespace LeetObfuscator
{
    class AAMBAPass : public llvm::PassInfoMixin<AAMBAPass>
    {
    public:
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        void ObfuscateFunction(llvm::Function& function);
        void ObfuscateInstruction(llvm::Instruction* instruction);
    };
}