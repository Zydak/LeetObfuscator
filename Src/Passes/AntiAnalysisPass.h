#pragma once

#include "llvm/IR/PassManager.h"
#include "SettingsParser.h"
#include "llvm/IR/BasicBlock.h"

#include "llvm/IR/Dominators.h"

#include <memory>

namespace LeetObfuscator
{
    class AntiAnalysisPass : public llvm::PassInfoMixin<AntiAnalysisPass>
    {
    public:
            explicit AntiAnalysisPass(SettingsParser::PassArguments arguments) : m_Arguments(std::move(arguments)) {}
            llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

            static llvm::BasicBlock* CreateInvalidBogusBlock(llvm::Function* function, std::shared_ptr<RandomNumberGenerator> generator);
            static llvm::BasicBlock* ChainBogusIntoBlock(llvm::BasicBlock* block, llvm::BasicBlock* bogusBlock, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator);
            static llvm::BasicBlock* ChainBogusIntoBlockRdtsc(llvm::BasicBlock* block, llvm::BasicBlock* bogusBlock, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator);
        private:
            void ObfuscateFunction(llvm::Function& function);
            bool ObfuscateBlock(llvm::BasicBlock* block, bool randomPos = false, std::shared_ptr<RandomNumberGenerator> generator = nullptr, bool rdtsc = false);

            static llvm::Value* FindUsableInput(llvm::DominatorTree& DT, llvm::BasicBlock* block, llvm::BasicBlock::iterator insertIt);
            static int RankValue(llvm::Value* value);
            static bool IsSafeToTimeAcross(llvm::Instruction &I);
            SettingsParser::PassArguments m_Arguments;
    };
}
