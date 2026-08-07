#pragma once

#include "llvm/Passes/PassBuilder.h"
#include "SettingsParser.h"
#include "Logger.h"

namespace LeetObfuscator
{
    class MBAPass : public llvm::PassInfoMixin<MBAPass>
    {
    public:
        explicit MBAPass(SettingsParser::PassArguments arguments)
            : m_Arguments(std::move(arguments)), m_Logger("MBAPass") {}

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
        void ObfuscateBinaryOperation(llvm::Instruction* instruction, uint32_t expansionCount, uint32_t iteration = 0);
        void ObfuscateCompareOperation(llvm::Instruction* instruction, uint32_t expansionCount, uint32_t iteration = 0);

        SettingsParser::PassArguments m_Arguments;
        Logger m_Logger;
    };
}
