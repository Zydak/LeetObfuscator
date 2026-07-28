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

llvm::Function *EmitLeetPermutationWithDeps(llvm::Module &M);

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

    //llvm::errs() << "RUNNING IN: " << function->getName() << "\n";

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

    uint32_t tableSize = basicBlocks.size() + 1; // 1 for dispatcher
    
    // Allocate jump table with all the addresses
    llvm::IRBuilder<> entryBeginBuilder(entryBlock, entryBlock->begin());
    llvm::ArrayType* jumpTableType = llvm::ArrayType::get(entryBeginBuilder.getPtrTy(), tableSize);
    llvm::AllocaInst* jumpTable = entryBeginBuilder.CreateAlloca(jumpTableType);
    llvm::ArrayType* permutationTableType = llvm::ArrayType::get(entryBeginBuilder.getInt32Ty(), tableSize);
    llvm::AllocaInst* permutationTable = entryBeginBuilder.CreateAlloca(permutationTableType);

    // Fill the permTable with indices
    llvm::Function* permFunction = module->getFunction("__leet_permutation");

    if (!permFunction)
    {
        permFunction = EmitLeetPermutationWithDeps(*module);
        if (!permFunction)
        {
            llvm::errs() << "Can't find nor emit LLVM permutation func\n";
            exit(1);
        }
    }

    // Populate perm table
    entryBeginBuilder.CreateCall(permFunction, {permutationTable, entryBeginBuilder.getInt32(tableSize)});

    // Create block for the dispatcher and make entry jump to it
    llvm::BasicBlock* dispatcherBlock = llvm::BasicBlock::Create(context, "dispatcher", function);

    for (size_t i = 0; i < basicBlocks.size(); i++)
    {
        // Load index from perm table

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

    // Also store dispatcher block in the table
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

    // Allocate dispatcher state
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
        // Get dispatcher block from the jump table
        llvm::Value* dispatcherBlockGEP = entryEndBuilder.CreateInBoundsGEP(
            jumpTableType,
            jumpTable,
            {entryEndBuilder.getInt32(0), dispatcherBlockIndex}
        );
        llvm::Value* dispatcherBlockAddress = entryEndBuilder.CreateLoad(entryEndBuilder.getPtrTy(), dispatcherBlockGEP, true);

        llvm::IndirectBrInst* entryEndIndirectBr = entryEndBuilder.CreateIndirectBr(dispatcherBlockAddress, 1);
        entryEndIndirectBr->addDestination(dispatcherBlock);
    }

    // Create dispatcher
    llvm::IRBuilder<> dispatcherBuilder(dispatcherBlock);

    // Create an empty barrier function to prevent optimization of the dispatcher block
    // Without the barriers optimizer will just delete it completely and chain the blocks together
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

    // PROPER INDIRECT JUMPS
    //
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
        // Create a barrier function, without it optimizers like to inline this block into other functions
        llvm::Function* barrierFnBlock = llvm::Function::Create(barrierFnType, llvm::GlobalValue::InternalLinkage, "dispatcher_barrier", module);
        barrierFnBlock->addFnAttr(llvm::Attribute::NoDuplicate);
        barrierFnBlock->addFnAttr(llvm::Attribute::Convergent);
        barrierFnBlock->addFnAttr(llvm::Attribute::NoInline);
        barrierFnBlock->addFnAttr(llvm::Attribute::OptimizeNone);

        llvm::BasicBlock* blockBarrierBlock = llvm::BasicBlock::Create(context, "barrier", barrierFnBlock);
        llvm::IRBuilder<> barrierBuilderBlock(blockBarrierBlock);
        barrierBuilderBlock.CreateRetVoid();

        llvm::IRBuilder<> terminatorBuilder(terminator);
        terminatorBuilder.CreateStore(nextIndex, dispatcherState, true);

        // Get dispatcher block from the jump table
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
                    
                llvm::IRBuilder<> terminatorBuilder(branch);
                uint32_t compileTimeIndex = it - basicBlocks.begin();
                llvm::Value* realIndexGEP = terminatorBuilder.CreateInBoundsGEP(
                    permutationTableType,
                    permutationTable,
                    {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndex)}
                );
                llvm::Value* realIndex = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), realIndexGEP);
                
                rewriteToDispatcher(branch, realIndex);
            }
            else if (branch->isConditional())
            {
                llvm::BasicBlock* trueSuccessor = branch->getSuccessor(0);
                llvm::BasicBlock* falseSuccessor = branch->getSuccessor(1);
                auto itTrue = std::find(basicBlocks.begin(), basicBlocks.end(), trueSuccessor);
                auto itFalse = std::find(basicBlocks.begin(), basicBlocks.end(), falseSuccessor);
                if (itTrue == basicBlocks.end() || itFalse == basicBlocks.end())
                    continue;

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
            uint32_t compileTimeIndex = it - basicBlocks.begin();
            llvm::Value* nextIndexGEP = terminatorBuilder.CreateInBoundsGEP(
                permutationTableType,
                permutationTable,
                {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(compileTimeIndex)}
            );
            llvm::Value* nextIndex = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), nextIndexGEP);

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


