#include "DispatcherPass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Scalar/Reg2Mem.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <random>
#include "SettingsParser.h"
#include "llvm/IR/NoFolder.h"

llvm::PreservedAnalyses LeetObfuscator::DispatcherPass::run(llvm::Module &module, llvm::ModuleAnalysisManager& mam)
{
    llvm::errs() << "Running DispatcherPass\n";

    for (auto& function : module)
    {
        CreateDispatcherInAFunction(&function, mam);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::DispatcherPass::CreateDispatcherInAFunction(llvm::Function *function, llvm::ModuleAnalysisManager &mam)
{
    // Skip functions with exception handling
    if (function->hasPersonalityFn())
    {
        llvm::errs() << "Function '" << function->getName() << "' has a personality function, skipping dispatcher creation.\n";
        return;
    }

    bool hasAnyBlocks = false;
    for (auto& block : *function)
    {
        if (&block != &function->getEntryBlock())
        {
            hasAnyBlocks = true;
            break;
        }
    }
    if (!hasAnyBlocks)
    {
        return; // nothing to do
    }

    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(*function);
    if (attributes.skip)
    {
        return;
    }
    
    llvm::Module* module = function->getParent();

    // Lifetime intrinsics require their pointer operand to trace directly to an
    // alloca. Once we demote across the dispatcher's shuffled blocks, that
    // breaks since the RegToMem pass spills the alloca pointer itself into another
    // slot. And since it's an llvm instrisic used only for optimization, just purge
    // all these fuckers from existence, begone
    std::vector<llvm::CallInst*> lifetimeCalls;
    for (auto& block : *function)
    {
        for (auto& inst : block)
        {
            if (auto* call = llvm::dyn_cast<llvm::CallInst>(&inst))
            {
                if (auto* callee = call->getCalledFunction())
                {
                    if (callee->getIntrinsicID() == llvm::Intrinsic::lifetime_start ||
                        callee->getIntrinsicID() == llvm::Intrinsic::lifetime_end)
                    {
                        lifetimeCalls.push_back(call);
                    }
                }
            }
        }
    }
    for (auto* call : lifetimeCalls)
        call->eraseFromParent();
    
    // The code will technically be valid but the verifier will still complain about uses before initialization
    // so demote everything to stack first. You could call MemToRegPass after everything is done but it will
    // generate insane amount of instructions because blocks will be jumping between demoting to memory and promoting
    // to stack on every entry, if you have a lot of blocks it's just bloat that doesn't do anything. So I just
    // do RegToMem without promoting it back afterwards
    auto &fam = mam.getResult<llvm::FunctionAnalysisManagerModuleProxy>(*module).getManager();
    llvm::RegToMemPass().run(*function, fam);
    
    llvm::LLVMContext& context = module->getContext();
    std::vector<llvm::BasicBlock*> basicBlocks;
    for (auto& basicBlock : *function)
    {
        if (&basicBlock != &function->getEntryBlock())
        {
            basicBlocks.push_back(&basicBlock);
        }
    }

    if (basicBlocks.empty())
        return; // nothing to do

    // Split entry block after initialization so that there's no logic in the entry block, everything will go through the dispatcher
    llvm::BasicBlock* entryBlock = &function->getEntryBlock();
    llvm::Instruction* firstNonAlloca = llvm::dyn_cast<llvm::Instruction>(entryBlock->getFirstNonPHIOrDbgOrAlloca());
    if (!firstNonAlloca)
    {
        llvm::errs() << "ERROR: Function '" << function->getName() << "' has no non-alloca instructions in the entry block, skipping.\n";
        return;
    }
    llvm::BasicBlock* bodyBlock = entryBlock->splitBasicBlock(firstNonAlloca);
    basicBlocks.insert(basicBlocks.begin(), bodyBlock);

    // Shuffle the blocks around so indices in the jump table are less obvious
    std::mt19937 rng(std::random_device{}());
    std::shuffle(basicBlocks.begin(), basicBlocks.end(), rng);
    // Find body block index after shuffle
    auto it = std::find(basicBlocks.begin(), basicBlocks.end(), bodyBlock);
    uint32_t bodyIndex = it - basicBlocks.begin();

    uint32_t tableSizeLog2 = static_cast<uint32_t>(std::ceil(std::log2(basicBlocks.size() * 1.3)));
    uint32_t tableSize = 1u << tableSizeLog2;

    auto getSlotFromID = [&](uint32_t id) -> uint32_t
    {
        return (id * 2654435761u) >> (32 - tableSizeLog2);
    };

    // Generate random block IDs
    std::vector<uint32_t> blockIds(basicBlocks.size(), -1);
    std::vector<int> slotOwner(tableSize, -1); 
    for (size_t i = 0; i < basicBlocks.size(); i++)
    {
        uint32_t slot;
        uint32_t randomID;
        do
        {
            randomID = rng();
            slot = getSlotFromID(randomID);
        } while (slotOwner[slot] != -1);

        slotOwner[slot] = (int)i;
        blockIds[i] = randomID;
    }
    
    // Allocate jump table with all the addresses
    llvm::IRBuilder<> entryBeginBuilder(entryBlock, entryBlock->begin());
    llvm::ArrayType* jumpTableType = llvm::ArrayType::get(entryBeginBuilder.getPtrTy(), tableSize);
    llvm::AllocaInst* jumpTable = entryBeginBuilder.CreateAlloca(jumpTableType);

    // Because of the hashing some slots will be unoccupied, so just insert a dead block there just in case
    // TODO: make some bogus blocks with random ass logic
    llvm::BasicBlock* trapBlock = llvm::BasicBlock::Create(context, "", function);
    llvm::IRBuilder<>(trapBlock).CreateUnreachable();
    for (uint32_t i = 0; i < tableSize; i++)
    {
        if (slotOwner[i] == -1)
        {
            llvm::Value* blockAddress = entryBeginBuilder.CreateInBoundsGEP(
                jumpTableType,
                jumpTable,
                {entryBeginBuilder.getInt32(0), entryBeginBuilder.getInt32(i)}
            );

            entryBeginBuilder.CreateStore(llvm::BlockAddress::get(trapBlock), blockAddress);
        }
    }

    for (size_t i = 0; i < basicBlocks.size(); i++)
    {
        llvm::Value* blockAddress = entryBeginBuilder.CreateInBoundsGEP(
            jumpTableType,
            jumpTable,
            {entryBeginBuilder.getInt32(0), entryBeginBuilder.getInt32(getSlotFromID(blockIds[i]))}
        );

        entryBeginBuilder.CreateStore(llvm::BlockAddress::get(basicBlocks[i]), blockAddress);
    }

    // Allocate dispatcher state
    llvm::AllocaInst* dispatcherState = entryBeginBuilder.CreateAlloca(entryBeginBuilder.getInt32Ty());

    // Create block for the dispatcher and make entry jump to it
    llvm::BasicBlock* dispatcherBlock = llvm::BasicBlock::Create(context, "dispatcher", function);
    entryBlock->getTerminator()->eraseFromParent();
    llvm::IRBuilder<> entryEndBuilder(entryBlock, entryBlock->end());
    entryEndBuilder.CreateStore(entryEndBuilder.getInt32(blockIds[bodyIndex]), dispatcherState);
    entryEndBuilder.CreateBr(dispatcherBlock);

    // Create dispatcher
    llvm::IRBuilder<> dispatcherBuilder(dispatcherBlock);

    // Create an empty barrier function to prevent optimization of the dispatcher block
    llvm::FunctionType* barrierFnType = llvm::FunctionType::get(dispatcherBuilder.getVoidTy(), false);
    llvm::Function* barrierFn = llvm::Function::Create(barrierFnType, llvm::GlobalValue::InternalLinkage, "dispatcher_barrier", module);
    barrierFn->addFnAttr(llvm::Attribute::NoDuplicate);
    barrierFn->addFnAttr(llvm::Attribute::Convergent);
    barrierFn->addFnAttr(llvm::Attribute::NoInline);
    barrierFn->addFnAttr(llvm::Attribute::OptimizeNone);

    llvm::BasicBlock* barrierBlock = llvm::BasicBlock::Create(context, "barrier", barrierFn);
    llvm::IRBuilder<> barrierBuilder(barrierBlock);
    barrierBuilder.CreateRetVoid();

    dispatcherBuilder.CreateCall(barrierFn);

    llvm::Value* dispatcherStateLoad = dispatcherBuilder.CreateLoad(dispatcherBuilder.getInt32Ty(), dispatcherState, true);
    llvm::Value* hashed = dispatcherBuilder.CreateMul(dispatcherStateLoad, dispatcherBuilder.getInt32(2654435761u));
    llvm::Value* slot = dispatcherBuilder.CreateLShr(hashed, dispatcherBuilder.getInt32(32 - tableSizeLog2));

    // PROPER INDIRECT JUMPS
    //
    llvm::Value* nextBlockGEP = dispatcherBuilder.CreateInBoundsGEP(
        jumpTableType,
        jumpTable,
        {dispatcherBuilder.getInt32(0), slot}
    );
    llvm::Value* nextBlockAddress = dispatcherBuilder.CreateLoad(dispatcherBuilder.getPtrTy(), nextBlockGEP, true);
    llvm::IndirectBrInst* indirectBr = dispatcherBuilder.CreateIndirectBr(nextBlockAddress, basicBlocks.size());
    for (auto* basicBlock : basicBlocks)
    {
        indirectBr->addDestination(basicBlock);
    }
    indirectBr->addDestination(trapBlock);

    // // Not indirect branches for debugging
    // llvm::SwitchInst* sw = dispatcherBuilder.CreateSwitch(dispatcherStateXored, trapBlock, basicBlocks.size());
    // for (size_t i = 0; i < basicBlocks.size(); i++)
    // {
    //     sw->addCase(dispatcherBuilder.getInt32(blockIds[i]), basicBlocks[i]);
    // }

    // Rewrite every block terminator to change dispatcher state to the next block and jump back to dispatcher block.
    // We only rewrite successors that stay within the dispatcher-managed block set; external exits are left intact.
    auto rewriteToDispatcher = [&](llvm::Instruction* terminator, llvm::Value* nextIndex)
    {
        llvm::IRBuilder<> terminatorBuilder(terminator);
        terminatorBuilder.CreateStore(nextIndex, dispatcherState, true);
        // terminatorBuilder.CreateBr(dispatcherBlock);
        llvm::BlockAddress* dispatcherBlockAddress = llvm::BlockAddress::get(dispatcherBlock);
        llvm::IndirectBrInst* indirectBr = terminatorBuilder.CreateIndirectBr(dispatcherBlockAddress, 1);
        // for (auto* basicBlock : basicBlocks)
        // {
        //     if (terminator->getParent() == basicBlock)
        //         continue; // Don't add self as successor, it will cause infinite loop
        //     indirectBr->addDestination(basicBlock);
        // }
        indirectBr->addDestination(dispatcherBlock);

        terminator->eraseFromParent();
    };

    for (auto* basicBlock : basicBlocks)
    {
        llvm::Instruction* terminator = basicBlock->getTerminator();
        if (!terminator)
            continue;

        if (auto* branch = llvm::dyn_cast<llvm::BranchInst>(terminator))
        {
            if (branch->isUnconditional())
            {
                llvm::BasicBlock* successor = branch->getSuccessor(0);
                auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
                if (it == basicBlocks.end())
                    continue;

                uint32_t successorIndex = blockIds[it - basicBlocks.begin()];
                llvm::IRBuilder<> terminatorBuilder(branch);
                rewriteToDispatcher(branch, terminatorBuilder.getInt32(successorIndex));
            }
            else if (branch->isConditional())
            {
                llvm::BasicBlock* trueSuccessor = branch->getSuccessor(0);
                llvm::BasicBlock* falseSuccessor = branch->getSuccessor(1);
                auto itTrue = std::find(basicBlocks.begin(), basicBlocks.end(), trueSuccessor);
                auto itFalse = std::find(basicBlocks.begin(), basicBlocks.end(), falseSuccessor);
                if (itTrue == basicBlocks.end() || itFalse == basicBlocks.end())
                    continue;

                uint32_t trueSuccessorIndex = blockIds[itTrue - basicBlocks.begin()];
                uint32_t falseSuccessorIndex = blockIds[itFalse - basicBlocks.begin()];
                llvm::IRBuilder<> terminatorBuilder(branch);
                llvm::Value* nextIndex = terminatorBuilder.CreateSelect(
                    branch->getCondition(),
                    terminatorBuilder.getInt32(trueSuccessorIndex),
                    terminatorBuilder.getInt32(falseSuccessorIndex)
                );

                rewriteToDispatcher(branch, nextIndex);
            }
            continue;
        }

        if (auto* switchInst = llvm::dyn_cast<llvm::SwitchInst>(terminator))
        {
            llvm::BasicBlock* defaultDest = switchInst->getDefaultDest();
            auto defaultIt = std::find(basicBlocks.begin(), basicBlocks.end(), defaultDest);
            if (defaultIt == basicBlocks.end())
                continue;

            llvm::IRBuilder<> terminatorBuilder(switchInst);
            llvm::Value* nextIndex = terminatorBuilder.getInt32(blockIds[defaultIt - basicBlocks.begin()]);
            for (auto caseIt = switchInst->case_begin(); caseIt != switchInst->case_end(); ++caseIt)
            {
                llvm::BasicBlock* caseSuccessor = caseIt->getCaseSuccessor();
                auto caseSuccessorIt = std::find(basicBlocks.begin(), basicBlocks.end(), caseSuccessor);
                if (caseSuccessorIt == basicBlocks.end())
                    continue;

                llvm::Value* caseIndex = terminatorBuilder.getInt32(blockIds[caseSuccessorIt - basicBlocks.begin()]);
                llvm::Value* condition = terminatorBuilder.CreateICmpEQ(
                    switchInst->getCondition(),
                    caseIt->getCaseValue()
                );
                nextIndex = terminatorBuilder.CreateSelect(condition, caseIndex, nextIndex);
            }

            rewriteToDispatcher(switchInst, nextIndex);
        }

        if (auto* indirectBr = llvm::dyn_cast<llvm::IndirectBrInst>(terminator))
        {
            llvm::IRBuilder<> terminatorBuilder(indirectBr);
            llvm::Value* nextIndex = nullptr;
            for (unsigned i = 0; i < indirectBr->getNumSuccessors(); ++i)
            {
                llvm::BasicBlock* successor = indirectBr->getSuccessor(i);
                auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
                if (it == basicBlocks.end())
                    continue;

                llvm::Value* successorIndex = terminatorBuilder.getInt32(blockIds[it - basicBlocks.begin()]);
                if (!nextIndex)
                {
                    nextIndex = successorIndex;
                    continue;
                }

                llvm::Value* condition = terminatorBuilder.CreateICmpEQ(
                    indirectBr->getAddress(),
                    llvm::BlockAddress::get(successor)
                );
                nextIndex = terminatorBuilder.CreateSelect(condition, successorIndex, nextIndex);
            }

            if (nextIndex)
                rewriteToDispatcher(indirectBr, nextIndex);
        }

        if (auto* callBr = llvm::dyn_cast<llvm::CallBrInst>(terminator))
        {
            if (callBr->getNumSuccessors() != 1)
                continue;

            llvm::BasicBlock* successor = callBr->getSuccessor(0);
            auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
            if (it == basicBlocks.end())
                continue;

            llvm::IRBuilder<> terminatorBuilder(callBr);
            llvm::Value* nextIndex = terminatorBuilder.getInt32(blockIds[it - basicBlocks.begin()]);
            rewriteToDispatcher(callBr, nextIndex);
        }
    }

    // Verify the function at the end
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] DispatcherPass: Function '" << function->getName() << "' verification failed after transformation!\n";

        // Dump the function IR and terminate
        llvm::errs() << "DispatcherPass: Function IR:\n";
        std::error_code ec;
        llvm::raw_fd_ostream logFile("error_log.txt", ec);
        if (!ec)
        {
            function->print(logFile);
            logFile.close();
            llvm::errs() << "DispatcherPass: Function IR dumped to error_log.txt\n";
        }
        else
        {
            llvm::errs() << "DispatcherPass: Failed to open error_log.txt for writing: " << ec.message() << "\n";
        }
        exit(1);
    }

}
