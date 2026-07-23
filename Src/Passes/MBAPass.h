#pragma once

#include "llvm/Passes/PassBuilder.h"

namespace LeetObfuscator
{
    class MBAPass : public llvm::PassInfoMixin<MBAPass>
    {
    public:
        MBAPass(uint32_t passCount) : m_DefaultMaxPassCount(passCount) {}

        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        class InstructionHolder
        {
        public:
            llvm::Value* Add(llvm::Value* instruction);
            inline void Clear() { m_CreatedInstructions.clear(); }

            inline std::vector<llvm::Instruction*>& GetInstructions() { return m_CreatedInstructions; }
        private:
            std::vector<llvm::Instruction*> m_CreatedInstructions;
        };

        void ObfuscateFunction(llvm::Function& function);

        void ObfuscateInstruction(llvm::Instruction* instruction, uint32_t expansionCount);
        void ObfuscateBinaryInstruction(llvm::Instruction* instruction, uint32_t expansionCount, uint32_t iteration = 0);

        uint32_t m_DefaultMaxPassCount = 2;
    };
}