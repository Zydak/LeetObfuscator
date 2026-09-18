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

struct DispatcherMapping
{
    uint32_t tableSize = 0;
    uint32_t tableMask = 0;
    uint32_t multiplierA = 0;
    uint32_t inverseMultiplierA = 0;
    uint32_t addendB = 0;
    uint32_t xorKey = 0;
    bool useMBA = true;
    bool stateHardening = true;
    std::vector<uint32_t> dispatcherSlots;
    std::vector<std::vector<uint32_t>> blockSlots;
};

static uint32_t ComputeModularInverse32(uint32_t value)
{
    uint32_t inverse = value;
    inverse = inverse * (2u - value * inverse);
    inverse = inverse * (2u - value * inverse);
    inverse = inverse * (2u - value * inverse);
    inverse = inverse * (2u - value * inverse);
    return inverse;
}

static uint32_t GenerateStateForSlot(
    uint32_t targetSlot,
    const DispatcherMapping& mapping,
    LeetObfuscator::RandomNumberGenerator& generator
)
{
    uint32_t targetLower = (mapping.inverseMultiplierA * (targetSlot - mapping.addendB)) & mapping.tableMask;
    uint32_t randomUpper = generator.DrawRange(0x10000000u, 0xEFFFFFFFu) & ~mapping.tableMask;
    uint32_t rawState = randomUpper | targetLower;
    uint32_t finalState = rawState ^ mapping.xorKey;
    return finalState;
}

static constexpr const char* ARITHMETIC_TAG = "obfuscator.arithmetic";

static llvm::Value* TagArithmeticValue(llvm::Value* value)
{
    if (llvm::Instruction* instruction = llvm::dyn_cast<llvm::Instruction>(value))
    {
        instruction->setMetadata(ARITHMETIC_TAG, llvm::MDNode::get(instruction->getContext(), {}));
    }
    return value;
}

static llvm::Value* EmitOpaqueBarrier(llvm::IRBuilder<>& builder, llvm::Value* value)
{
    llvm::InlineAsm* asmOpaque = llvm::InlineAsm::get(
        llvm::FunctionType::get(value->getType(), {value->getType()}, false),
        "",
        "=r,0",
        false
    );
    return builder.CreateCall(asmOpaque, {value});
}

static llvm::Value* EmitHardenedState(
    llvm::IRBuilder<>& builder,
    uint32_t rawState,
    LeetObfuscator::RandomNumberGenerator& generator,
    bool stateHardening = true
)
{
    if (!stateHardening)
    {
        return builder.getInt32(rawState);
    }

    uint32_t share1 = generator.DrawRange(0x10000000u, 0xEFFFFFFFu);
    uint32_t share2 = rawState ^ share1;

    llvm::Value* share1Val = EmitOpaqueBarrier(builder, builder.getInt32(share1));
    llvm::Value* share2Val = builder.getInt32(share2);
    llvm::Value* combinedState = TagArithmeticValue(builder.CreateXor(share1Val, share2Val, "leet.disp.split.state"));

    return combinedState;
}

