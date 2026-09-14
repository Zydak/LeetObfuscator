#pragma once

#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "SettingsParser.h"
#include "Logger.h"
#include "llvm/IR/BasicBlock.h"

#include "llvm/IR/Dominators.h"

#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/ADT/SmallVector.h"

#include <memory>

namespace LeetObfuscator
{
    class AntiAnalysisPass : public llvm::PassInfoMixin<AntiAnalysisPass>
    {
    public:
            explicit AntiAnalysisPass(SettingsParser::PassArguments arguments)
                : m_Arguments(std::move(arguments)), m_Logger("AntiAnalysisPass") {}
            llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

            enum class AntiDebugType
            {
                pid,
                blacklist
            };

            struct EmittedTemplate
            {
                llvm::Function* pidFunction = nullptr;
                llvm::Function* blacklistFunction = nullptr;
                std::unique_ptr<llvm::Module> module = nullptr;
            };

            struct DominatingPair
            {
                llvm::Value* first = nullptr;
                llvm::Value* second = nullptr;
            };

            static llvm::GlobalVariable* GetOrEmitPoisonRingGlobal(llvm::Module& module, std::shared_ptr<RandomNumberGenerator> generator);
            static void EmitRingStatePoisoning(llvm::IRBuilder<>& builder, llvm::Module& module, llvm::Value* detectedCondition, std::shared_ptr<RandomNumberGenerator> generator);
            static bool EmitLocalStatePoisoning(llvm::IRBuilder<>& builder, llvm::BasicBlock* block, llvm::BasicBlock* newSplitBlock, llvm::Value* detectedCondition, DominatingPair inputPair, std::shared_ptr<RandomNumberGenerator> generator);
            static llvm::Value* GenerateNonLinear2DMbaPredicate(llvm::IRBuilder<>& builder, llvm::Value* x, llvm::Value* y, std::shared_ptr<RandomNumberGenerator> generator);
            static DominatingPair FindUsableInputPair(llvm::BasicBlock* block, llvm::BasicBlock::iterator insertIt, std::shared_ptr<RandomNumberGenerator> generator);

            static llvm::BasicBlock* CreateInvalidBogusBlock(llvm::Function* function, std::shared_ptr<RandomNumberGenerator> generator);
            static llvm::BasicBlock* ChainBogusIntoBlock(llvm::BasicBlock* block, llvm::BasicBlock* bogusBlock, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator, const SettingsParser::FunctionAttributes& attributes);
            static llvm::BasicBlock* ChainBogusIntoBlockRdtsc(llvm::BasicBlock* block, llvm::BasicBlock* bogusBlock, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator, const SettingsParser::FunctionAttributes& attributes);
            static llvm::BasicBlock* ChainBogusIntoBlockAntiDebug(llvm::BasicBlock* block, llvm::BasicBlock* bogusBlock, AntiDebugType antiDebugType, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator, EmittedTemplate& templates, const SettingsParser::FunctionAttributes& attributes);
        private:

            void ObfuscateFunction(llvm::Function& function, EmittedTemplate& templates);
            bool ObfuscateBlock(llvm::BasicBlock* block, SettingsParser::FunctionAttributes& attributes, EmittedTemplate& templates, std::shared_ptr<RandomNumberGenerator> generator);
            bool LinkTemplateModule(llvm::Module& module, EmittedTemplate& templates);
            std::string ComputeUniqueModuleTag(llvm::Module& module);
            
            static int RankValue(llvm::Value* value);
            static bool IsSafeToTimeAcross(llvm::Instruction &I);
            SettingsParser::PassArguments m_Arguments;
            Logger m_Logger;
    };

    class AntiDissasemblyEmitter : public llvm::MCCodeEmitter
    {
        std::unique_ptr<MCCodeEmitter> m_Real;
    public:
        AntiDissasemblyEmitter(std::unique_ptr<MCCodeEmitter> R) : m_Real(std::move(R)) {}

        void encodeInstruction(const llvm::MCInst& instruction, llvm::SmallVectorImpl<char>& bytes, llvm::SmallVectorImpl<llvm::MCFixup>& fixups, const llvm::MCSubtargetInfo& sti) const override;
    };
}
