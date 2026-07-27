#include "AntiAnalysisPass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Verifier.h"

#include <random>
#include <algorithm>

static constexpr const char *ANTI_ANALYSIS_TAG = "leet.AA";

llvm::PreservedAnalyses LeetObfuscator::AntiAnalysisPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running AntiAnalysisPass\n";

    for (auto& function : module)
    {
        ObfuscateFunction(function);
    }

    return llvm::PreservedAnalyses::none();
}

// Ranks a candidate value: lower is better, -1 means unusable.
static int RankValue(llvm::Value* V)
{
    if (!V || llvm::isa<llvm::InlineAsm>(V))
        return -1;
    llvm::CallInst *CI = llvm::dyn_cast<llvm::CallInst>(V);
    if (CI)
    {
        if (llvm::isa<llvm::InlineAsm>(CI->getCalledOperand()))
        {
            return -1;
        }
    }
    llvm::Type* Ty = V->getType();
    bool isConst = llvm::isa<llvm::Constant>(V);
    if (Ty->isIntegerTy())
        return isConst ? 2 : 0;
    if (Ty->isPointerTy())
        return isConst ? 3 : 1;
    return -1;
}

// Check every instruction and operand before insertIt
static llvm::Value* FindUsableInput(llvm::BasicBlock* block, llvm::BasicBlock::iterator insertIt)
{
    llvm::Value* best = nullptr;
    int bestRank = INT_MAX;

    auto consider = [&](llvm::Value* V)
    {
        int r = RankValue(V);
        if (r >= 0 && r < bestRank)
        {
            bestRank = r;
            best = V;
        }
    };

    bool isInsertPoint = true; // insertIt's own result hasn't executed yet so skip it
    for (auto it = insertIt; ; --it)
    {
        if (it != block->end())
        {
            llvm::Instruction* inst = &*it;
            if (!isInsertPoint)
                consider(inst);
            for (llvm::Value* Op : inst->operands())
                consider(Op);
            if (bestRank == 0)
                return best; // non-constant int, can't do better
        }
        isInsertPoint = false;
        if (it == block->begin())
            break;
    }
    if (best)
        return best;

    llvm::Function* F = block->getParent();
    for (llvm::Argument& Arg : F->args())
        consider(&Arg);
    return best;
}

void LeetObfuscator::AntiAnalysisPass::ObfuscateFunction(llvm::Function &function)
{
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> distBlock(0, 100);

    {
        uint32_t testProb = 100;
        std::vector<llvm::BasicBlock*> blocksToObfuscate;
        for (auto& block : function)
        {
            // Check if it hasn't been split already
            llvm::Instruction* terminator = block.getTerminator();
            if (terminator->getMetadata(ANTI_ANALYSIS_TAG))
            {
                continue; // This was already split
            }
            
            if (testProb >= distBlock(rng))
            {
                blocksToObfuscate.push_back(&block);
            }
        }

        for (llvm::BasicBlock* block : blocksToObfuscate)
        {
            ObfuscateBlock(block);
        }
    }

    {
        uint32_t testProb = 100;
        std::vector<llvm::BasicBlock*> blocksToObfuscate;
        for (auto& block : function)
        {
            // Check if it hasn't been split already
            llvm::Instruction* terminator = block.getTerminator();
            if (terminator->getMetadata(ANTI_ANALYSIS_TAG))
            {
                continue; // This was already split
            }
            
            if (testProb >= distBlock(rng))
            {
                blocksToObfuscate.push_back(&block);
            }
        }

        for (llvm::BasicBlock* block : blocksToObfuscate)
        {
            ObfuscateBlock(block, true);
        }
    }

    // Verify the function at the end
    if (llvm::verifyFunction(function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] AntiAnalysisPass: Function '" << function.getName() << "' verification failed after transformation!\n";

        // Dump the function IR and terminate
        
        std::error_code ec;
        llvm::raw_fd_ostream logFile("error_log.txt", ec);
        if (!ec)
        {
            function.print(logFile);
            logFile.close();
            llvm::errs() << "AntiAnalysisPass: Function IR dumped to error_log.txt\n";
        }
        else
        {
            llvm::errs() << "AntiAnalysisPass: Failed to open error_log.txt for writing: " << ec.message() << "\n";
        }
        exit(1);
    }
}