void RewriteTerminatorToDispatcher(
    llvm::Instruction* terminator,
    llvm::Value* nextStateValue,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    uint32_t dispatcherSlot,
    llvm::BasicBlock* dispatcherBlock,
    bool useMBA = true
)
{
    llvm::IRBuilder<> terminatorBuilder(terminator);

    terminatorBuilder.CreateStore(nextStateValue, dispatcherState, true);

    llvm::Type* intPtrType = jumpTable->getParent()->getDataLayout().getIntPtrType(jumpTable->getContext());

    llvm::Value* opaqueSlot = EmitOpaqueBarrier(terminatorBuilder, terminatorBuilder.getInt32(dispatcherSlot));

    llvm::Value* dispatcherOffsetGEP = terminatorBuilder.CreateInBoundsGEP(
        jumpTable->getValueType(),
        jumpTable,
        {terminatorBuilder.getInt32(0), opaqueSlot}
    );
    llvm::Value* dispatcherOffset32 = terminatorBuilder.CreateLoad(terminatorBuilder.getInt32Ty(), dispatcherOffsetGEP, true);
    llvm::Value* dispatcherOffset = terminatorBuilder.CreateSExt(dispatcherOffset32, intPtrType);
    llvm::Value* tableAddress = terminatorBuilder.CreatePtrToInt(jumpTable, intPtrType);

    llvm::Value* dispatcherAddressInt = nullptr;
    if (useMBA)
    {
        llvm::Value* oneConstant = llvm::ConstantInt::get(intPtrType, 1);
        llvm::Value* xorAddr = TagArithmeticValue(terminatorBuilder.CreateXor(tableAddress, dispatcherOffset, "leet.disp.mba.xor"));
        llvm::Value* andAddr = TagArithmeticValue(terminatorBuilder.CreateAnd(tableAddress, dispatcherOffset, "leet.disp.mba.and"));
        llvm::Value* shlAddr = TagArithmeticValue(terminatorBuilder.CreateShl(andAddr, oneConstant, "leet.disp.mba.shl"));
        dispatcherAddressInt = TagArithmeticValue(terminatorBuilder.CreateAdd(xorAddr, shlAddr, "leet.disp.mba.addr"));
    }
    else
    {
        dispatcherAddressInt = TagArithmeticValue(terminatorBuilder.CreateAdd(tableAddress, dispatcherOffset, "leet.disp.addr"));
    }

    llvm::Value* dispatcherAddress = terminatorBuilder.CreateIntToPtr(dispatcherAddressInt, terminatorBuilder.getPtrTy());

    llvm::IndirectBrInst* indirectBranchInstruction = terminatorBuilder.CreateIndirectBr(dispatcherAddress, 1);
    indirectBranchInstruction->addDestination(dispatcherBlock);

    terminator->eraseFromParent();
}

