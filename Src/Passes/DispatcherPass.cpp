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

#include "llvm/IR/IntrinsicsX86.h"

#include "PermutationHelper.h"

void RemoveLifetimeIntrinsics(llvm::Function* function)
{
    std::vector<llvm::Instruction*> lifetimeCalls;
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
}

llvm::Function* CreateBarrierFunction(llvm::Module* module, llvm::LLVMContext& context, llvm::FunctionType* barrierFnType, const char* name)
{
    llvm::Function* barrierFn = llvm::Function::Create(barrierFnType, llvm::GlobalValue::InternalLinkage, name, module);
    barrierFn->addFnAttr(llvm::Attribute::NoDuplicate);
    barrierFn->addFnAttr(llvm::Attribute::Convergent);
    barrierFn->addFnAttr(llvm::Attribute::NoInline);
    barrierFn->addFnAttr(llvm::Attribute::OptimizeNone);

    llvm::BasicBlock* barrierBlock = llvm::BasicBlock::Create(context, "barrier", barrierFn);
    llvm::IRBuilder<> barrierBuilder(barrierBlock);
    barrierBuilder.CreateRetVoid();

    return barrierFn;
}

void RewriteTerminatorToDispatcher(llvm::Instruction* terminator, llvm::Value* nextIndex, llvm::AllocaInst* dispatcherState, llvm::ArrayType* jumpTableType, llvm::AllocaInst* jumpTable, llvm::Value* dispatcherBlockIndex, llvm::BasicBlock* dispatcherBlock, llvm::LLVMContext& context, llvm::Module* module, llvm::FunctionType* barrierFnType)
{
    llvm::Function* barrierFnBlock = CreateBarrierFunction(module, context, barrierFnType, "dispatcher_barrier");

    llvm::IRBuilder<> terminatorBuilder(terminator);
    terminatorBuilder.CreateStore(nextIndex, dispatcherState, true);

    llvm::Value* dispatcherBlockGEP = terminatorBuilder.CreateInBoundsGEP(
        jumpTableType,
        jumpTable,
        {terminatorBuilder.getInt32(0), dispatcherBlockIndex}
    );
    llvm::Value* dispatcherBlockAddress = terminatorBuilder.CreateLoad(terminatorBuilder.getPtrTy(), dispatcherBlockGEP, true);

    terminatorBuilder.CreateCall(barrierFnBlock);
    llvm::IndirectBrInst* indirectBr = terminatorBuilder.CreateIndirectBr(dispatcherBlockAddress, 1);
    indirectBr->addDestination(dispatcherBlock);

    terminator->eraseFromParent();
}