//
// -------------------------------- HELPER --------------------------------
//
// Emits IR equivalent to:
//
//   uint64_t splitmix64_next(uint64_t *state) {
//       uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
//       z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
//       z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
//       return z ^ (z >> 31);
//   }
//
//   extern "C" void __leet_permutation(uint32_t* table, uint32_t size)
//   {
//       uint64_t state = __rdtsc();
//       for (uint32_t i = 0; i < size; i++)
//           table[i] = i;
//       for (int i = size - 1; i > 0; i--) {
//           uint64_t z = splitmix64_next(&state);
//           int j = z % (i + 1);
//           uint32_t tmp = table[i];
//           table[i] = table[j];
//           table[j] = tmp;
//       }
//   }

// Emits `uint64_t splitmix64_next(uint64_t *state)` into the module and returns it
static llvm::Function* EmitSplitmix64Next(llvm::Module &module)
{
    llvm::LLVMContext& context = module.getContext();
    llvm::IRBuilder<> builder(context);

    llvm::FunctionType* functionType = llvm::FunctionType::get(builder.getInt64Ty(), {builder.getPtrTy()}, false);
    llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, "__leet_split_mix_64", &module);
    function->arg_begin()->setName("state");
    llvm::Argument* statePtr = function->arg_begin();

    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(context, "entry", function);
    builder.SetInsertPoint(entryBlock);

    const uint64_t k1 = 0x9E3779B97F4A7C15ULL;
    const uint64_t k2 = 0xBF58476D1CE4E5B9ULL;
    const uint64_t k3 = 0x94D049BB133111EBULL;

    // z = (*state += k1);
    llvm::Value* oldState = builder.CreateLoad(builder.getInt64Ty(), statePtr, "old");
    llvm::Value* newState = builder.CreateAdd(oldState, builder.getInt64(k1), "z");
    builder.CreateStore(newState, statePtr);

    // z = (z ^ (z >> 30)) * k2;
    llvm::Value* shr1 = builder.CreateLShr(newState, builder.getInt64(30), "shr1");
    llvm::Value* xor1 = builder.CreateXor(newState, shr1, "xor1");
    llvm::Value* mul1 = builder.CreateMul(xor1, builder.getInt64(k2), "mul1");

    // z = (z ^ (z >> 27)) * k3;
    llvm::Value* shr2 = builder.CreateLShr(mul1, builder.getInt64(27), "shr2");
    llvm::Value* xor2 = builder.CreateXor(mul1, shr2, "xor2");
    llvm::Value* mul2 = builder.CreateMul(xor2, builder.getInt64(k3), "mul2");

    // return z ^ (z >> 31);
    llvm::Value* shr3 = builder.CreateLShr(mul2, builder.getInt64(31), "shr3");
    llvm::Value* retVal = builder.CreateXor(mul2, shr3, "xor3");

    builder.CreateRet(retVal);

    llvm::verifyFunction(*function);
    return function;
}