void RewriteBranchTerminator(
    llvm::BranchInst* branch,
    const std::vector<llvm::BasicBlock*>& basicBlocks,
    const DispatcherMapping& mapping,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    llvm::BasicBlock* dispatcherBlock,
    LeetObfuscator::RandomNumberGenerator& generator
)
{
    if (branch->isUnconditional())
    {
        llvm::BasicBlock* successor = branch->getSuccessor(0);
        auto iterator = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
        if (iterator == basicBlocks.end())
        {
            return;
        }

        uint32_t blockIndex = (uint32_t)std::distance(basicBlocks.begin(), iterator);
        const std::vector<uint32_t>& slots = mapping.blockSlots[blockIndex];
        uint32_t chosenSlot = slots[generator.DrawRange(0u, (uint32_t)slots.size() - 1)];
        uint32_t nextState = GenerateStateForSlot(chosenSlot, mapping, generator);

        llvm::IRBuilder<> terminatorBuilder(branch);
        llvm::Value* nextStateValue = EmitHardenedState(terminatorBuilder, nextState, generator, mapping.stateHardening);

        uint32_t chosenDispatcherSlot = mapping.dispatcherSlots[generator.DrawRange(0u, (uint32_t)mapping.dispatcherSlots.size() - 1)];
        RewriteTerminatorToDispatcher(branch, nextStateValue, dispatcherState, jumpTable, chosenDispatcherSlot, dispatcherBlock, mapping.useMBA);
    }
    else if (branch->isConditional())
    {
        llvm::BasicBlock* trueSuccessor = branch->getSuccessor(0);
        llvm::BasicBlock* falseSuccessor = branch->getSuccessor(1);
        auto trueIterator = std::find(basicBlocks.begin(), basicBlocks.end(), trueSuccessor);
        auto falseIterator = std::find(basicBlocks.begin(), basicBlocks.end(), falseSuccessor);
        if (trueIterator == basicBlocks.end() || falseIterator == basicBlocks.end())
        {
            return;
        }

        uint32_t trueBlockIndex = (uint32_t)std::distance(basicBlocks.begin(), trueIterator);
        uint32_t falseBlockIndex = (uint32_t)std::distance(basicBlocks.begin(), falseIterator);

        const std::vector<uint32_t>& trueSlots = mapping.blockSlots[trueBlockIndex];
        const std::vector<uint32_t>& falseSlots = mapping.blockSlots[falseBlockIndex];

        uint32_t chosenTrueSlot = trueSlots[generator.DrawRange(0u, (uint32_t)trueSlots.size() - 1)];
        uint32_t chosenFalseSlot = falseSlots[generator.DrawRange(0u, (uint32_t)falseSlots.size() - 1)];

        uint32_t stateTrue = GenerateStateForSlot(chosenTrueSlot, mapping, generator);
        uint32_t stateFalse = GenerateStateForSlot(chosenFalseSlot, mapping, generator);

        llvm::IRBuilder<> terminatorBuilder(branch);

        llvm::Value* nextStateValue = nullptr;
        if (mapping.stateHardening)
        {
            llvm::Value* conditionExt = terminatorBuilder.CreateZExt(branch->getCondition(), terminatorBuilder.getInt32Ty(), "leet.disp.cond.ext");
            llvm::Value* conditionMask = TagArithmeticValue(terminatorBuilder.CreateNeg(conditionExt, "leet.disp.cond.mask"));
            uint32_t stateDifference = stateTrue ^ stateFalse;
            llvm::Value* opaqueDiff = EmitOpaqueBarrier(terminatorBuilder, terminatorBuilder.getInt32(stateDifference));
            llvm::Value* maskedDifference = TagArithmeticValue(terminatorBuilder.CreateAnd(conditionMask, opaqueDiff, "leet.disp.diff.masked"));

            uint32_t falseShare1 = generator.DrawRange(0x10000000u, 0xEFFFFFFFu);
            uint32_t falseShare2 = stateFalse ^ falseShare1;
            llvm::Value* opaqueFalseShare1 = EmitOpaqueBarrier(terminatorBuilder, terminatorBuilder.getInt32(falseShare1));
            llvm::Value* baseFalseState = TagArithmeticValue(terminatorBuilder.CreateXor(opaqueFalseShare1, terminatorBuilder.getInt32(falseShare2), "leet.disp.base.false"));

            nextStateValue = TagArithmeticValue(terminatorBuilder.CreateXor(maskedDifference, baseFalseState, "leet.disp.next.state"));
        }
        else
        {
            llvm::Value* conditionExt = terminatorBuilder.CreateZExt(branch->getCondition(), terminatorBuilder.getInt32Ty(), "leet.disp.cond.ext");
            llvm::Value* conditionMask = TagArithmeticValue(terminatorBuilder.CreateNeg(conditionExt, "leet.disp.cond.mask"));
            uint32_t stateDifference = stateTrue ^ stateFalse;
            llvm::Value* maskedDifference = TagArithmeticValue(terminatorBuilder.CreateAnd(conditionMask, terminatorBuilder.getInt32(stateDifference), "leet.disp.diff.masked"));
            nextStateValue = TagArithmeticValue(terminatorBuilder.CreateXor(maskedDifference, terminatorBuilder.getInt32(stateFalse), "leet.disp.next.state"));
        }

        uint32_t chosenDispatcherSlot = mapping.dispatcherSlots[generator.DrawRange(0u, (uint32_t)mapping.dispatcherSlots.size() - 1)];
        RewriteTerminatorToDispatcher(branch, nextStateValue, dispatcherState, jumpTable, chosenDispatcherSlot, dispatcherBlock, mapping.useMBA);
    }
}