void RewriteBranchTerminator(llvm::BranchInst* branch, std::vector<llvm::BasicBlock*>& basicBlocks, llvm::ArrayType* permutationTableType, llvm::AllocaInst* permutationTable, llvm::AllocaInst* dispatcherState, llvm::ArrayType* jumpTableType, llvm::AllocaInst* jumpTable, llvm::Value* dispatcherBlockIndex, llvm::BasicBlock* dispatcherBlock, llvm::LLVMContext& context, llvm::Module* module, llvm::FunctionType* barrierFnType)
{
    if (branch->isUnconditional())
    {
        llvm::BasicBlock* successor = branch->getSuccessor(0);
        auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
        if (it == basicBlocks.end())
            return;

        llvm::IRBuilder<> terminatorBuilder(branch);
        uint32_t compileTimeIndex = it - basicBlocks.begin();
        llvm::Value* realIndexGEP = terminatorBuilder.CreateInBoundsGEP(
            permutationTableType,
            permutationTable,
            {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndex)}
        );
        llvm::Value* realIndex = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), realIndexGEP);

        RewriteTerminatorToDispatcher(branch, realIndex, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
    }
    else if (branch->isConditional())
    {
        llvm::BasicBlock* trueSuccessor = branch->getSuccessor(0);
        llvm::BasicBlock* falseSuccessor = branch->getSuccessor(1);
        auto itTrue = std::find(basicBlocks.begin(), basicBlocks.end(), trueSuccessor);
        auto itFalse = std::find(basicBlocks.begin(), basicBlocks.end(), falseSuccessor);
        if (itTrue == basicBlocks.end() || itFalse == basicBlocks.end())
            return;

        llvm::IRBuilder<> terminatorBuilder(branch);
        uint32_t compileTimeIndexTrue = itTrue - basicBlocks.begin();
        llvm::Value* realIndexTrueGEP = terminatorBuilder.CreateInBoundsGEP(
            permutationTableType,
            permutationTable,
            {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndexTrue)}
        );
        llvm::Value* realIndexTrue = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), realIndexTrueGEP);
        uint32_t compileTimeIndexFalse = itFalse - basicBlocks.begin();
        llvm::Value* realIndexFalseGEP = terminatorBuilder.CreateInBoundsGEP(
            permutationTableType,
            permutationTable,
            {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndexFalse)}
        );
        llvm::Value* realIndexFalse = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), realIndexFalseGEP);

        llvm::Value* nextIndex = terminatorBuilder.CreateSelect(
            branch->getCondition(),
            realIndexTrue,
            realIndexFalse
        );

        RewriteTerminatorToDispatcher(branch, nextIndex, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
    }
}

void RewriteSwitchTerminator(llvm::SwitchInst* switchInst, std::vector<llvm::BasicBlock*>& basicBlocks, llvm::ArrayType* permutationTableType, llvm::AllocaInst* permutationTable, llvm::AllocaInst* dispatcherState, llvm::ArrayType* jumpTableType, llvm::AllocaInst* jumpTable, llvm::Value* dispatcherBlockIndex, llvm::BasicBlock* dispatcherBlock, llvm::LLVMContext& context, llvm::Module* module, llvm::FunctionType* barrierFnType)
{
    llvm::BasicBlock* defaultDest = switchInst->getDefaultDest();
    auto defaultIt = std::find(basicBlocks.begin(), basicBlocks.end(), defaultDest);
    if (defaultIt == basicBlocks.end())
        return;

    llvm::IRBuilder<> terminatorBuilder(switchInst);
    uint32_t compileTimeIndex = defaultIt - basicBlocks.begin();
    llvm::Value* nextIndexGEP = terminatorBuilder.CreateInBoundsGEP(
        permutationTableType,
        permutationTable,
        {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndex)}
    );
    llvm::Value* nextIndex = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), nextIndexGEP);

    for (auto caseIt = switchInst->case_begin(); caseIt != switchInst->case_end(); ++caseIt)
    {
        llvm::BasicBlock* caseSuccessor = caseIt->getCaseSuccessor();
        auto caseSuccessorIt = std::find(basicBlocks.begin(), basicBlocks.end(), caseSuccessor);
        if (caseSuccessorIt == basicBlocks.end())
            continue;

        uint32_t compileTimeIndexCase = caseSuccessorIt - basicBlocks.begin();
        llvm::Value* caseIndexGEP = terminatorBuilder.CreateInBoundsGEP(
            permutationTableType,
            permutationTable,
            {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndexCase)}
        );
        llvm::Value* caseIndex = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), caseIndexGEP);
        llvm::Value* condition = terminatorBuilder.CreateICmpEQ(
            switchInst->getCondition(),
            caseIt->getCaseValue()
        );
        nextIndex = terminatorBuilder.CreateSelect(condition, caseIndex, nextIndex);
    }

    RewriteTerminatorToDispatcher(switchInst, nextIndex, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
}

