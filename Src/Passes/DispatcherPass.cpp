#include "DispatcherPass.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <iterator>
#include "SettingsParser.h"
#include "RandomNumberGenerator.h"

#include "llvm/IR/InlineAsm.h"

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

void DemoteCrossBlockInstructions(llvm::Function* function)
{
    llvm::BasicBlock& entryBlock = function->getEntryBlock();
    llvm::BasicBlock::iterator allocaInsertionPoint = entryBlock.begin();
    const llvm::DataLayout& dataLayout = function->getParent()->getDataLayout();

    std::vector<llvm::PHINode*> phiNodes;
    for (llvm::BasicBlock& basicBlock : *function)
    {
        for (llvm::PHINode& phiNode : basicBlock.phis())
        {
            phiNodes.push_back(&phiNode);
        }
    }
    for (llvm::PHINode* phiNode : phiNodes)
    {
        llvm::DemotePHIToStack(phiNode, allocaInsertionPoint);
    }

    std::vector<llvm::Instruction*> crossBlockInstructions;
    for (llvm::BasicBlock& basicBlock : *function)
    {
        for (llvm::Instruction& instruction : basicBlock)
        {
            if (llvm::isa<llvm::AllocaInst>(&instruction))
                continue;

            if (!instruction.getType()->isSized())
                continue;

            bool escapes = false;
            for (llvm::User* user : instruction.users())
            {
                if (llvm::Instruction* userInstruction = llvm::dyn_cast<llvm::Instruction>(user))
                {
                    if (userInstruction->getParent() != &basicBlock)
                    {
                        escapes = true;
                        break;
                    }
                }
            }

            if (escapes)
            {
                crossBlockInstructions.push_back(&instruction);
            }
        }
    }

    for (llvm::Instruction* instruction : crossBlockInstructions)
    {
        llvm::BasicBlock* parentBlock = instruction->getParent();
        llvm::AllocaInst* stackSlot = new llvm::AllocaInst(
            instruction->getType(),
            dataLayout.getAllocaAddrSpace(),
            nullptr,
            instruction->getName() + ".cross_block",
            allocaInsertionPoint
        );

        if (!instruction->isTerminator())
        {
            llvm::BasicBlock::iterator insertionPoint = ++instruction->getIterator();
            while (insertionPoint != parentBlock->end() && (llvm::isa<llvm::PHINode>(insertionPoint) || insertionPoint->isEHPad()))
            {
                insertionPoint++;
            }
            if (insertionPoint != parentBlock->end() && llvm::isa<llvm::CatchSwitchInst>(insertionPoint))
            {
                for (llvm::BasicBlock* handler : llvm::successors(&*insertionPoint))
                {
                    new llvm::StoreInst(instruction, stackSlot, handler->getFirstInsertionPt());
                }
            }
            else
            {
                new llvm::StoreInst(instruction, stackSlot, insertionPoint);
            }
        }
        else if (llvm::InvokeInst* invokeInstruction = llvm::dyn_cast<llvm::InvokeInst>(instruction))
        {
            new llvm::StoreInst(instruction, stackSlot, invokeInstruction->getNormalDest()->getFirstInsertionPt());
        }
        else if (llvm::CallBrInst* callBrInstruction = llvm::dyn_cast<llvm::CallBrInst>(instruction))
        {
            for (llvm::BasicBlock* successor : llvm::successors(callBrInstruction))
            {
                new llvm::StoreInst(callBrInstruction, stackSlot, successor->getFirstInsertionPt());
            }
        }

        std::vector<std::pair<llvm::Instruction*, unsigned>> usesToReplace;
        for (llvm::Use& use : instruction->uses())
        {
            if (llvm::Instruction* userInstruction = llvm::dyn_cast<llvm::Instruction>(use.getUser()))
            {
                if (userInstruction->getParent() != parentBlock)
                {
                    usesToReplace.push_back({userInstruction, use.getOperandNo()});
                }
            }
        }

        for (auto& pair : usesToReplace)
        {
            llvm::Instruction* userInstruction = pair.first;
            unsigned operandIndex = pair.second;
            llvm::LoadInst* reloadInstruction = new llvm::LoadInst(
                instruction->getType(),
                stackSlot,
                instruction->getName() + ".reload",
                false,
                userInstruction->getIterator()
            );
            userInstruction->setOperand(operandIndex, reloadInstruction);
        }
    }
}

