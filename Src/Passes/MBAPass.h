#pragma once

#include "llvm/Passes/PassBuilder.h"

namespace LeetObfuscator
{
    class MBAPass : public llvm::PassInfoMixin<MBAPass>
    {
    public:
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

        void ObfuscateModule(llvm::Module& module, InstructionHolder& createdInstructions);

        void ObfuscateInstruction(llvm::Instruction* instruction, InstructionHolder& createdInstructions);
        llvm::Value* GetObfuscatedBinaryInstruction(llvm::Instruction* instruction, InstructionHolder& createdInstructions);

        uint32_t m_MaxPassCount = 2;
    };
}