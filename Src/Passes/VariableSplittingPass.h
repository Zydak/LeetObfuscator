#pragma once

#include "llvm/IR/Instructions.h"
#include "llvm/Passes/PassBuilder.h"
#include "SettingsParser.h"
#include "Logger.h"

namespace LeetObfuscator
{
    class VariableSplittingPass : public llvm::PassInfoMixin<VariableSplittingPass>
    {
    public:
        explicit VariableSplittingPass(SettingsParser::PassArguments arguments)
            : m_Arguments(std::move(arguments)), m_Logger("StringEncryptionPass") {}
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        void ObfuscateFunction(llvm::Function* function);
        bool IsAllocaSplittable(llvm::AllocaInst* allocaInstruciton);
        void SplitAlloca(llvm::AllocaInst* allocaInstruction, llvm::IntegerType* intType, uint64_t byteSize);

        SettingsParser::PassArguments m_Arguments;
        Logger m_Logger;
    };
}
