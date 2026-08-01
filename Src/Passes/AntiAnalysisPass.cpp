#include "AntiAnalysisPass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IntrinsicsX86.h"
#include "SettingsParser.h"

#include <random>
#include <algorithm>
#include <math.h>

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

void LeetObfuscator::AntiAnalysisPass::ObfuscateFunction(llvm::Function &function)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(
        function, SettingsParser::PassType::AntiAnalysisPass, m_Arguments
    );
    
    if (SettingsParser::ShouldSkipFunction(&function, attributes))
        return;

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    {
        std::vector<llvm::BasicBlock*> blocksToObfuscate;
        for (auto& block : function)
        {
            if (generator->DrawRange(0u, 100u) > attributes.antiAnalysisProbability)
                continue;
            
            blocksToObfuscate.push_back(&block);
        }

        for (llvm::BasicBlock* block : blocksToObfuscate)
        {
            bool rdtsc = false;
            if (generator->DrawRange(0u, 100u) <= attributes.antiAnalysisRdtscProbability)
                rdtsc = true;
            
            if (attributes.antiAnalysisInsertPosition == SettingsParser::BogusInsertPosition::Start)
                ObfuscateBlock(block, false, generator, rdtsc);
            else
            {
                if (!ObfuscateBlock(block, true, generator, rdtsc))
                    ObfuscateBlock(block, true, generator, rdtsc); // Try one reroll
            }
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

bool LeetObfuscator::AntiAnalysisPass::ObfuscateBlock(llvm::BasicBlock* block, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator, bool rdtsc)
{
    llvm::BasicBlock* bogus = CreateInvalidBogusBlock(block->getParent(), generator);
    llvm::BasicBlock* newSplitBlock = nullptr;

    if (rdtsc)
        newSplitBlock = ChainBogusIntoBlockRdtsc(block, bogus, randomPos, generator);
    else
        newSplitBlock = ChainBogusIntoBlock(block, bogus, randomPos, generator);

    if (!newSplitBlock || !bogus)
    {
        bogus->eraseFromParent();
        return false;
    }

    return true;
}


llvm::BasicBlock *LeetObfuscator::AntiAnalysisPass::CreateInvalidBogusBlock(llvm::Function* function, std::shared_ptr<RandomNumberGenerator> generator)
{
    llvm::LLVMContext& context = function->getContext();
    llvm::BasicBlock* bogusBlock = llvm::BasicBlock::Create(context, "leet.invalid.bogus", function, function->getEntryBlock().getNextNode()); // TODO random pos in func

    llvm::IRBuilder<> bogusBuilder(bogusBlock);

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
    };

    const char* selectedAsm = asmOptions[generator->DrawRange(0u, (uint32_t)asmOptions.size() - 1)];

    llvm::InlineAsm* inAsm = llvm::InlineAsm::get(
        llvm::FunctionType::get(bogusBuilder.getVoidTy(), false),
        ".byte " + std::string(selectedAsm),
        "",
        true
    );

    bogusBuilder.CreateCall(inAsm);

    return bogusBlock;
}

llvm::BasicBlock* LeetObfuscator::AntiAnalysisPass::ChainBogusIntoBlock(llvm::BasicBlock *block, llvm::BasicBlock *bogusBlock, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator)
{
    llvm::Function* function = block->getParent();

    auto insertPoint = block->getFirstInsertionPt();
    
    uint32_t instructionCount = 0;
    for (auto it = block->getFirstInsertionPt(); it != block->end(); it++)
    {
        instructionCount++;
    }

    if(randomPos)
    {
        uint32_t t = generator->DrawRange(0u, (uint32_t)std::max(int(instructionCount)-1, 0));
        std::advance(insertPoint, t);
    }

    if (insertPoint == block->end())
    {
        return nullptr;
    }

    llvm::DominatorTree tree(*function);
    llvm::Value* input = FindUsableInput(tree, block, insertPoint);
    if (!input)
        return nullptr;

    llvm::BasicBlock* newSplitBlock = block->splitBasicBlock(insertPoint);
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

    originalBlockBuilder.CreateCondBr(condition, newSplitBlock, bogusBlock);

    llvm::IRBuilder<> bogusBlockBuilder(bogusBlock, bogusBlock->end());
    bogusBlockBuilder.CreateBr(newSplitBlock);

    return newSplitBlock;
}

bool LeetObfuscator::AntiAnalysisPass::IsSafeToTimeAcross(llvm::Instruction &I)
{
    // Exclude anything that can block, trap, or take unbounded/variable time.
    if (llvm::isa<llvm::CallBase>(I)) // covers CallInst, InvokeInst, CallBrInst
        return false;
    if (I.isAtomic()) // atomicrmw, cmpxchg, atomic load/store
        return false;
    if (llvm::isa<llvm::FenceInst>(I))
        return false;
    if (auto *LI = llvm::dyn_cast<llvm::LoadInst>(&I))
        if (LI->isVolatile())
            return false;
    if (auto *SI = llvm::dyn_cast<llvm::StoreInst>(&I))
        if (SI->isVolatile())
            return false;
    return true;
}

llvm::BasicBlock *LeetObfuscator::AntiAnalysisPass::ChainBogusIntoBlockRdtsc(llvm::BasicBlock *block, llvm::BasicBlock *bogusBlock, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator)
{
    // Instead of inserting a normal check that is always true insert an rdtsc check, this will also prevent any debugging

    llvm::Module* module = block->getModule();

    // Check if theres at least 3 instructions forward from the start
    uint32_t instructionCount = 0;
    for (auto it = block->getFirstNonPHIOrDbgOrAlloca(); it != block->end(); it++)
    {
        instructionCount++;
    }
    if (instructionCount < 3)
        return nullptr;

    auto startIt = block->getFirstNonPHIOrDbgOrAlloca();
    if (randomPos)
    {
        uint32_t t = generator->DrawRange(0u, (uint32_t)std::max(int(instructionCount)-5, 0));
        std::advance(startIt, t);
    }

    uint32_t secondTimerStep = 0;
    for (auto it = startIt; it != block->end(); it++)
    {
        if (!IsSafeToTimeAcross(*it))
            break;
        if (secondTimerStep >= 10)
            break;
        if (it == block->end())
            break;
        
        secondTimerStep++;
    }

    if (secondTimerStep < 3)
        return nullptr;

    auto blockIt = startIt;
    llvm::IRBuilder<> originalBlockBuilder(block, blockIt);
    originalBlockBuilder.SetCurrentDebugLocation(llvm::DebugLoc());
    llvm::Function* rdtscIntr = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::x86_rdtsc);
    llvm::Value* rdtscStart = originalBlockBuilder.CreateCall(rdtscIntr, {}, "rdtsc");

    std::advance(blockIt, secondTimerStep-1);

    llvm::BasicBlock* newSplitBlock = block->splitBasicBlock(blockIt);
    block->getTerminator()->eraseFromParent();
    originalBlockBuilder.SetInsertPoint(block);
    originalBlockBuilder.SetCurrentDebugLocation(llvm::DebugLoc());

    llvm::Value* rdtscEnd = originalBlockBuilder.CreateCall(rdtscIntr, {}, "rdtsc");
    llvm::Value* time = originalBlockBuilder.CreateSub(rdtscEnd, rdtscStart);

    llvm::Value* condition = originalBlockBuilder.CreateICmpUGE(time, originalBlockBuilder.getInt64(100000000ULL));
    originalBlockBuilder.CreateCondBr(condition, bogusBlock, newSplitBlock);

    llvm::IRBuilder<> bogusBlockBuilder(bogusBlock, bogusBlock->end());
    bogusBlockBuilder.CreateBr(newSplitBlock);

    return newSplitBlock;
}

