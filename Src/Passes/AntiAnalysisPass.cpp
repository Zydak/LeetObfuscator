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

llvm::PreservedAnalyses LeetObfuscator::AntiAnalysisPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running AntiAnalysisPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

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
    {
        m_Logger.LogFunction(function, "Skipping function due to settings", 1);
        return;
    }

    m_Logger.LogFunction(function, "Processing function", 1);

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    {
        std::vector<llvm::BasicBlock*> blocksToObfuscate;
        for (auto& block : function)
        {
            if (generator->DrawRange(1u, 100u) > attributes.antiAnalysisProbability)
                continue;
            
            blocksToObfuscate.push_back(&block);
        }

        for (llvm::BasicBlock* block : blocksToObfuscate)
        {
            bool rdtsc = false;
            m_Logger.LogFunction(function, "Instrumenting basic block", 2);
            if (generator->DrawRange(1u, 100u) <= attributes.antiAnalysisRdtscProbability)
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
    // TODO: random position in function insert
    llvm::BasicBlock* bogusBlock = llvm::BasicBlock::Create(context, "leet.invalid.bogus", function, function->getEntryBlock().getNextNode());

    llvm::IRBuilder<> bogusBuilder(bogusBlock);

    std::vector<std::string> asmOptions;

    // Near CALL / JMP (eats 4)
    asmOptions.push_back(".byte 0xE8");
    asmOptions.push_back(".byte 0xE9");

    // PUSH imm (eats 4 or 1)
    asmOptions.push_back(".byte 0x68");
    asmOptions.push_back(".byte 0x6A");

    // RET / RETF imm (eats 2)
    asmOptions.push_back(".byte 0xC2");
    asmOptions.push_back(".byte 0xCA");

    // Far CALL / JMP (eats 6)
    asmOptions.push_back(".byte 0x9A");
    asmOptions.push_back(".byte 0xEA");

    // movabs (eats 8)
    asmOptions.push_back(".byte 0x48, 0xB8");
    asmOptions.push_back(".byte 0x48, 0xB9");
    asmOptions.push_back(".byte 0x48, 0xBA");
    asmOptions.push_back(".byte 0x48, 0xBB");
    asmOptions.push_back(".byte 0x48, 0xBC");
    asmOptions.push_back(".byte 0x48, 0xBD");
    asmOptions.push_back(".byte 0x48, 0xBE");
    asmOptions.push_back(".byte 0x48, 0xBF");

    // ENTER (eats 3)
    asmOptions.push_back(".byte 0xC8");

    // Multi byte escapes that force further decoding
    asmOptions.push_back(".byte 0x0F, 0x1F");
    asmOptions.push_back(".byte 0x0F, 0x0D");
    asmOptions.push_back(".byte 0x0F, 0x38");
    asmOptions.push_back(".byte 0x0F, 0x3A");

    // Long ModR/M + SIB forms (primary + ModR/M + SIB)
    static const uint8_t primaries[] = {
        0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, // duplicated for better distribution
        0xC7, // MOV
        0x69, // IMUL
        0xF7 // TEST
    };
    static const uint8_t longModRMs[] = { 0x84, 0x8C, 0x94, 0x9C, 0xA4, 0xAC, 0xB4, 0xBC };

    // Add one random long form for every primary and every long ModR/M
    for (uint8_t primary : primaries)
    {
        for (uint8_t modrm : longModRMs)
        {
            uint8_t sib = static_cast<uint8_t>(generator->DrawRange(0u, 255u));

            std::string seq;

            if (generator->DrawRange(0u, 2u) == 0) seq += ".byte 0x66\n";
            if (generator->DrawRange(0u, 2u) == 0) seq += ".byte 0x67\n";
            if (generator->DrawRange(0u, 3u) == 0) seq += ".byte 0x48\n";
            if (generator->DrawRange(0u, 4u) == 0) seq += ".byte 0x2E\n";
            if (generator->DrawRange(0u, 4u) == 0) seq += ".byte 0x3E\n";
            if (generator->DrawRange(0u, 5u) == 0) seq += ".byte 0xF0\n";

            seq += ".byte " + std::to_string(primary) + "\n";
            seq += ".byte " + std::to_string(modrm)   + "\n";
            seq += ".byte " + std::to_string(sib)     + "\n";

            asmOptions.push_back(std::move(seq));
        }
    }

    // Select one
    const std::string& selected = asmOptions[generator->DrawRange(0u, (uint32_t)asmOptions.size() - 1)];

    llvm::InlineAsm* inAsm = llvm::InlineAsm::get(
        llvm::FunctionType::get(bogusBuilder.getVoidTy(), false),
        selected,
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

bool LeetObfuscator::AntiAnalysisPass::IsSafeToTimeAcross(llvm::Instruction &instruction)
{
    // Exclude anything that can block, trap, or take unbounded/variable time.
    if (llvm::isa<llvm::CallBase>(instruction)) // covers CallInst, InvokeInst, CallBrInst
        return false;
    if (instruction.isAtomic()) // atomicrmw, cmpxchg, atomic load/store
        return false;
    if (llvm::isa<llvm::FenceInst>(instruction))
        return false;
    if (auto *LI = llvm::dyn_cast<llvm::LoadInst>(&instruction))
        if (LI->isVolatile())
            return false;
    if (auto *SI = llvm::dyn_cast<llvm::StoreInst>(&instruction))
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

    auto consider = [&](llvm::Value* value)
    {
        int r = RankValue(value);
        if (r < 0 || r >= bestRank)
            return;

        // A candidate is only safe to reuse at insertIt if it actually
        // dominates that point so check with with a tree
        if (auto* instruction = llvm::dyn_cast<llvm::Instruction>(value))
        {
            if (!tree.dominates(instruction, insertPointInst))
                return;
        }
        // Function arguments dominate every instruction in the function,
        // so no check is needed for those.

        bestRank = r;
        best = value;
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

    llvm::Function* function = block->getParent();
    for (llvm::Argument& Arg : function->args())
        consider(&Arg);
    return best;
}

int LeetObfuscator::AntiAnalysisPass::RankValue(llvm::Value* value)
{
    if (!value || llvm::isa<llvm::InlineAsm>(value))
        return -1;

    if (auto *function = llvm::dyn_cast<llvm::Function>(value))
        if (function->isIntrinsic())
            return -1;

    if (auto *callInstruction = llvm::dyn_cast<llvm::CallInst>(value))
        if (llvm::isa<llvm::InlineAsm>(callInstruction->getCalledOperand()))
            return -1;

    llvm::Type* Ty = value->getType();
    bool isConst = llvm::isa<llvm::Constant>(value);

    if (Ty->isIntegerTy())
    {
        unsigned bits = Ty->getIntegerBitWidth();
        // no i1, no i128+, no odd widths, they're not allocated in normal registers
        // and LLVM doesn't like that fact
        if (bits != 8 && bits != 16 && bits != 32 && bits != 64)
            return -1;
        return isConst ? 2 : 0; // non const int can't do better
    }
    if (Ty->isPointerTy())
        return isConst ? 3 : 1;

    return -1;
}