void RewriteIndirectBrTerminator(llvm::IndirectBrInst* indirectBr, std::vector<llvm::BasicBlock*>& basicBlocks, llvm::ArrayType* permutationTableType, llvm::AllocaInst* permutationTable, llvm::AllocaInst* dispatcherState, llvm::ArrayType* jumpTableType, llvm::AllocaInst* jumpTable, llvm::Value* dispatcherBlockIndex, llvm::BasicBlock* dispatcherBlock, llvm::LLVMContext& context, llvm::Module* module, llvm::FunctionType* barrierFnType)
{
    llvm::IRBuilder<> terminatorBuilder(indirectBr);
    llvm::Value* nextIndex = nullptr;
    for (unsigned i = 0; i < indirectBr->getNumSuccessors(); ++i)
    {
        llvm::BasicBlock* successor = indirectBr->getSuccessor(i);
        auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
        if (it == basicBlocks.end())
            continue;

        uint32_t compileTimeIndex = it - basicBlocks.begin();
        llvm::Value* successorIndexGEP = terminatorBuilder.CreateInBoundsGEP(
            permutationTableType,
            permutationTable,
            {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndex)}
        );
        llvm::Value* successorIndex = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), successorIndexGEP);
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
        RewriteTerminatorToDispatcher(indirectBr, nextIndex, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
}

void RewriteCallBrTerminator(llvm::CallBrInst* callBr, std::vector<llvm::BasicBlock*>& basicBlocks, llvm::ArrayType* permutationTableType, llvm::AllocaInst* permutationTable, llvm::AllocaInst* dispatcherState, llvm::ArrayType* jumpTableType, llvm::AllocaInst* jumpTable, llvm::Value* dispatcherBlockIndex, llvm::BasicBlock* dispatcherBlock, llvm::LLVMContext& context, llvm::Module* module, llvm::FunctionType* barrierFnType)
{
    if (callBr->getNumSuccessors() != 1)
        return;

    llvm::BasicBlock* successor = callBr->getSuccessor(0);
    auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
    if (it == basicBlocks.end())
        return;

    llvm::IRBuilder<> terminatorBuilder(callBr);
    uint32_t compileTimeIndex = it - basicBlocks.begin();
    llvm::Value* nextIndexGEP = terminatorBuilder.CreateInBoundsGEP(
        permutationTableType,
        permutationTable,
        {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndex)}
    );
    llvm::Value* nextIndex = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), nextIndexGEP);

    RewriteTerminatorToDispatcher(callBr, nextIndex, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
}

llvm::PreservedAnalyses LeetObfuscator::DispatcherPass::run(llvm::Module &module, llvm::ModuleAnalysisManager& mam)
{
    llvm::errs() << "Running DispatcherPass\n";

    for (auto& function : module)
    {
        if (function.getName() != "__leet_split_mix_64" && function.getName() != "__leet_permutation")
            CreateDispatcherInAFunction(&function, mam);
    }

    return llvm::PreservedAnalyses::none();
}