// Check every instruction and operand before insertIt
llvm::Value* LeetObfuscator::AntiAnalysisPass::FindUsableInput(llvm::DominatorTree& tree, llvm::BasicBlock* block, llvm::BasicBlock::iterator insertIt)
{
    llvm::Value* best = nullptr;
    int bestRank = INT_MAX;

    // insertIt is always a real instruction here (callers check insertIt != block->end()
    // before calling FindUsableInput), so this is safe to dereference.
    llvm::Instruction* insertPointInst = &*insertIt;

    auto consider = [&](llvm::Value* V)
    {
        int r = RankValue(V);
        if (r < 0 || r >= bestRank)
            return;

        // A candidate is only safe to reuse at insertIt if it actually
        // dominates that point so check with with a tree
        if (auto* I = llvm::dyn_cast<llvm::Instruction>(V))
        {
            if (!tree.dominates(I, insertPointInst))
                return;
        }
        // Function arguments dominate every instruction in the function,
        // so no check is needed for those.

        bestRank = r;
        best = V;
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

// Ranks a candidate value: lower is better, -1 means unusable.
int LeetObfuscator::AntiAnalysisPass::RankValue(llvm::Value* V)
{
    if (!V || llvm::isa<llvm::InlineAsm>(V))
        return -1;

    // Reject intrinsic functions like llvm.x86.rdtsc.
    // They cannot be used as ordinary values / addresses for whatever reason
    if (auto *F = llvm::dyn_cast<llvm::Function>(V)) {
        if (F->isIntrinsic())
            return -1;
    }

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