void RewriteSwitchTerminator(
    llvm::SwitchInst* switchInstruction,
    const std::vector<llvm::BasicBlock*>& basicBlocks,
    const DispatcherMapping& mapping,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    llvm::BasicBlock* dispatcherBlock,
    LeetObfuscator::RandomNumberGenerator& generator
)
{
    llvm::BasicBlock* defaultDestination = switchInstruction->getDefaultDest();
    auto defaultIterator = std::find(basicBlocks.begin(), basicBlocks.end(), defaultDestination);
    if (defaultIterator == basicBlocks.end())
    {
        return;
    }

    uint32_t defaultBlockIndex = (uint32_t)std::distance(basicBlocks.begin(), defaultIterator);
    const std::vector<uint32_t>& defaultSlots = mapping.blockSlots[defaultBlockIndex];
    uint32_t chosenDefaultSlot = defaultSlots[generator.DrawRange(0u, (uint32_t)defaultSlots.size() - 1)];
    uint32_t defaultState = GenerateStateForSlot(chosenDefaultSlot, mapping, generator);

    llvm::IRBuilder<> terminatorBuilder(switchInstruction);
    llvm::Value* nextStateValue = EmitHardenedState(terminatorBuilder, defaultState, generator, mapping.stateHardening);

    for (auto caseIterator = switchInstruction->case_begin(); caseIterator != switchInstruction->case_end(); caseIterator++)
    {
        llvm::BasicBlock* caseSuccessor = caseIterator->getCaseSuccessor();
        auto caseSuccessorIterator = std::find(basicBlocks.begin(), basicBlocks.end(), caseSuccessor);
        if (caseSuccessorIterator == basicBlocks.end())
        {
            continue;
        }

        uint32_t caseBlockIndex = (uint32_t)std::distance(basicBlocks.begin(), caseSuccessorIterator);
        const std::vector<uint32_t>& caseSlots = mapping.blockSlots[caseBlockIndex];
        uint32_t chosenCaseSlot = caseSlots[generator.DrawRange(0u, (uint32_t)caseSlots.size() - 1)];
        uint32_t caseState = GenerateStateForSlot(chosenCaseSlot, mapping, generator);

        llvm::Value* condition = terminatorBuilder.CreateICmpEQ(
            switchInstruction->getCondition(),
            caseIterator->getCaseValue(),
            "leet.disp.case.cond"
        );

        llvm::Value* conditionExt = terminatorBuilder.CreateZExt(condition, terminatorBuilder.getInt32Ty(), "leet.disp.case.ext");
        llvm::Value* conditionMask = TagArithmeticValue(terminatorBuilder.CreateNeg(conditionExt, "leet.disp.case.mask"));
        llvm::Value* caseStateValue = EmitHardenedState(terminatorBuilder, caseState, generator, mapping.stateHardening);
        llvm::Value* difference = TagArithmeticValue(terminatorBuilder.CreateXor(caseStateValue, nextStateValue, "leet.disp.case.diff"));
        llvm::Value* maskedDifference = TagArithmeticValue(terminatorBuilder.CreateAnd(conditionMask, difference, "leet.disp.case.masked"));
        nextStateValue = TagArithmeticValue(terminatorBuilder.CreateXor(nextStateValue, maskedDifference, "leet.disp.case.state"));
    }

    uint32_t chosenDispatcherSlot = mapping.dispatcherSlots[generator.DrawRange(0u, (uint32_t)mapping.dispatcherSlots.size() - 1)];
    RewriteTerminatorToDispatcher(switchInstruction, nextStateValue, dispatcherState, jumpTable, chosenDispatcherSlot, dispatcherBlock, mapping.useMBA);
}

void RewriteIndirectBrTerminator(
    llvm::IndirectBrInst* indirectBranch,
    const std::vector<llvm::BasicBlock*>& basicBlocks,
    const DispatcherMapping& mapping,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    llvm::BasicBlock* dispatcherBlock,
    LeetObfuscator::RandomNumberGenerator& generator
)
{
    llvm::IRBuilder<> terminatorBuilder(indirectBranch);
    llvm::Value* nextStateValue = nullptr;

    for (unsigned int i = 0; i < indirectBranch->getNumSuccessors(); i++)
    {
        llvm::BasicBlock* successor = indirectBranch->getSuccessor(i);
        auto iterator = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
        if (iterator == basicBlocks.end())
        {
            continue;
        }

        uint32_t blockIndex = (uint32_t)std::distance(basicBlocks.begin(), iterator);
        const std::vector<uint32_t>& slots = mapping.blockSlots[blockIndex];
        uint32_t chosenSlot = slots[generator.DrawRange(0u, (uint32_t)slots.size() - 1)];
        uint32_t successorState = GenerateStateForSlot(chosenSlot, mapping, generator);
        llvm::Value* successorStateValue = EmitHardenedState(terminatorBuilder, successorState, generator, mapping.stateHardening);

        if (!nextStateValue)
        {
            nextStateValue = successorStateValue;
            continue;
        }

        llvm::Value* condition = terminatorBuilder.CreateICmpEQ(
            indirectBranch->getAddress(),
            llvm::BlockAddress::get(successor),
            "leet.disp.indirect.cond"
        );

        llvm::Value* conditionExt = terminatorBuilder.CreateZExt(condition, terminatorBuilder.getInt32Ty(), "leet.disp.indirect.ext");
        llvm::Value* conditionMask = TagArithmeticValue(terminatorBuilder.CreateNeg(conditionExt, "leet.disp.indirect.mask"));
        llvm::Value* difference = TagArithmeticValue(terminatorBuilder.CreateXor(successorStateValue, nextStateValue, "leet.disp.indirect.diff"));
        llvm::Value* maskedDifference = TagArithmeticValue(terminatorBuilder.CreateAnd(conditionMask, difference, "leet.disp.indirect.masked"));
        nextStateValue = TagArithmeticValue(terminatorBuilder.CreateXor(nextStateValue, maskedDifference, "leet.disp.indirect.state"));
    }

    if (nextStateValue)
    {
        uint32_t chosenDispatcherSlot = mapping.dispatcherSlots[generator.DrawRange(0u, (uint32_t)mapping.dispatcherSlots.size() - 1)];
        RewriteTerminatorToDispatcher(indirectBranch, nextStateValue, dispatcherState, jumpTable, chosenDispatcherSlot, dispatcherBlock, mapping.useMBA);
    }
}