bool LeetObfuscator::AntiAnalysisPass::ObfuscateBlock(llvm::BasicBlock* block, bool randomPos)
{
    llvm::BasicBlock::iterator insertIt;
    std::mt19937 rng(std::random_device{}());

    if (randomPos)
    {
        unsigned count = 0;
        for (llvm::Instruction& instruction : *block)
        {
            count++;
        }

        std::uniform_int_distribution<uint32_t> distInstruction(0, count);
        uint32_t insertPoint = distInstruction(rng);

        insertIt = block->begin();

        llvm::BasicBlock::iterator safeInsertPt = block->getFirstInsertionPt(); // skips PHIs (and landingpads)
        if (insertIt != safeInsertPt &&
            insertIt->comesBefore(&*safeInsertPt)) {
            insertIt = safeInsertPt;
        }
        std::advance(insertIt, insertPoint);
    }
    else
    {
        insertIt = block->getFirstInsertionPt();

        insertIt++;
    }

    llvm::Function* function = block->getParent();

    if (insertIt == block->end())
    {
        return false;
    }

    llvm::Value* input = FindUsableInput(&*block, insertIt);
    if (!input)
    {
        return false;
    }

    llvm::BasicBlock* newBlock = block->splitBasicBlock(insertIt);
    llvm::BasicBlock* bogus = llvm::BasicBlock::Create(block->getContext(), "", function, newBlock);

    block->getTerminator()->eraseFromParent();
    llvm::IRBuilder<> originalBlockBuilder(block, block->end());

    if (input->getType()->isPointerTy())
    {
        const llvm::DataLayout& DL = function->getParent()->getDataLayout();
        llvm::Type* intPtrTy = DL.getIntPtrType(input->getType());
        input = originalBlockBuilder.CreatePtrToInt(input, intPtrTy);
    }

    // technically useless, but keeps LLVM from opting this shit out
    llvm::InlineAsm* identity = llvm::InlineAsm::get(
        llvm::FunctionType::get(input->getType(), {input->getType()}, false), "", "=r,0", /*hasSideEffects=*/true
    );
    llvm::Value* opaqueInput = originalBlockBuilder.CreateCall(identity, {input});
    llvm::Value* xoredInput = originalBlockBuilder.CreateXor(input, opaqueInput);
    llvm::Value* condition = originalBlockBuilder.CreateICmpEQ(xoredInput, llvm::ConstantInt::get(xoredInput->getType(), 0));

    originalBlockBuilder.CreateCondBr(condition, newBlock, bogus);

    // mark the smaller block as split
    if (newBlock->size() <= block->size())
        newBlock->getTerminator()->setMetadata(ANTI_ANALYSIS_TAG, llvm::MDNode::get(block->getContext(), {}));
    else
        block->getTerminator()->setMetadata(ANTI_ANALYSIS_TAG, llvm::MDNode::get(block->getContext(), {}));

    llvm::IRBuilder<> bogusBuilder(bogus);

    std::vector<const char*> asmOptions = {
        "0x0F",
        "0x68",
        "0xF2",
        "0x6A",
        "0xE9",
        "0xC7",
        "0xF6",
        "0xF7",
        "0xFE",
        "0x6B",
        "0x0F,0x3A"
    };

    std::uniform_int_distribution<uint32_t> asmDist(0, asmOptions.size() - 1);

    const char* selectedAsm = asmOptions[asmDist(rng)];

    llvm::InlineAsm* inAsm = llvm::InlineAsm::get(
        llvm::FunctionType::get(bogusBuilder.getVoidTy(), false),
        ".byte " + std::string(selectedAsm),
        "",
        true
    );

    bogusBuilder.CreateCall(inAsm);
    llvm::BranchInst* bogusTerm = bogusBuilder.CreateBr(newBlock);
    bogusTerm->setMetadata(ANTI_ANALYSIS_TAG, llvm::MDNode::get(block->getContext(), {}));

    return true;
}