void RewriteTerminatorToDispatcher(
    llvm::Instruction* terminator,
    llvm::Value* nextIndex,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    uint32_t dispatcherIndex,
    llvm::BasicBlock* dispatcherBlock
)
{
    llvm::IRBuilder<> terminatorBuilder(terminator);
    terminatorBuilder.CreateStore(nextIndex, dispatcherState, true);

    llvm::InlineAsm* asmBarrier = llvm::InlineAsm::get(
        llvm::FunctionType::get(terminatorBuilder.getVoidTy(), false),
        "",
        "~{memory}",
        true
    );

    terminatorBuilder.CreateCall(asmBarrier);

    llvm::Type* intPtrType = jumpTable->getParent()->getDataLayout().getIntPtrType(jumpTable->getContext());
    llvm::Value* dispatcherOffsetGEP = terminatorBuilder.CreateInBoundsGEP(
        jumpTable->getValueType(),
        jumpTable,
        {terminatorBuilder.getInt32(0), terminatorBuilder.getInt32(dispatcherIndex)}
    );
    llvm::Value* dispatcherOffset32 = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), dispatcherOffsetGEP, true);
    llvm::Value* dispatcherOffset = terminatorBuilder.CreateSExt(dispatcherOffset32, intPtrType);
    llvm::Value* tableAddress = terminatorBuilder.CreatePtrToInt(jumpTable, intPtrType);
    llvm::Value* dispatcherAddressInt = terminatorBuilder.CreateAdd(tableAddress, dispatcherOffset);
    llvm::Value* dispatcherAddress = terminatorBuilder.CreateIntToPtr(dispatcherAddressInt, terminatorBuilder.getPtrTy());

    llvm::IndirectBrInst* indirectBranchInstruction = terminatorBuilder.CreateIndirectBr(dispatcherAddress, 1);
    indirectBranchInstruction->addDestination(dispatcherBlock);

    terminator->eraseFromParent();
}