void HoistAllocasToEntryBlock(llvm::Function* function)
{
    llvm::BasicBlock& entryBlock = function->getEntryBlock();
    auto insertPt = entryBlock.getFirstInsertionPt();

    std::vector<llvm::AllocaInst*> toMove;
    for (auto& block : *function)
    {
        for (auto& inst : block)
            if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(&inst))
                toMove.push_back(alloca);
    }

    for (auto* alloca : toMove)
        alloca->moveBefore(insertPt);
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
    // them from existence
    RemoveLifetimeIntrinsics(function);

    // The code will technically be valid but the verifier will still complain about uses before initialization
    // so demote everything to stack first. You could call MemToRegPass after everything is done but it will
    // generate insane amount of instructions because blocks will be jumping between demoting to memory and promoting
    // to stack on every entry, if you have a lot of blocks it's just bloat that doesn't do anything. So I just
    // do RegToMem without promoting it back afterwards
    auto &fam = mam.getResult<llvm::FunctionAnalysisManagerModuleProxy>(*module).getManager();
    llvm::RegToMemPass().run(*function, fam);

    HoistAllocasToEntryBlock(function);

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

    // split the entry block so the dispatcher owns the flow
    llvm::BasicBlock* entryBlock = &function->getEntryBlock();
    llvm::Instruction* firstNonAlloca = llvm::dyn_cast<llvm::Instruction>(entryBlock->getFirstNonPHIOrDbgOrAlloca());
    if (!firstNonAlloca)
    {
        llvm::errs() << "ERROR: Function '" << function->getName() << "' has no non-alloca instructions in the entry block, skipping.\n";
        return;
    }
    llvm::BasicBlock* bodyBlock = entryBlock->splitBasicBlock(firstNonAlloca);
    basicBlocks.insert(basicBlocks.begin(), bodyBlock);

    entryBlock->setName("leet.entry.block");

    // shuffle the blocks so the table is less obvious
    std::mt19937 rng(std::random_device{}());
    std::shuffle(basicBlocks.begin(), basicBlocks.end(), rng);
    auto it = std::find(basicBlocks.begin(), basicBlocks.end(), bodyBlock);
    uint32_t bodyIndex = it - basicBlocks.begin();

    uint32_t tableSize = basicBlocks.size() + 1; // 1 for dispatcher

    // allocate jump table and perm table
    llvm::IRBuilder<> entryBeginBuilder(entryBlock, entryBlock->begin());
    llvm::ArrayType* jumpTableType = llvm::ArrayType::get(entryBeginBuilder.getPtrTy(), tableSize);
    llvm::AllocaInst* jumpTable = entryBeginBuilder.CreateAlloca(jumpTableType);
    llvm::ArrayType* permutationTableType = llvm::ArrayType::get(entryBeginBuilder.getInt32Ty(), tableSize);
    llvm::AllocaInst* permutationTable = entryBeginBuilder.CreateAlloca(permutationTableType);

    llvm::Function* permFunction = GetOrEmitLeetPermutationWithDeps(*module);

    if (!permFunction)
    {
        llvm::errs() << "Can't find nor emit LLVM permutation func\n";
        exit(1);
    }

    entryBeginBuilder.CreateCall(permFunction, {permutationTable, entryBeginBuilder.getInt32(tableSize)});

    llvm::BasicBlock* dispatcherBlock = llvm::BasicBlock::Create(context, "leet.dispatcher.block", function);

    for (size_t i = 0; i < basicBlocks.size(); i++)
    {
        llvm::Value* blockIndexGEP = entryBeginBuilder.CreateInBoundsGEP(
            permutationTableType,
            permutationTable,
            {entryBeginBuilder.getInt32(0), entryBeginBuilder.getInt32(i)}
        );
        llvm::Value* blockIndex = entryBeginBuilder.CreateLoad(entryBeginBuilder.getInt32Ty(), blockIndexGEP);

        llvm::Value* blockAddress = entryBeginBuilder.CreateInBoundsGEP(
            jumpTableType,
            jumpTable,
            {entryBeginBuilder.getInt32(0), blockIndex}
        );

        entryBeginBuilder.CreateStore(llvm::BlockAddress::get(basicBlocks[i]), blockAddress);
    }

    llvm::Value* dispatcherBlockIndexGEP = entryBeginBuilder.CreateInBoundsGEP(
        permutationTableType,
        permutationTable,
        {entryBeginBuilder.getInt32(0), entryBeginBuilder.getInt32(tableSize - 1)} // Dispatcher is always last
    );
    llvm::Value* dispatcherBlockIndex = entryBeginBuilder.CreateLoad(entryBeginBuilder.getInt32Ty(), dispatcherBlockIndexGEP);
    llvm::Value* dispatcherBlockAddress = entryBeginBuilder.CreateInBoundsGEP(
        jumpTableType,
        jumpTable,
        {entryBeginBuilder.getInt32(0), dispatcherBlockIndex}
    );
    entryBeginBuilder.CreateStore(llvm::BlockAddress::get(dispatcherBlock), dispatcherBlockAddress);

    llvm::AllocaInst* dispatcherState = entryBeginBuilder.CreateAlloca(entryBeginBuilder.getInt32Ty());
    entryBlock->getTerminator()->eraseFromParent();
    llvm::IRBuilder<> entryEndBuilder(entryBlock, entryBlock->end());
    llvm::Value* bodyBlockIndexGEP = entryEndBuilder.CreateInBoundsGEP(
        permutationTableType,
        permutationTable,
        {entryEndBuilder.getInt32(0), entryEndBuilder.getInt32(bodyIndex)}
    );
    llvm::Value* bodyBlockIndex = entryEndBuilder.CreateLoad(entryEndBuilder.getInt32Ty(), bodyBlockIndexGEP);
    entryEndBuilder.CreateStore(bodyBlockIndex, dispatcherState);

    {
        llvm::Value* dispatcherBlockGEP = entryEndBuilder.CreateInBoundsGEP(
            jumpTableType,
            jumpTable,
            {entryEndBuilder.getInt32(0), dispatcherBlockIndex}
        );
        llvm::Value* dispatcherBlockAddress = entryEndBuilder.CreateLoad(entryEndBuilder.getPtrTy(), dispatcherBlockGEP, true);

        llvm::IndirectBrInst* entryEndIndirectBr = entryEndBuilder.CreateIndirectBr(dispatcherBlockAddress, 1);
        entryEndIndirectBr->addDestination(dispatcherBlock);
    }

    llvm::IRBuilder<> dispatcherBuilder(dispatcherBlock);
    llvm::FunctionType* barrierFnType = llvm::FunctionType::get(dispatcherBuilder.getVoidTy(), false);
    llvm::Function* barrierFn = CreateBarrierFunction(module, context, barrierFnType, "dispatcher_barrier");

    dispatcherBuilder.CreateCall(barrierFn);

    llvm::Value* dispatcherStateLoad = dispatcherBuilder.CreateLoad(dispatcherBuilder.getInt32Ty(), dispatcherState, true);
    llvm::Value* nextBlockGEP = dispatcherBuilder.CreateInBoundsGEP(
        jumpTableType,
        jumpTable,
        {dispatcherBuilder.getInt32(0), dispatcherStateLoad}
    );
    llvm::Value* nextBlockAddress = dispatcherBuilder.CreateLoad(dispatcherBuilder.getPtrTy(), nextBlockGEP, true);
    llvm::IndirectBrInst* indirectBr = dispatcherBuilder.CreateIndirectBr(nextBlockAddress, basicBlocks.size());
    for (auto* basicBlock : basicBlocks)
    {
        indirectBr->addDestination(basicBlock);
    }

    for (auto* basicBlock : basicBlocks)
    {
        llvm::Instruction* terminator = basicBlock->getTerminator();
        if (!terminator)
            continue;

        if (auto* branch = llvm::dyn_cast<llvm::BranchInst>(terminator))
        {
            RewriteBranchTerminator(branch, basicBlocks, permutationTableType, permutationTable, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
            continue;
        }

        if (auto* switchInst = llvm::dyn_cast<llvm::SwitchInst>(terminator))
        {
            RewriteSwitchTerminator(switchInst, basicBlocks, permutationTableType, permutationTable, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
        }

        if (auto* indirectBr = llvm::dyn_cast<llvm::IndirectBrInst>(terminator))
        {
            RewriteIndirectBrTerminator(indirectBr, basicBlocks, permutationTableType, permutationTable, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
        }

        if (auto* callBr = llvm::dyn_cast<llvm::CallBrInst>(terminator))
        {
            RewriteCallBrTerminator(callBr, basicBlocks, permutationTableType, permutationTable, dispatcherState, jumpTableType, jumpTable, dispatcherBlockIndex, dispatcherBlock, context, module, barrierFnType);
        }
    }

    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] DispatcherPass: Function '" << function->getName() << "' verification failed after transformation!\n";

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
