#include "BlockSplitterPass.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "SettingsParser.h"

#include <random>

llvm::PreservedAnalyses LeetObfuscator::BlockSplitterPass::run(llvm::Module &module, llvm::ModuleAnalysisManager &)
{
    for (auto& function : module)
    {
        SplitFunction(function);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::BlockSplitterPass::SplitFunction(llvm::Function& function)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(function);
    if (attributes.skip)
    {
        return;
    }

    std::vector<llvm::BasicBlock*> blocksToSplit;
    for (auto& block : function)
    {
        if (block.size() > attributes.maxBlockSize)
        {
            blocksToSplit.push_back(&block);
        }
    }

    while(!blocksToSplit.empty())
    {
        llvm::BasicBlock* block = blocksToSplit.back();
        //llvm::errs() << "SPLITTING: " << block->getParent()->getName() << "\n";
        blocksToSplit.pop_back();
        if (block->size() <= attributes.maxBlockSize)
        {
            continue;
        }

        auto it = block->begin();
        std::advance(it, attributes.maxBlockSize);

        llvm::BasicBlock* newBlock = block->splitBasicBlock(it);

        blocksToSplit.push_back(newBlock);
    }

    // Shuffle the blocks around in memory (except for the entry block)
    std::vector<llvm::BasicBlock*> blocksToShuffle;
    for (auto& block : function)
    {
        if (&block != &function.getEntryBlock())
        {
            blocksToShuffle.push_back(&block);
        }
    }

    std::shuffle(blocksToShuffle.begin(), blocksToShuffle.end(), std::mt19937(std::random_device{}()));

    for (auto* block : blocksToShuffle)
    {
        block->moveAfter(&function.getEntryBlock());
    }
}