// Emits `void __leet_permutation(uint32_t* table, uint32_t size)` into the module, calling the provided splitmix64_next
llvm::Function* EmitLeetPermutation(llvm::Module &module, llvm::Function* splitmix64Next)
{
    llvm::LLVMContext& context = module.getContext();
    llvm::IRBuilder<> builder(context);

    llvm::FunctionType* functionType = llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy(), builder.getInt32Ty()}, false);
    llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, "__leet_permutation", &module);

    auto argIt = function->arg_begin();
    llvm::Argument* tablePtr = &*argIt++;
    llvm::Argument* size = &*argIt++;
    tablePtr->setName("table");
    size->setName("size");

    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(context, "entry", function);
    llvm::BasicBlock* loop1Cond = llvm::BasicBlock::Create(context, "loop1.cond", function);
    llvm::BasicBlock* loop1Body = llvm::BasicBlock::Create(context, "loop1.body", function);
    llvm::BasicBlock* loop1End = llvm::BasicBlock::Create(context, "loop1.end", function);
    llvm::BasicBlock* loop2Cond = llvm::BasicBlock::Create(context, "loop2.cond", function);
    llvm::BasicBlock* loop2Body = llvm::BasicBlock::Create(context, "loop2.body", function);
    llvm::BasicBlock* loop2End = llvm::BasicBlock::Create(context, "loop2.end", function);

    // entry: state = __rdtsc();
    builder.SetInsertPoint(entryBlock);
    llvm::AllocaInst* stateAlloca = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "state");

    llvm::Function* rdtscIntr = llvm::Intrinsic::getOrInsertDeclaration(&module, llvm::Intrinsic::x86_rdtsc);
    llvm::Value* rdtsc = builder.CreateCall(rdtscIntr, {}, "rdtsc");

    builder.CreateStore(rdtsc, stateAlloca);
    builder.CreateBr(loop1Cond);

    // loop1.cond: for (i = 0; i < size; i++)
    builder.SetInsertPoint(loop1Cond);
    llvm::PHINode* i1 = builder.CreatePHI(builder.getInt32Ty(), 2, "i1");
    i1->addIncoming(builder.getInt32(0), entryBlock);
    llvm::Value* cmp1 = builder.CreateICmpULT(i1, size, "cmp1");
    builder.CreateCondBr(cmp1, loop1Body, loop1End);

    // loop1.body: table[i] = i;
    builder.SetInsertPoint(loop1Body);
    llvm::Value* gep1 = builder.CreateInBoundsGEP(builder.getInt32Ty(), tablePtr, i1, "gep1");
    builder.CreateStore(i1, gep1);
    llvm::Value* i1Next = builder.CreateAdd(i1, builder.getInt32(1), "i1.next");
    builder.CreateBr(loop1Cond);
    i1->addIncoming(i1Next, loop1Body);

    // loop1.end: int i = size - 1;
    builder.SetInsertPoint(loop1End);
    llvm::Value* init2 = builder.CreateSub(size, builder.getInt32(1), "init2");
    builder.CreateBr(loop2Cond);

    // loop2.cond: for (; i > 0; i--)
    builder.SetInsertPoint(loop2Cond);
    llvm::PHINode* i2 = builder.CreatePHI(builder.getInt32Ty(), 2, "i2");
    i2->addIncoming(init2, loop1End);
    llvm::Value* cmp2 = builder.CreateICmpSGT(i2, builder.getInt32(0), "cmp2");
    builder.CreateCondBr(cmp2, loop2Body, loop2End);

    // loop2.body: Fisher-Yates swap
    builder.SetInsertPoint(loop2Body);
    llvm::Value* z = builder.CreateCall(splitmix64Next, {stateAlloca}, "z");

    llvm::Value* iPlus1 = builder.CreateAdd(i2, builder.getInt32(1), "ip1");
    llvm::Value* iPlus164 = builder.CreateSExt(iPlus1, builder.getInt64Ty(), "ip1.64"); // i>0 so i+1 > 0
    llvm::Value* j64 = builder.CreateURem(z, iPlus164, "j64");
    llvm::Value* j = builder.CreateTrunc(j64, builder.getInt32Ty(), "j");

    llvm::Value* gepI = builder.CreateInBoundsGEP(builder.getInt32Ty(), tablePtr, i2, "gepi");
    llvm::Value* tmp = builder.CreateLoad(builder.getInt32Ty(), gepI, "tmp");
    llvm::Value* gepJ = builder.CreateInBoundsGEP(builder.getInt32Ty(), tablePtr, j, "gepj");
    llvm::Value* tableJ = builder.CreateLoad(builder.getInt32Ty(), gepJ, "tablej");

    builder.CreateStore(tableJ, gepI);
    builder.CreateStore(tmp, gepJ);

    llvm::Value* i2Next = builder.CreateSub(i2, builder.getInt32(1), "i2.next");
    builder.CreateBr(loop2Cond);
    i2->addIncoming(i2Next, loop2Body);

    // loop2.end: return;
    builder.SetInsertPoint(loop2End);
    builder.CreateRetVoid();

    llvm::verifyFunction(*function);
    return function;
}

// Convenience entry point: emits both functions and returns __leet_permutation's llvm::Function*
llvm::Function* EmitLeetPermutationWithDeps(llvm::Module &module)
{
    llvm::Function* splitmix64Next = EmitSplitmix64Next(module);
    llvm::Function* permFunc = EmitLeetPermutation(module, splitmix64Next);

    return permFunc;
}