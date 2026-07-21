#include "DispatcherPass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Scalar/Reg2Mem.h"
#include "llvm/IR/Verifier.h"

llvm::PreservedAnalyses LeetObfuscator::DispatcherPass::run(llvm::Module &module, llvm::ModuleAnalysisManager& mam)
{
    for (auto& function : module)
    {
        CreateDispatcherInAFunction(&function, mam);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::DispatcherPass::CreateDispatcherInAFunction(llvm::Function *function, llvm::ModuleAnalysisManager& mam)
{
    llvm::Module* module = function->getParent();
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
    
    // The code will technically be valid but the verifier will still complain about uses before initialization
    // so demote everything to stack first. You could call MemToRegPass after everything is done but it will
    // generate insane amount of instructions because blocks will be jumping between demoting to memory and promoting
    // to stack on every entry, if you have a lot of blocks it's just bloat that doesn't do anything. So I just
    // do RegToMem without promoting it back afterwards
    auto &fam = mam.getResult<llvm::FunctionAnalysisManagerModuleProxy>(*module).getManager();
    llvm::RegToMemPass().run(*function, fam);

    llvm::errs() << "DISPATCHER IN FUNCTION: " << function->getName() << "\n";

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
    
    // Allocate jump table with all the addresses
    llvm::IRBuilder<> entryBeginBuilder(entryBlock, entryBlock->begin());
    llvm::ArrayType* jumpTableType = llvm::ArrayType::get(entryBeginBuilder.getPtrTy(), basicBlocks.size());
    llvm::AllocaInst* jumpTable = entryBeginBuilder.CreateAlloca(jumpTableType);
    for (size_t i = 0; i < basicBlocks.size(); i++)
    {
        llvm::Value* blockAddress = entryBeginBuilder.CreateInBoundsGEP(
            jumpTableType,
            jumpTable,
            {entryBeginBuilder.getInt32(0), entryBeginBuilder.getInt32(i)}
        );

        entryBeginBuilder.CreateStore(llvm::BlockAddress::get(basicBlocks[i]), blockAddress);
    }

    // Allocate dispatcher state
    llvm::AllocaInst* dispatcherState = entryBeginBuilder.CreateAlloca(entryBeginBuilder.getInt32Ty());

    // Create block for the dispatcher and make entry jump to it
    llvm::BasicBlock* dispatcherBlock = llvm::BasicBlock::Create(context, "dispatcher", function);
    entryBlock->getTerminator()->eraseFromParent();
    llvm::IRBuilder<> entryEndBuilder(entryBlock, entryBlock->end());
    entryEndBuilder.CreateStore(entryEndBuilder.getInt32(0), dispatcherState);
    entryEndBuilder.CreateBr(dispatcherBlock);

    // Create dispatcher
    llvm::IRBuilder<> dispatcherBuilder(dispatcherBlock);
    llvm::Value* dispatcherStateLoad = dispatcherBuilder.CreateLoad(dispatcherBuilder.getInt32Ty(), dispatcherState);

    // PROPER INDIRECT JUMPS
    //
    llvm::Value* nextBlockGEP = dispatcherBuilder.CreateInBoundsGEP(
        jumpTableType,
        jumpTable,
        {dispatcherBuilder.getInt32(0), dispatcherStateLoad}
    );
    llvm::Value* nextBlockAddress = dispatcherBuilder.CreateLoad(dispatcherBuilder.getPtrTy(), nextBlockGEP);
    llvm::IndirectBrInst* indirectBr = dispatcherBuilder.CreateIndirectBr(nextBlockAddress, basicBlocks.size());
    for (auto* basicBlock : basicBlocks)
    {
        indirectBr->addDestination(basicBlock);
    }

    // TEST SWITCH STATEMENT FOR DEBUGGING OUTPUT
    //
    // llvm::BasicBlock* trapBlock = llvm::BasicBlock::Create(context, "dispatcher.trap", function);
    // llvm::IRBuilder<>(trapBlock).CreateUnreachable();
    // llvm::SwitchInst* sw = dispatcherBuilder.CreateSwitch(dispatcherStateLoad, trapBlock, basicBlocks.size());
    // for (size_t i = 0; i < basicBlocks.size(); i++)
    //     sw->addCase(dispatcherBuilder.getInt32(i), basicBlocks[i]);

    // Rewrite every blocks' terminator to change dispatcher state to the next block and jump back to dispatcher block
    for (auto* basicBlock : basicBlocks)
    {
        llvm::BranchInst* originalTerminator = llvm::dyn_cast<llvm::BranchInst>(basicBlock->getTerminator());

        // if the block has no branch terminator it means it's the last block in the function
        // so leave it alone and let it return from the function
        if (!originalTerminator)
            continue;

        if (originalTerminator->isUnconditional())
        {
            llvm::BasicBlock* successor = originalTerminator->getSuccessor(0);
            auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
            if (it == basicBlocks.end())
            {
                llvm::errs() << "Couldn't find basic block successor? Should never happen.\n";
                return;
            }

            uint32_t successorIndex = it - basicBlocks.begin();
            llvm::IRBuilder<> terminatorBuilder(originalTerminator);
            terminatorBuilder.CreateStore(terminatorBuilder.getInt32(successorIndex), dispatcherState);
            terminatorBuilder.CreateBr(dispatcherBlock);
            originalTerminator->eraseFromParent();
        }
        if (originalTerminator->isConditional())
        {
            llvm::BasicBlock* trueSuccessor = originalTerminator->getSuccessor(0);
            llvm::BasicBlock* falseSuccessor = originalTerminator->getSuccessor(1);
            auto itTrue = std::find(basicBlocks.begin(), basicBlocks.end(), trueSuccessor);
            auto itFalse = std::find(basicBlocks.begin(), basicBlocks.end(), falseSuccessor);
            if (itTrue == basicBlocks.end() || itFalse == basicBlocks.end())
            {
                llvm::errs() << "Couldn't find basic block successor? Should never happen.\n";
                return;
            }

            uint32_t trueSuccessorIndex = itTrue - basicBlocks.begin();
            uint32_t falseSuccessorIndex = itFalse - basicBlocks.begin();
            llvm::IRBuilder<> terminatorBuilder(originalTerminator);
            llvm::Value* nextIndex = terminatorBuilder.CreateSelect(
                originalTerminator->getCondition(),
                terminatorBuilder.getInt32(trueSuccessorIndex),
                terminatorBuilder.getInt32(falseSuccessorIndex)
            );
            terminatorBuilder.CreateStore(nextIndex, dispatcherState);
            terminatorBuilder.CreateBr(dispatcherBlock);
            originalTerminator->eraseFromParent();
        }
        // TODO: possibly handle SwitchInst here?
    }

    // Verify the function at the end
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        llvm::errs() << "DispatcherPass: Function '" << function->getName() << "' verification failed after transformation!\n";

        // Dump the function IR and terminate
        llvm::errs() << "DispatcherPass: Function IR:\n";
        llvm::errs() << *function << "\n";
        exit(1);
    }

}