void RewriteCallBrTerminator(
    llvm::CallBrInst* callBranch,
    const std::vector<llvm::BasicBlock*>& basicBlocks,
    const DispatcherMapping& mapping,
    llvm::AllocaInst* dispatcherState,
    llvm::GlobalVariable* jumpTable,
    llvm::BasicBlock* dispatcherBlock,
    LeetObfuscator::RandomNumberGenerator& generator
)
{
    if (callBranch->getNumSuccessors() != 1)
    {
        return;
    }

    llvm::BasicBlock* successor = callBranch->getSuccessor(0);
    auto iterator = std::find(basicBlocks.begin(), basicBlocks.end(), successor);
    if (iterator == basicBlocks.end())
    {
        return;
    }

    uint32_t blockIndex = (uint32_t)std::distance(basicBlocks.begin(), iterator);
    const std::vector<uint32_t>& slots = mapping.blockSlots[blockIndex];
    uint32_t chosenSlot = slots[generator.DrawRange(0u, (uint32_t)slots.size() - 1)];
    uint32_t nextState = GenerateStateForSlot(chosenSlot, mapping, generator);

    llvm::IRBuilder<> terminatorBuilder(callBranch);
    llvm::Value* nextStateValue = EmitHardenedState(terminatorBuilder, nextState, generator, mapping.stateHardening);

    uint32_t chosenDispatcherSlot = mapping.dispatcherSlots[generator.DrawRange(0u, (uint32_t)mapping.dispatcherSlots.size() - 1)];
    RewriteTerminatorToDispatcher(callBranch, nextStateValue, dispatcherState, jumpTable, chosenDispatcherSlot, dispatcherBlock, mapping.useMBA);
}

