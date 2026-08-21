#pragma once

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
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
            : m_Arguments(std::move(arguments)), m_Logger("VariableSplittingPass") {}
        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

        // A value represented as parts
        struct PartsInfo
        {
            std::vector<llvm::Value*> parts;
            uint32_t partByteSize = 0;
        };

        struct SplitContext
        {
            std::unordered_map<llvm::Instruction*, PartsInfo> partsMap;
            std::vector<llvm::Instruction*> toErase;
            SettingsParser::FunctionAttributes* attributes;
        };

    private:

        void ObfuscateFunction(llvm::Function* function);
        bool IsAllocaSplittable(llvm::AllocaInst* allocaInstruciton);
        void SplitAlloca(llvm::AllocaInst* allocaInstruction, llvm::IntegerType* intType, uint64_t byteSize, uint32_t pieceByteSize, SplitContext& splitContext);
        llvm::Value* MergeParts(llvm::IRBuilder<>& builder, const PartsInfo& parts, llvm::IntegerType* type);
        std::vector<llvm::Value*> SplitValue(llvm::IRBuilder<>& builder, llvm::Value* value, uint32_t pieceByteSize);
        bool GetOperandAsParts(llvm::IRBuilder<>& builder, llvm::Value* operand, SplitContext& splitContext, PartsInfo& outParts);
        void RewriteInstruction(llvm::Instruction* instruction, SplitContext& splitContext);
        void MaterializePhiOperands(llvm::PHINode* phi, SplitContext& splitContext);
        bool IsValueSplit(llvm::Value* value, const LeetObfuscator::VariableSplittingPass::SplitContext& splitContext);

        // Splitting-granularity / probability helpers
        uint32_t ComputePartByteSize(uint64_t totalByteSize, uint32_t splitCount);
        void AlignPartsToCommonSize(llvm::IRBuilder<>& builder, PartsInfo& lhs, PartsInfo& rhs);

        bool TryRewriteBitwise(llvm::Instruction* instruction, SplitContext& splitContext);
        bool TryRewriteAdd(llvm::Instruction* instruction, SplitContext& splitContext);
        bool TryRewriteSub(llvm::Instruction* instruction, SplitContext& splitContext);
        bool TryRewriteIcmpEq(llvm::Instruction* instruction, SplitContext& splitContext);

        SettingsParser::PassArguments m_Arguments;
        Logger m_Logger;
    };
}