void RewriteBranchTerminator(
    llvm::BranchInst* branch,
    std::vector<llvm::BasicBlock*>& basicBlocks,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    uint32_t dispatcherIndex,
    llvm::BasicBlock* dispatcherBlock
)
{
    if (branch->isUnconditional())
    {
        llvm::BasicBlock* successor = branch->getSuccessor(0);
        auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
        if (it == basicBlocks.end())
        {
            return;
        }

        llvm::IRBuilder<> terminatorBuilder(branch);
        uint32_t compileTimeIndex = (uint32_t)std::distance(basicBlocks.begin(), it);
        llvm::Value* nextIndex = terminatorBuilder.getInt32(compileTimeIndex);

        RewriteTerminatorToDispatcher(branch, nextIndex, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
    }
    else if (branch->isConditional())
    {
        llvm::BasicBlock* trueSuccessor = branch->getSuccessor(0);
        llvm::BasicBlock* falseSuccessor = branch->getSuccessor(1);
        auto itTrue = std::find(basicBlocks.begin(), basicBlocks.end(), trueSuccessor);
        auto itFalse = std::find(basicBlocks.begin(), basicBlocks.end(), falseSuccessor);
        if (itTrue == basicBlocks.end() || itFalse == basicBlocks.end())
        {
            return;
        }

        llvm::IRBuilder<> terminatorBuilder(branch);
        uint32_t compileTimeIndexTrue = (uint32_t)std::distance(basicBlocks.begin(), itTrue);
        uint32_t compileTimeIndexFalse = (uint32_t)std::distance(basicBlocks.begin(), itFalse);

        llvm::Value* nextIndex = terminatorBuilder.CreateSelect(
            branch->getCondition(),
            terminatorBuilder.getInt32(compileTimeIndexTrue),
            terminatorBuilder.getInt32(compileTimeIndexFalse)
        );

        RewriteTerminatorToDispatcher(branch, nextIndex, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
    }
}

void RewriteSwitchTerminator(
    llvm::SwitchInst* switchInstruction,
    std::vector<llvm::BasicBlock*>& basicBlocks,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    uint32_t dispatcherIndex,
    llvm::BasicBlock* dispatcherBlock
)
{
    llvm::BasicBlock* defaultDestination = switchInstruction->getDefaultDest();
    auto defaultIterator = std::find(basicBlocks.begin(), basicBlocks.end(), defaultDestination);
    if (defaultIterator == basicBlocks.end())
    {
        return;
    }

    llvm::IRBuilder<> terminatorBuilder(switchInstruction);
    uint32_t defaultCompileTimeIndex = (uint32_t)std::distance(basicBlocks.begin(), defaultIterator);
    llvm::Value* nextIndex = terminatorBuilder.getInt32(defaultCompileTimeIndex);

    for (auto caseIterator = switchInstruction->case_begin(); caseIterator != switchInstruction->case_end(); caseIterator++)
    {
        llvm::BasicBlock* caseSuccessor = caseIterator->getCaseSuccessor();
        auto caseSuccessorIterator = std::find(basicBlocks.begin(), basicBlocks.end(), caseSuccessor);
        if (caseSuccessorIterator == basicBlocks.end())
        {
            continue;
        }

        uint32_t compileTimeIndexCase = (uint32_t)std::distance(basicBlocks.begin(), caseSuccessorIterator);
        llvm::Value* caseIndex = terminatorBuilder.getInt32(compileTimeIndexCase);
        llvm::Value* condition = terminatorBuilder.CreateICmpEQ(
            switchInstruction->getCondition(),
            caseIterator->getCaseValue()
        );
        nextIndex = terminatorBuilder.CreateSelect(condition, caseIndex, nextIndex);
    }

    RewriteTerminatorToDispatcher(switchInstruction, nextIndex, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
}

void RewriteIndirectBrTerminator(
    llvm::IndirectBrInst* indirectBranch,
    std::vector<llvm::BasicBlock*>& basicBlocks,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    uint32_t dispatcherIndex,
    llvm::BasicBlock* dispatcherBlock
)
{
    llvm::IRBuilder<> terminatorBuilder(indirectBranch);
    llvm::Value* nextIndex = nullptr;
    for (unsigned int i = 0; i < indirectBranch->getNumSuccessors(); i++)
    {
        llvm::BasicBlock* successor = indirectBranch->getSuccessor(i);
        auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
        if (it == basicBlocks.end())
        {
            continue;
        }

        uint32_t compileTimeIndex = (uint32_t)std::distance(basicBlocks.begin(), it);
        llvm::Value* successorIndex = terminatorBuilder.getInt32(compileTimeIndex);
        if (!nextIndex)
        {
            nextIndex = successorIndex;
            continue;
        }

        llvm::Value* condition = terminatorBuilder.CreateICmpEQ(
            indirectBranch->getAddress(),
            llvm::BlockAddress::get(successor)
        );
        nextIndex = terminatorBuilder.CreateSelect(condition, successorIndex, nextIndex);
    }

    if (nextIndex)
    {
        RewriteTerminatorToDispatcher(indirectBranch, nextIndex, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
    }
}

void RewriteCallBrTerminator(
    llvm::CallBrInst* callBranch,
    std::vector<llvm::BasicBlock*>& basicBlocks,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    uint32_t dispatcherIndex,
    llvm::BasicBlock* dispatcherBlock
)
{
    if (callBranch->getNumSuccessors() != 1)
    {
        return;
    }

    llvm::BasicBlock* successor = callBranch->getSuccessor(0);
    auto it = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
    if (it == basicBlocks.end())
    {
        return;
    }

    llvm::IRBuilder<> terminatorBuilder(callBranch);
    uint32_t compileTimeIndex = (uint32_t)std::distance(basicBlocks.begin(), it);
    llvm::Value* nextIndex = terminatorBuilder.getInt32(compileTimeIndex);

    RewriteTerminatorToDispatcher(callBranch, nextIndex, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
}

llvm::PreservedAnalyses LeetObfuscator::DispatcherPass::run(llvm::Module &module, llvm::ModuleAnalysisManager& mam)
{
    llvm::errs() << "Running DispatcherPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    std::vector<llvm::Function*> functions;

    for (auto& function : module)
    {
        functions.push_back(&function);
    }

    
    for (auto& function : functions)
    {
        CreateDispatcherInAFunction(function);
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

void LeetObfuscator::DispatcherPass::CreateDispatcherInAFunction(llvm::Function* function)
{
    // Skip functions with exception handling
    if (function->hasPersonalityFn())
    {
        llvm::errs() << "Function '" << function->getName() << "' has a personality function, skipping dispatcher creation.\n";
        return;
    }

    // Very special case, this function is used in the exception handler for function address lookup, no calls can be created there
    // and this will obviously insert split_mix64
    if (function->getName().contains("_ZNSt8__detail9_Map_baseIjSt4pairIKjPvESaIS4_ENS_10_Select1stESt8equal_toIjESt4hashIjENS_18_Mod_range_hashingENS_20_Default_ranged_hashENS_20_Prime_"))
    {
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

    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(*function, SettingsParser::PassType::DispatcherPass, m_Arguments);
    
    if (SettingsParser::ShouldSkipFunction(function, attributes))
    {
        m_Logger.LogFunction(*function, "Skipping function due to settings", 1);
        return;
    }

    m_Logger.LogFunction(*function, "Processing function", 1);

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    if (generator->DrawRange(1u, 100u) > attributes.dispatcherProbability)
    {
        m_Logger.LogFunction(*function, "Skipping dispatcher insertion due to probability", 2);
        return;
    }

    m_Logger.LogFunction(*function, "Creating dispatcher in function", 2);

    llvm::Module* module = function->getParent();

    // Lifetime intrinsics require their pointer operand to trace directly to an
    // alloca. Once we demote across the dispatcher's shuffled blocks, that
    // breaks since the DemoteCrossBlockInstructions spills the alloca pointer itself into another
    // slot. And since it's an llvm instrisic used only for optimization, just purge
    // them from existence
    RemoveLifetimeIntrinsics(function);

    DemoteCrossBlockInstructions(function);

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

    llvm::IRBuilder<> entryBeginBuilder(entryBlock, entryBlock->begin());
    llvm::AllocaInst* dispatcherState = entryBeginBuilder.CreateAlloca(entryBeginBuilder.getInt32Ty());

    llvm::BasicBlock* dispatcherBlock = llvm::BasicBlock::Create(context, "leet.dispatcher.block", function);
    basicBlocks.push_back(dispatcherBlock);
    uint32_t dispatcherIndex = (uint32_t)(basicBlocks.size() - 1);

    llvm::Type* int32Type = llvm::Type::getInt32Ty(context);
    llvm::Type* intPtrType = function->getParent()->getDataLayout().getIntPtrType(context);
    llvm::ArrayType* jumpTableType = llvm::ArrayType::get(int32Type, basicBlocks.size());
    std::string tableName = "leetJumpTable." + function->getName().str();
    llvm::GlobalVariable* jumpTable = new llvm::GlobalVariable(
        *function->getParent(),
        jumpTableType,
        false,
        llvm::GlobalValue::InternalLinkage,
        nullptr,
        tableName
    );

    if (function->hasComdat())
    {
        jumpTable->setComdat(function->getComdat());
        jumpTable->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);
        jumpTable->setVisibility(llvm::GlobalValue::HiddenVisibility);
    }

    std::vector<llvm::Constant*> blockAddresses;
    for (llvm::BasicBlock* basicBlock : basicBlocks)
    {
        llvm::Constant* blockAddress = llvm::ConstantExpr::getPtrToInt(llvm::BlockAddress::get(basicBlock), intPtrType);
        llvm::Constant* tableAddress = llvm::ConstantExpr::getPtrToInt(jumpTable, intPtrType);
        llvm::Constant* relativeOffset64 = llvm::ConstantExpr::getSub(blockAddress, tableAddress);
        llvm::Constant* relativeOffset32 = llvm::ConstantExpr::getTrunc(relativeOffset64, int32Type);

        blockAddresses.push_back(relativeOffset32);
    }
    llvm::Constant* initializer = llvm::ConstantArray::get(jumpTableType, blockAddresses);
    jumpTable->setInitializer(initializer);

    RewriteTerminatorToDispatcher(
        entryBlock->getTerminator(),
        entryBeginBuilder.getInt32(0),
        dispatcherState,
        jumpTable,
        dispatcherIndex,
        dispatcherBlock
    );

    llvm::IRBuilder<> dispatcherBuilder(dispatcherBlock);

    llvm::InlineAsm* asmBarrier = llvm::InlineAsm::get(
        llvm::FunctionType::get(dispatcherBuilder.getVoidTy(), false),
        "",
        "~{memory}",
        true
    );

    dispatcherBuilder.CreateCall(asmBarrier);

    llvm::Value* dispatcherStateLoad = dispatcherBuilder.CreateLoad(dispatcherBuilder.getInt32Ty(), dispatcherState, true);
    llvm::Value* relativeOffsetGEP = dispatcherBuilder.CreateInBoundsGEP(
        jumpTable->getValueType(),
        jumpTable,
        {dispatcherBuilder.getInt32(0), dispatcherStateLoad}
    );
    llvm::Value* relativeOffset32 = dispatcherBuilder.CreateLoad(int32Type, relativeOffsetGEP, true);
    llvm::Value* relativeOffset = dispatcherBuilder.CreateSExt(relativeOffset32, intPtrType);
    llvm::Value* tableAddress = dispatcherBuilder.CreatePtrToInt(jumpTable, intPtrType);
    llvm::Value* targetAddressInt = dispatcherBuilder.CreateAdd(tableAddress, relativeOffset);
    llvm::Value* nextBlockAddress = dispatcherBuilder.CreateIntToPtr(targetAddressInt, dispatcherBuilder.getPtrTy());

    llvm::IndirectBrInst* indirectBranchInstruction = dispatcherBuilder.CreateIndirectBr(nextBlockAddress, basicBlocks.size());
    for (llvm::BasicBlock* basicBlock : basicBlocks)
    {
        indirectBranchInstruction->addDestination(basicBlock);
    }

    for (llvm::BasicBlock* basicBlock : basicBlocks)
    {
        if (basicBlock == dispatcherBlock)
        {
            continue;
        }

        llvm::Instruction* terminator = basicBlock->getTerminator();
        if (!terminator)
        {
            continue;
        }

        if (auto* branch = llvm::dyn_cast<llvm::BranchInst>(terminator))
        {
            RewriteBranchTerminator(branch, basicBlocks, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
            continue;
        }

        if (auto* switchInstruction = llvm::dyn_cast<llvm::SwitchInst>(terminator))
        {
            RewriteSwitchTerminator(switchInstruction, basicBlocks, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
            continue;
        }

        if (auto* indirectBranch = llvm::dyn_cast<llvm::IndirectBrInst>(terminator))
        {
            RewriteIndirectBrTerminator(indirectBranch, basicBlocks, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
            continue;
        }

        if (auto* callBranch = llvm::dyn_cast<llvm::CallBrInst>(terminator))
        {
            RewriteCallBrTerminator(callBranch, basicBlocks, dispatcherState, jumpTable, dispatcherIndex, dispatcherBlock);
            continue;
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