llvm::PreservedAnalyses LeetObfuscator::DispatcherPass::run(llvm::Module &module, llvm::ModuleAnalysisManager& mam)
{
    llvm::errs() << "Running DispatcherPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    m_ProcessedFunctions = 0;
    m_TotalJumpTableEntries = 0;
    m_TotalDestinations = 0;
    m_TotalDecoys = 0;

    std::vector<llvm::Function*> functions;

    for (auto& function : module)
    {
        functions.push_back(&function);
    }

    
    for (auto& function : functions)
    {
        CreateDispatcherInAFunction(function);
    }

    m_Logger.LogModule(
        module,
        "DispatcherPass summary: processed " + std::to_string(m_ProcessedFunctions) +
        " functions with " + std::to_string(m_TotalJumpTableEntries) +
        " total jump table entries across " + std::to_string(m_TotalDestinations) +
        " destinations and " + std::to_string(m_TotalDecoys) + " decoys",
        0
    );

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
    if (function->isDeclaration())
    {
        return;
    }

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
        m_Logger.LogFunction(*function, "Skipping function: only contains entry block", 1);
        return;
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
    {
        m_Logger.LogFunction(*function, "Skipping function: only contains entry block", 1);
        return;
    }

    // split the entry block so the dispatcher owns the flow
    llvm::BasicBlock* entryBlock = &function->getEntryBlock();
    llvm::Instruction* firstNonAlloca = llvm::dyn_cast<llvm::Instruction>(entryBlock->getFirstNonPHIOrDbgOrAlloca());
    if (!firstNonAlloca)
    {
        llvm::errs() << "ERROR: Function '" << function->getName() << "' has no non alloca instructions in the entry block, skipping.\n";
        return;
    }
    llvm::BasicBlock* bodyBlock = entryBlock->splitBasicBlock(firstNonAlloca);
    basicBlocks.insert(basicBlocks.begin(), bodyBlock);

    entryBlock->setName("leet.entry.block");

    llvm::IRBuilder<> entryBeginBuilder(entryBlock, entryBlock->begin());
    llvm::AllocaInst* dispatcherState = entryBeginBuilder.CreateAlloca(entryBeginBuilder.getInt32Ty());

    llvm::BasicBlock* dispatcherBlock = llvm::BasicBlock::Create(context, "leet.dispatcher.block", function);

    uint32_t numBlocks = (uint32_t)basicBlocks.size();
    uint32_t slotsPerBlock = attributes.dispatcherJumpTableSlotsPerBlock;
    if (slotsPerBlock == 0)
    {
        slotsPerBlock = 1;
    }
    if (attributes.dispatcherJumpTableMaxBlocksForMultiSlot > 0 && numBlocks > attributes.dispatcherJumpTableMaxBlocksForMultiSlot)
    {
        slotsPerBlock = 1;
    }

    uint32_t numDispatcherSlots = attributes.dispatcherJumpTableDispatcherSlots;
    if (numDispatcherSlots == 0)
    {
        numDispatcherSlots = 1;
    }

    uint32_t minTableSize = attributes.dispatcherJumpTableMinSize;
    if (minTableSize < 4)
    {
        minTableSize = 4;
    }

    uint32_t rawTableSize = numBlocks * slotsPerBlock + numDispatcherSlots + attributes.dispatcherJumpTablePadding;
    uint32_t tableSize = minTableSize;
    while (tableSize < rawTableSize)
    {
        tableSize <<= 1;
    }

    if (attributes.dispatcherJumpTableMaxSize != 0 && tableSize > attributes.dispatcherJumpTableMaxSize)
    {
        uint32_t requiredSlots = numBlocks + numDispatcherSlots;
        uint32_t cappedSize = std::max(attributes.dispatcherJumpTableMaxSize, requiredSlots);
        uint32_t pow2 = 1;
        while (pow2 < cappedSize)
        {
            pow2 <<= 1;
        }
        tableSize = pow2;
    }

    std::vector<uint32_t> availableSlots(tableSize);
    for (uint32_t i = 0; i < tableSize; i++)
    {
        availableSlots[i] = i;
    }
    generator->Shuffle(availableSlots.begin(), availableSlots.end());

    DispatcherMapping mapping;
    mapping.tableSize = tableSize;
    mapping.tableMask = tableSize - 1;
    mapping.useMBA = (generator->DrawRange(1u, 100u) <= attributes.dispatcherMBAProbability);
    mapping.stateHardening = attributes.dispatcherStateHardening;

    for (uint32_t d = 0; d < numDispatcherSlots && !availableSlots.empty(); d++)
    {
        mapping.dispatcherSlots.push_back(availableSlots.back());
        availableSlots.pop_back();
    }

    mapping.blockSlots.resize(numBlocks);
    for (uint32_t i = 0; i < numBlocks; i++)
    {
        for (uint32_t s = 0; s < slotsPerBlock; s++)
        {
            mapping.blockSlots[i].push_back(availableSlots.back());
            availableSlots.pop_back();
        }
    }

    mapping.multiplierA = generator->DrawRange(0x10000000u, 0xEFFFFFFFu) | 1u;
    mapping.inverseMultiplierA = ComputeModularInverse32(mapping.multiplierA);
    mapping.addendB = generator->DrawRange(0x10000000u, 0xEFFFFFFFu);
    mapping.xorKey = generator->DrawRange(0x10000000u, 0xEFFFFFFFu);

    std::vector<llvm::BasicBlock*> tableBlockTarget(tableSize, nullptr);
    for (uint32_t dispSlot : mapping.dispatcherSlots)
    {
        tableBlockTarget[dispSlot] = dispatcherBlock;
    }

    for (uint32_t i = 0; i < numBlocks; i++)
    {
        for (uint32_t slot : mapping.blockSlots[i])
        {
            tableBlockTarget[slot] = basicBlocks[i];
        }
    }

    for (uint32_t decoySlot : availableSlots)
    {
        uint32_t randomBlockIndex = generator->DrawRange(0u, numBlocks - 1);
        tableBlockTarget[decoySlot] = basicBlocks[randomBlockIndex];
    }

    uint32_t decoySlotsCount = (uint32_t)availableSlots.size();
    m_Logger.LogFunction(
        *function,
        "Creating dispatcher with jump table of size " + std::to_string(tableSize) +
        " (" + std::to_string(numBlocks) + " destinations, " +
        std::to_string(slotsPerBlock) + " duplicates per block, " +
        std::to_string(numDispatcherSlots) + " dispatcher slots, " +
        std::to_string(decoySlotsCount) + " decoys)",
        2
    );

    m_ProcessedFunctions++;
    m_TotalJumpTableEntries += tableSize;
    m_TotalDestinations += numBlocks;
    m_TotalDecoys += decoySlotsCount;

    llvm::Type* int32Type = llvm::Type::getInt32Ty(context);
    llvm::Type* intPtrType = function->getParent()->getDataLayout().getIntPtrType(context);
    llvm::ArrayType* jumpTableType = llvm::ArrayType::get(int32Type, tableSize);
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
    for (uint32_t s = 0; s < tableSize; s++)
    {
        llvm::BasicBlock* targetBlock = tableBlockTarget[s];
        llvm::Constant* blockAddress = llvm::ConstantExpr::getPtrToInt(llvm::BlockAddress::get(targetBlock), intPtrType);
        llvm::Constant* tableAddress = llvm::ConstantExpr::getPtrToInt(jumpTable, intPtrType);
        llvm::Constant* relativeOffset64 = llvm::ConstantExpr::getSub(blockAddress, tableAddress);
        llvm::Constant* relativeOffset32 = llvm::ConstantExpr::getTrunc(relativeOffset64, int32Type);

        blockAddresses.push_back(relativeOffset32);
    }
    llvm::Constant* initializer = llvm::ConstantArray::get(jumpTableType, blockAddresses);
    jumpTable->setInitializer(initializer);

    uint32_t bodySlot = mapping.blockSlots[0][generator->DrawRange(0u, (uint32_t)mapping.blockSlots[0].size() - 1)];
    uint32_t initialBodyState = GenerateStateForSlot(bodySlot, mapping, *generator);

    llvm::IRBuilder<> entryTermBuilder(entryBlock->getTerminator());
    llvm::Value* initialHardenedState = EmitHardenedState(entryTermBuilder, initialBodyState, *generator, mapping.stateHardening);
    uint32_t chosenEntryDispSlot = mapping.dispatcherSlots[generator->DrawRange(0u, (uint32_t)mapping.dispatcherSlots.size() - 1)];

    RewriteTerminatorToDispatcher(
        entryBlock->getTerminator(),
        initialHardenedState,
        dispatcherState,
        jumpTable,
        chosenEntryDispSlot,
        dispatcherBlock,
        mapping.useMBA
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

    llvm::Value* unmaskedState = nullptr;
    if (mapping.useMBA)
    {
        uint32_t xorChoice = generator->DrawRange(0u, 2u);
        llvm::Value* xorKeyVal = dispatcherBuilder.getInt32(mapping.xorKey);
        if (xorChoice == 0)
        {
            llvm::Value* orVal = TagArithmeticValue(dispatcherBuilder.CreateOr(dispatcherStateLoad, xorKeyVal, "leet.disp.xor.or"));
            llvm::Value* andVal = TagArithmeticValue(dispatcherBuilder.CreateAnd(dispatcherStateLoad, xorKeyVal, "leet.disp.xor.and"));
            unmaskedState = TagArithmeticValue(dispatcherBuilder.CreateSub(orVal, andVal, "leet.disp.unmask"));
        }
        else if (xorChoice == 1)
        {
            llvm::Value* notKey = TagArithmeticValue(dispatcherBuilder.CreateNot(xorKeyVal, "leet.disp.not.key"));
            llvm::Value* notState = TagArithmeticValue(dispatcherBuilder.CreateNot(dispatcherStateLoad, "leet.disp.not.state"));
            llvm::Value* stateAndNotKey = TagArithmeticValue(dispatcherBuilder.CreateAnd(dispatcherStateLoad, notKey, "leet.disp.s.notk"));
            llvm::Value* notStateAndKey = TagArithmeticValue(dispatcherBuilder.CreateAnd(notState, xorKeyVal, "leet.disp.nots.k"));
            unmaskedState = TagArithmeticValue(dispatcherBuilder.CreateOr(stateAndNotKey, notStateAndKey, "leet.disp.unmask"));
        }
        else
        {
            llvm::Value* addVal = TagArithmeticValue(dispatcherBuilder.CreateAdd(dispatcherStateLoad, xorKeyVal, "leet.disp.xor.add"));
            llvm::Value* andVal = TagArithmeticValue(dispatcherBuilder.CreateAnd(dispatcherStateLoad, xorKeyVal, "leet.disp.xor.and"));
            llvm::Value* mulVal = TagArithmeticValue(dispatcherBuilder.CreateMul(andVal, dispatcherBuilder.getInt32(2), "leet.disp.xor.mul"));
            unmaskedState = TagArithmeticValue(dispatcherBuilder.CreateSub(addVal, mulVal, "leet.disp.unmask"));
        }
    }
    else
    {
        unmaskedState = TagArithmeticValue(dispatcherBuilder.CreateXor(dispatcherStateLoad, dispatcherBuilder.getInt32(mapping.xorKey), "leet.disp.unmask"));
    }

    llvm::Value* stateMul = TagArithmeticValue(dispatcherBuilder.CreateMul(
        unmaskedState,
        dispatcherBuilder.getInt32(mapping.multiplierA),
        "leet.disp.mul"
    ));

    llvm::Value* stateAdd = TagArithmeticValue(dispatcherBuilder.CreateAdd(
        stateMul,
        dispatcherBuilder.getInt32(mapping.addendB),
        "leet.disp.add"
    ));

    llvm::Value* decodedSlot = TagArithmeticValue(dispatcherBuilder.CreateAnd(
        stateAdd,
        dispatcherBuilder.getInt32(mapping.tableMask),
        "leet.disp.slot"
    ));

    llvm::Value* relativeOffsetGEP = dispatcherBuilder.CreateInBoundsGEP(
        jumpTable->getValueType(),
        jumpTable,
        {dispatcherBuilder.getInt32(0), decodedSlot}
    );
    llvm::Value* relativeOffset32 = dispatcherBuilder.CreateLoad(int32Type, relativeOffsetGEP, true);
    llvm::Value* relativeOffset = dispatcherBuilder.CreateSExt(relativeOffset32, intPtrType);
    llvm::Value* tableAddress = dispatcherBuilder.CreatePtrToInt(jumpTable, intPtrType);
    llvm::Value* targetAddressInt = TagArithmeticValue(dispatcherBuilder.CreateAdd(tableAddress, relativeOffset));
    llvm::Value* nextBlockAddress = dispatcherBuilder.CreateIntToPtr(targetAddressInt, dispatcherBuilder.getPtrTy());

    llvm::IndirectBrInst* indirectBranchInstruction = dispatcherBuilder.CreateIndirectBr(nextBlockAddress, basicBlocks.size() + 1);
    for (llvm::BasicBlock* basicBlock : basicBlocks)
    {
        indirectBranchInstruction->addDestination(basicBlock);
    }
    indirectBranchInstruction->addDestination(dispatcherBlock);

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
            RewriteBranchTerminator(branch, basicBlocks, mapping, dispatcherState, jumpTable, dispatcherBlock, *generator);
            continue;
        }

        if (auto* switchInstruction = llvm::dyn_cast<llvm::SwitchInst>(terminator))
        {
            RewriteSwitchTerminator(switchInstruction, basicBlocks, mapping, dispatcherState, jumpTable, dispatcherBlock, *generator);
            continue;
        }

        if (auto* indirectBranch = llvm::dyn_cast<llvm::IndirectBrInst>(terminator))
        {
            RewriteIndirectBrTerminator(indirectBranch, basicBlocks, mapping, dispatcherState, jumpTable, dispatcherBlock, *generator);
            continue;
        }

        if (auto* callBranch = llvm::dyn_cast<llvm::CallBrInst>(terminator))
        {
            RewriteCallBrTerminator(callBranch, basicBlocks, mapping, dispatcherState, jumpTable, dispatcherBlock, *generator);
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
