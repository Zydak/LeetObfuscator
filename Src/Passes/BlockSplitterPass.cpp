#include "BlockSplitterPass.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"

#include <random>

llvm::PreservedAnalyses LeetObfuscator::BlockSplitterPass::run(llvm::Module &module, llvm::ModuleAnalysisManager &)
{
    const int MAX_BLOCK_SIZE = 20; // TODO: config file

    std::vector<llvm::BasicBlock*> blocksToSplit;
    for (auto& function : module)
    {
        for (auto& block : function)
        {
            if (block.size() > MAX_BLOCK_SIZE)
            {
                blocksToSplit.push_back(&block);
            }
        }
    }

    while(!blocksToSplit.empty())
    {
        llvm::BasicBlock* block = blocksToSplit.back();
        blocksToSplit.pop_back();
        if (block->size() <= MAX_BLOCK_SIZE)
        {
            continue;
        }

        auto it = block->begin();
        std::advance(it, MAX_BLOCK_SIZE);

        llvm::BasicBlock* newBlock = block->splitBasicBlock(it);

        blocksToSplit.push_back(newBlock);
    }

    // Shuffle the blocks around in memory (except for the entry block)
    for (auto& func : module)
    {
        std::vector<llvm::BasicBlock*> blocksToShuffle;
        for (auto& block : func)
        {
            if (&block != &func.getEntryBlock())
            {
                blocksToShuffle.push_back(&block);
            }
        }

        std::shuffle(blocksToShuffle.begin(), blocksToShuffle.end(), std::mt19937(std::random_device{}()));

        for (auto* block : blocksToShuffle)
        {
            block->moveAfter(&func.getEntryBlock());
        }
    }

    return llvm::PreservedAnalyses::none();
}