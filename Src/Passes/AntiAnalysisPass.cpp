#include "AntiAnalysisPass.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IntrinsicsX86.h"
#include <llvm/Linker/Linker.h>
#include "SettingsParser.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Support/MemoryBuffer.h"

#include "../../build/LeetObfuscator/LeetRuntimeAntiAnalysis.x64.inc" // Template bitcode
#include "../../build/LeetObfuscator/LeetRuntimeAntiAnalysis.x86.inc"

#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <math.h>


static int opaqueCounter = 0;
static int pidCounter = 0;
static int blacklistCounter = 0;
static int rdtscCounter = 0;

const char* ANTI_ANALYSIS_TAG = "leet.anti.analysis";

llvm::PreservedAnalyses LeetObfuscator::AntiAnalysisPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running AntiAnalysisPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    EmittedTemplate templates;
    if (!LinkTemplateModule(module, templates))
    {
        llvm::errs() << "AntiAnalysisPass Failed to link template module?\n";
        exit(1);
    }

    for (auto& function : module)
    {
        if (!function.getName().contains("__leet_is_debugger_present_blacklist") &&
            !function.getName().contains("__leet_is_debugger_present_tracer_pid")
        )
        {
            ObfuscateFunction(function, templates);
        }
    }

    std::ostringstream message;
    message << "Inserted " << opaqueCounter << " Opaques | " << rdtscCounter << " RDTSCs | " << pidCounter << " PIDs | " << blacklistCounter << " Blacklists";
    m_Logger.LogModule(module, message.str());
    opaqueCounter = 0;
    pidCounter = 0;
    blacklistCounter = 0;
    rdtscCounter = 0;
    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::AntiAnalysisPass::ObfuscateFunction(llvm::Function &function, EmittedTemplate& templates)
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

    bool onePerFunction = attributes.antiAnalysisOnlyEntryBlock;
    {
        std::vector<llvm::BasicBlock*> blocksToObfuscate;

        if (onePerFunction)
        {
            auto& block = function.getEntryBlock();
            
            if (!SettingsParser::ShouldSkipBlock(&block, attributes) && !block.getFirstNonPHIIt()->getMetadata(ANTI_ANALYSIS_TAG))
            {
                if (generator->DrawRange(1u, 100u) <= attributes.antiAnalysisProbability)
                    blocksToObfuscate.push_back(&block);
            } 
        }
        else
        {
            for (auto& block : function)
            {
                if (SettingsParser::ShouldSkipBlock(&block, attributes))
                    continue;
    
                if (block.getFirstNonPHIIt()->getMetadata(ANTI_ANALYSIS_TAG))
                {
                    continue;
                }
    
                if (generator->DrawRange(1u, 100u) > attributes.antiAnalysisProbability)
                    continue;
                
                blocksToObfuscate.push_back(&block);
            }
        }

        for (llvm::BasicBlock* block : blocksToObfuscate)
        {
            ObfuscateBlock(block, attributes, templates, generator);
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

bool LeetObfuscator::AntiAnalysisPass::ObfuscateBlock(llvm::BasicBlock* block, SettingsParser::FunctionAttributes& attributes, EmittedTemplate& templates, std::shared_ptr<RandomNumberGenerator> generator)
{
    float rdtscProb = (float)attributes.antiAnalysisRdtscRatio;
    float pidProb = (float)attributes.antiAnalysisPIDRatio;
    float blacklistProb = (float)attributes.antiAnalysisBlackListRatio;
    float opaqueProb = (float)attributes.antiAnalysisOpaqueRatio;

    llvm::Triple triple(block->getParent()->getParent()->getTargetTriple());

    if (block->getParent()->getName().contains("__leet_exception") || triple.isOSWindows())
    {
        blacklistProb = 0;
        pidProb = 0;
    }

    bool randomPos = true;
    if (attributes.antiAnalysisInsertPosition == SettingsParser::BogusInsertPosition::Start)
        randomPos = false;

    float probabilitySum = rdtscProb + pidProb + blacklistProb + opaqueProb;

    if (probabilitySum <= 0.0)
    {
        return false;
    }

    llvm::BasicBlock* bogus = CreateInvalidBogusBlock(block->getParent(), generator);
    llvm::BasicBlock* newSplitBlock = nullptr;

    float randomNumber = generator->DrawRange(1.0f, 100.0f);

    if (randomNumber <= (rdtscProb / probabilitySum) * 100.0f)
    {
        newSplitBlock = ChainBogusIntoBlockRdtsc(block, bogus, randomPos, generator, attributes);
        if (newSplitBlock)
            rdtscCounter++;
    }
    else if (randomNumber <= (rdtscProb + pidProb) / probabilitySum * 100.0f)
    {
        newSplitBlock = ChainBogusIntoBlockAntiDebug(block, bogus, AntiDebugType::pid, randomPos, generator, templates, attributes);
        if (newSplitBlock)
            pidCounter++;
    }
    else if (randomNumber <= (rdtscProb + pidProb + blacklistProb) / probabilitySum * 100.0f)
    {
        newSplitBlock = ChainBogusIntoBlockAntiDebug(block, bogus, AntiDebugType::blacklist, randomPos, generator, templates, attributes);
        if (newSplitBlock)
            blacklistCounter++;
    }
    else if (randomNumber <= (rdtscProb + pidProb + blacklistProb + opaqueProb) / probabilitySum * 100.0f)
    {
        newSplitBlock = ChainBogusIntoBlock(block, bogus, randomPos, generator, attributes);
        if (newSplitBlock)
            opaqueCounter++;
    }

    if (!newSplitBlock)
    {
        if (bogus)
            bogus->eraseFromParent();
        return false;
    }

    if (bogus && bogus->hasNPredecessors(0))
    {
        bogus->eraseFromParent();
        bogus = nullptr;
    }

    block->getFirstNonPHIIt()->setMetadata(ANTI_ANALYSIS_TAG, llvm::MDNode::get(block->getContext(), {}));
    if (bogus && !bogus->empty())
        bogus->getFirstNonPHIIt()->setMetadata(ANTI_ANALYSIS_TAG, llvm::MDNode::get(block->getContext(), {}));

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

llvm::GlobalVariable* LeetObfuscator::AntiAnalysisPass::GetOrEmitPoisonRingGlobal(llvm::Module& module, std::shared_ptr<RandomNumberGenerator> generator)
{
    llvm::GlobalVariable* poisonRingGlobal = module.getGlobalVariable("__leet_poison_ring");
    if (!poisonRingGlobal)
    {
        llvm::ArrayType* ringType = llvm::ArrayType::get(llvm::Type::getInt64Ty(module.getContext()), 8);
        std::vector<llvm::Constant*> initialValues;
        uint64_t cumulativeXor = 0;
        for (uint32_t i = 0; i < 7; i++)
        {
            uint64_t val = generator ? generator->DrawRange(0x10000000ULL, 0xFFFFFFFFULL) : 0x12345678ULL + (uint64_t)i;
            initialValues.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(module.getContext()), val));
            cumulativeXor ^= val;
        }
        initialValues.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(module.getContext()), cumulativeXor));
        llvm::Constant* ringInitializer = llvm::ConstantArray::get(ringType, initialValues);

        poisonRingGlobal = new llvm::GlobalVariable(
            module,
            ringType,
            false,
            llvm::GlobalValue::WeakAnyLinkage,
            ringInitializer,
            "__leet_poison_ring"
        );
    }
    return poisonRingGlobal;
}

void LeetObfuscator::AntiAnalysisPass::EmitRingStatePoisoning(llvm::IRBuilder<>& builder, llvm::Module& module, llvm::Value* detectedCondition, std::shared_ptr<RandomNumberGenerator> generator)
{
    uint32_t slotIndex = generator->DrawRange(0u, 7u);
    uint64_t poisonDelta = generator->DrawRange(0x10000000ULL, 0xFFFFFFFFULL);
    llvm::Value* poisonValue = builder.CreateSelect(
        detectedCondition,
        builder.getInt64(poisonDelta),
        builder.getInt64(0)
    );
    llvm::GlobalVariable* ringVar = GetOrEmitPoisonRingGlobal(module, generator);
    llvm::Value* slotPtr = builder.CreateInBoundsGEP(
        ringVar->getValueType(),
        ringVar,
        { builder.getInt32(0), builder.getInt32(slotIndex) }
    );
    llvm::Value* currentSlot = builder.CreateLoad(builder.getInt64Ty(), slotPtr, true);
    llvm::Value* newSlot = builder.CreateXor(currentSlot, poisonValue);
    builder.CreateStore(newSlot, slotPtr, true);
}

bool LeetObfuscator::AntiAnalysisPass::EmitLocalStatePoisoning(llvm::IRBuilder<>& builder, llvm::BasicBlock* block, llvm::BasicBlock* newSplitBlock, llvm::Value* detectedCondition, DominatingPair inputPair, std::shared_ptr<RandomNumberGenerator> generator)
{
    const llvm::DataLayout& DL = block->getModule()->getDataLayout();

    std::vector<llvm::Value*> candidates;

    auto consider = [&](llvm::Value* value)
    {
        if (!value || llvm::isa<llvm::Constant>(value) || llvm::isa<llvm::InlineAsm>(value))
        {
            return;
        }
        if (auto* inst = llvm::dyn_cast<llvm::Instruction>(value))
        {
            if (inst->getType()->isVoidTy() || inst->isTerminator() || llvm::isa<llvm::AllocaInst>(inst))
            {
                return;
            }
        }

        llvm::Type* type = value->getType();
        if (type->isIntegerTy())
        {
            unsigned int bits = type->getIntegerBitWidth();
            if (bits == 8 || bits == 16 || bits == 32 || bits == 64)
            {
                if (std::find(candidates.begin(), candidates.end(), value) == candidates.end())
                {
                    candidates.push_back(value);
                }
            }
        }
        else if (type->isPointerTy())
        {
            if (std::find(candidates.begin(), candidates.end(), value) == candidates.end())
            {
                candidates.push_back(value);
            }
        }
    };

    if (inputPair.first)
    {
        consider(inputPair.first);
    }
    if (inputPair.second)
    {
        consider(inputPair.second);
    }

    for (llvm::Instruction& inst : *block)
    {
        if (&inst == detectedCondition)
        {
            continue;
        }
        consider(&inst);
    }

    llvm::Function* function = block->getParent();
    for (llvm::Argument& arg : function->args())
    {
        consider(&arg);
    }

    if (candidates.empty())
    {
        return false;
    }

    if (candidates.size() > 1)
    {
        for (size_t i = candidates.size() - 1; i > 0; i--)
        {
            size_t j = (size_t)generator->DrawRange(0u, (uint32_t)i);
            std::swap(candidates[i], candidates[j]);
        }
    }

    uint32_t replacedCount = 0;

    for (llvm::Value* candidate : candidates)
    {
        std::vector<llvm::Instruction*> usersToUpdate;
        for (auto* user : candidate->users())
        {
            if (auto* userInst = llvm::dyn_cast<llvm::Instruction>(user))
            {
                usersToUpdate.push_back(userInst);
            }
        }

        std::vector<llvm::Instruction*> validUsers;
        for (auto* userInst : usersToUpdate)
        {
            if (!llvm::isa<llvm::PHINode>(userInst) && userInst->getParent() == newSplitBlock)
            {
                validUsers.push_back(userInst);
            }
        }

        if (validUsers.empty())
        {
            continue;
        }

        llvm::Value* corruptedVal = nullptr;

        if (candidate->getType()->isIntegerTy())
        {
            uint64_t garbageDelta = generator->DrawRange(1ULL, 0xFFFFFFFFULL);
            llvm::Value* deltaVal = llvm::ConstantInt::get(candidate->getType(), garbageDelta);
            llvm::Value* zeroVal = llvm::ConstantInt::get(candidate->getType(), 0);
            llvm::Value* corruptionMask = builder.CreateSelect(detectedCondition, deltaVal, zeroVal);
            corruptedVal = builder.CreateXor(candidate, corruptionMask);
        }
        else if (candidate->getType()->isPointerTy())
        {
            llvm::Type* intPtrTy = DL.getIntPtrType(candidate->getType());
            llvm::Value* ptrAsInt = builder.CreatePtrToInt(candidate, intPtrTy);
            uint64_t garbageOffset = generator->DrawRange(0x1000ULL, 0xFFFFFFULL);
            llvm::Value* offsetVal = llvm::ConstantInt::get(intPtrTy, garbageOffset);
            llvm::Value* zeroVal = llvm::ConstantInt::get(intPtrTy, 0);
            llvm::Value* corruptionMask = builder.CreateSelect(detectedCondition, offsetVal, zeroVal);
            llvm::Value* corruptedInt = builder.CreateXor(ptrAsInt, corruptionMask);
            corruptedVal = builder.CreateIntToPtr(corruptedInt, candidate->getType());
        }

        if (!corruptedVal)
        {
            continue;
        }

        for (auto* userInst : validUsers)
        {
            userInst->replaceUsesOfWith(candidate, corruptedVal);
            replacedCount++;
        }

        if (replacedCount > 0)
        {
            return true;
        }
    }

    return false;
}

llvm::Value* LeetObfuscator::AntiAnalysisPass::GenerateNonLinear2DMbaPredicate(llvm::IRBuilder<>& builder, llvm::Value* x, llvm::Value* y, std::shared_ptr<RandomNumberGenerator> generator)
{
    llvm::Function* function = builder.GetInsertBlock()->getParent();
    const llvm::DataLayout& DL = function->getParent()->getDataLayout();

    if (x->getType()->isPointerTy())
    {
        llvm::Type* intPtrTy = DL.getIntPtrType(x->getType());
        x = builder.CreatePtrToInt(x, intPtrTy);
    }

    if (!y)
    {
        uint64_t randomConst = generator->DrawRange(0x10000000ULL, 0xFFFFFFFFULL);
        llvm::Constant* cVal = llvm::ConstantInt::get(x->getType(), randomConst);
        y = builder.CreateAdd(builder.CreateXor(x, cVal), llvm::ConstantInt::get(x->getType(), 1));
    }
    else if (y->getType()->isPointerTy())
    {
        llvm::Type* intPtrTy = DL.getIntPtrType(y->getType());
        y = builder.CreatePtrToInt(y, intPtrTy);
    }

    if (x->getType()->getIntegerBitWidth() != y->getType()->getIntegerBitWidth())
    {
        if (x->getType()->getIntegerBitWidth() > y->getType()->getIntegerBitWidth())
            y = builder.CreateZExt(y, x->getType());
        else
            x = builder.CreateZExt(x, y->getType());
    }

    llvm::InlineAsm* identity = llvm::InlineAsm::get(
        llvm::FunctionType::get(x->getType(), {x->getType()}, false), "", "=r,0", true
    );
    llvm::Value* opaqueX = builder.CreateCall(identity, {x});
    llvm::Value* opaqueY = builder.CreateCall(identity, {y});

    uint32_t familyChoice = generator->DrawRange(0u, 2u);
    llvm::Value* condition = nullptr;

    if (familyChoice == 0)
    {
        llvm::Value* orVal = builder.CreateOr(opaqueX, opaqueY);
        llvm::Value* andVal = builder.CreateAnd(opaqueX, opaqueY);
        llvm::Value* notX = builder.CreateNot(opaqueX);
        llvm::Value* notY = builder.CreateNot(opaqueY);
        llvm::Value* andNotY = builder.CreateAnd(opaqueX, notY);
        llvm::Value* andNotX = builder.CreateAnd(opaqueY, notX);
        llvm::Value* term1 = builder.CreateMul(orVal, andVal);
        llvm::Value* term2 = builder.CreateMul(andNotY, andNotX);
        llvm::Value* sum = builder.CreateAdd(term1, term2);
        llvm::Value* mulXY = builder.CreateMul(opaqueX, opaqueY);
        llvm::Value* diff = builder.CreateSub(sum, mulXY);
        condition = builder.CreateICmpEQ(diff, llvm::ConstantInt::get(diff->getType(), 0));
    }
    else if (familyChoice == 1)
    {
        llvm::Value* ySq = builder.CreateMul(opaqueY, opaqueY);
        llvm::Value* sevenYSq = builder.CreateMul(llvm::ConstantInt::get(opaqueY->getType(), 7), ySq);
        llvm::Value* lhs = builder.CreateSub(sevenYSq, llvm::ConstantInt::get(opaqueY->getType(), 1));
        llvm::Value* xSq = builder.CreateMul(opaqueX, opaqueX);
        condition = builder.CreateICmpNE(lhs, xSq);
    }
    else
    {
        llvm::Value* xPlus1 = builder.CreateAdd(opaqueX, llvm::ConstantInt::get(opaqueX->getType(), 1));
        llvm::Value* xMul = builder.CreateMul(opaqueX, xPlus1);
        llvm::Value* xParity = builder.CreateAnd(xMul, llvm::ConstantInt::get(opaqueX->getType(), 1));
        llvm::Value* yPlus1 = builder.CreateAdd(opaqueY, llvm::ConstantInt::get(opaqueY->getType(), 1));
        llvm::Value* yMul = builder.CreateMul(opaqueY, yPlus1);
        llvm::Value* yParity = builder.CreateAnd(yMul, llvm::ConstantInt::get(opaqueY->getType(), 1));
        llvm::Value* combined = builder.CreateOr(xParity, yParity);
        condition = builder.CreateICmpEQ(combined, llvm::ConstantInt::get(combined->getType(), 0));
    }

    return condition;
}

llvm::BasicBlock* LeetObfuscator::AntiAnalysisPass::ChainBogusIntoBlock(llvm::BasicBlock *block, llvm::BasicBlock *bogusBlock, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator, const SettingsParser::FunctionAttributes& attributes)
{
    llvm::Function* function = block->getParent();

    auto termIt = block->getTerminator() ? block->getTerminator()->getIterator() : block->end();
    auto baseInsertPoint = block->getFirstInsertionPt();

    bool hasUsableArg = false;
    for (llvm::Argument& arg : function->args())
    {
        if (RankValue(&arg) >= 0)
        {
            hasUsableArg = true;
            break;
        }
    }

    if (!hasUsableArg)
    {
        for (auto it = block->getFirstInsertionPt(); it != termIt; it++)
        {
            if (RankValue(&*it) >= 0 && !llvm::isa<llvm::PHINode>(&*it) && !llvm::isa<llvm::AllocaInst>(&*it))
            {
                baseInsertPoint = std::next(it);
                break;
            }
        }
    }

    if (baseInsertPoint == termIt || baseInsertPoint == block->end())
    {
        return nullptr;
    }

    uint32_t instructionCount = 0;
    for (auto it = baseInsertPoint; it != termIt; it++)
    {
        instructionCount++;
    }

    if (instructionCount == 0)
    {
        return nullptr;
    }

    auto insertPoint = baseInsertPoint;
    if (randomPos && instructionCount > 1)
    {
        uint32_t t = generator->DrawRange(0u, instructionCount - 1);
        std::advance(insertPoint, t);
    }

    if (insertPoint == termIt || insertPoint == block->end())
    {
        return nullptr;
    }

    DominatingPair inputPair = FindUsableInputPair(block, insertPoint, generator);

    const uint32_t stepSize = 4;
    const uint32_t maxSteps = 5;
    uint32_t stepsTaken = 0;

    while (!inputPair.first && insertPoint != termIt && stepsTaken < maxSteps)
    {
        for (uint32_t i = 0; i < stepSize && insertPoint != termIt; i++)
        {
            insertPoint++;
        }

        if (insertPoint == termIt || insertPoint == block->end())
        {
            break;
        }

        inputPair = FindUsableInputPair(block, insertPoint, generator);
        stepsTaken++;
    }

    if (!inputPair.first || insertPoint == termIt || insertPoint == block->end())
    {
        return nullptr;
    }

    llvm::BasicBlock* newSplitBlock = block->splitBasicBlock(insertPoint);
    block->getTerminator()->eraseFromParent();

    llvm::IRBuilder<> originalBlockBuilder(block, block->end());
    llvm::Value* condition = GenerateNonLinear2DMbaPredicate(originalBlockBuilder, inputPair.first, inputPair.second, generator);

    bool usePoisoning = attributes.antiAnalysisDelayedPoisoning && (generator->DrawRange(1u, 100u) <= attributes.antiAnalysisPoisonProbability);
    if (usePoisoning)
    {
        llvm::Value* detectedCondition = originalBlockBuilder.CreateNot(condition);
        bool localSuccess = false;
        if (generator->DrawRange(1u, 100u) <= attributes.antiAnalysisLocalPoisonProbability)
        {
            localSuccess = EmitLocalStatePoisoning(originalBlockBuilder, block, newSplitBlock, detectedCondition, inputPair, generator);
        }
        if (!localSuccess)
        {
            EmitRingStatePoisoning(originalBlockBuilder, *function->getParent(), detectedCondition, generator);
        }
        originalBlockBuilder.CreateBr(newSplitBlock);
    }
    else
    {
        originalBlockBuilder.CreateCondBr(condition, newSplitBlock, bogusBlock);
        llvm::IRBuilder<> bogusBlockBuilder(bogusBlock, bogusBlock->end());
        bogusBlockBuilder.CreateBr(newSplitBlock);
    }

    return newSplitBlock;
}

bool LeetObfuscator::AntiAnalysisPass::IsSafeToTimeAcross(llvm::Instruction &instruction)
{
    if (llvm::isa<llvm::CallBase>(instruction))
        return false;
    if (instruction.isAtomic())
        return false;
    if (llvm::isa<llvm::FenceInst>(instruction))
        return false;
    if (auto *LI = llvm::dyn_cast<llvm::LoadInst>(&instruction))
        if (LI->isVolatile())
            return false;
    if (auto *SI = llvm::dyn_cast<llvm::StoreInst>(&instruction))
        if (SI->isVolatile())
            return false;
    if (llvm::isa<llvm::InlineAsm>(instruction))
        return false;
    return true;
}

llvm::BasicBlock *LeetObfuscator::AntiAnalysisPass::ChainBogusIntoBlockRdtsc(llvm::BasicBlock *block, llvm::BasicBlock *bogusBlock, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator, const SettingsParser::FunctionAttributes& attributes)
{
    llvm::Module* module = block->getModule();

    uint32_t instructionCount = 0;
    for (auto it = block->getFirstNonPHIOrDbgOrAlloca(); it != block->end(); it++)
    {
        instructionCount++;
    }
    if (instructionCount < 3)
    {
        return nullptr;
    }

    auto startIt = block->getFirstNonPHIOrDbgOrAlloca();
    if (randomPos)
    {
        uint32_t t = generator->DrawRange(0u, (uint32_t)std::max((int)instructionCount - 5, 0));
        std::advance(startIt, t);
    }

    uint32_t secondTimerStep = 0;
    for (auto it = startIt; it != block->end(); it++)
    {
        if (!IsSafeToTimeAcross(*it))
        {
            break;
        }
        if (secondTimerStep >= 10)
        {
            break;
        }
        if (it == block->end())
        {
            break;
        }
        
        secondTimerStep++;
    }

    if (secondTimerStep < 3)
    {
        return nullptr;
    }

    auto blockIt = startIt;
    llvm::IRBuilder<> originalBlockBuilder(block, blockIt);
    originalBlockBuilder.SetCurrentDebugLocation(llvm::DebugLoc());
    llvm::Function* rdtscIntr = llvm::Intrinsic::getOrInsertDeclaration(module, llvm::Intrinsic::x86_rdtsc);
    llvm::Value* rdtscStart = originalBlockBuilder.CreateCall(rdtscIntr, {}, "rdtsc");

    std::advance(blockIt, secondTimerStep - 1);

    llvm::BasicBlock* newSplitBlock = block->splitBasicBlock(blockIt);
    block->getTerminator()->eraseFromParent();
    originalBlockBuilder.SetInsertPoint(block);
    originalBlockBuilder.SetCurrentDebugLocation(llvm::DebugLoc());

    llvm::Value* rdtscEnd = originalBlockBuilder.CreateCall(rdtscIntr, {}, "rdtsc");
    llvm::Value* time = originalBlockBuilder.CreateSub(rdtscEnd, rdtscStart);

    llvm::Value* detectedCondition = originalBlockBuilder.CreateICmpUGE(time, originalBlockBuilder.getInt64(100000000ULL));

    bool usePoisoning = attributes.antiAnalysisDelayedPoisoning && (generator->DrawRange(1u, 100u) <= attributes.antiAnalysisPoisonProbability);
    if (usePoisoning)
    {
        bool localSuccess = false;
        if (generator->DrawRange(1u, 100u) <= attributes.antiAnalysisLocalPoisonProbability)
        {
            DominatingPair inputPair = FindUsableInputPair(block, block->end(), generator);
            localSuccess = EmitLocalStatePoisoning(originalBlockBuilder, block, newSplitBlock, detectedCondition, inputPair, generator);
        }
        if (!localSuccess)
        {
            EmitRingStatePoisoning(originalBlockBuilder, *module, detectedCondition, generator);
        }
        originalBlockBuilder.CreateBr(newSplitBlock);
    }
    else
    {
        originalBlockBuilder.CreateCondBr(detectedCondition, bogusBlock, newSplitBlock);
        llvm::IRBuilder<> bogusBlockBuilder(bogusBlock, bogusBlock->end());
        bogusBlockBuilder.CreateBr(newSplitBlock);
    }

    return newSplitBlock;
}

bool LeetObfuscator::AntiAnalysisPass::LinkTemplateModule(llvm::Module& module, EmittedTemplate& templates)
{
    llvm::LLVMContext& context = module.getContext();
    llvm::StringRef data;
    
    bool is64 = module.getDataLayout().getPointerSizeInBits() == 64;
    if (is64)
    {
        data = llvm::StringRef(reinterpret_cast<const char*>(LeetRuntimeAntiAnalysis_x64_bc), LeetRuntimeAntiAnalysis_x64_bc_len);
    }
    else
    {
        data = llvm::StringRef(reinterpret_cast<const char*>(LeetRuntimeAntiAnalysis_x86_bc), LeetRuntimeAntiAnalysis_x86_bc_len);
    }
    
    auto buffer = llvm::MemoryBuffer::getMemBuffer(data, "leet_anti_analysis_templates", false);
    auto modOrErr = llvm::parseBitcodeFile(buffer->getMemBufferRef(), context);
    if (!modOrErr)
    {
        llvm::errs() << modOrErr.takeError() << "\n";
        return false;
    }
    std::unique_ptr<llvm::Module> templateMod = std::move(modOrErr.get());

    if (llvm::NamedMDNode* flags = templateMod->getModuleFlagsMetadata())
        templateMod->eraseNamedMetadata(flags);

    templateMod->setTargetTriple(module.getTargetTriple());
    templateMod->setDataLayout(module.getDataLayout());

    llvm::Linker linker(module);
    if (linker.linkInModule(std::move(templateMod), llvm::Linker::Flags::None))
    {
        llvm::errs() << "ERROR: failed to link antianalysis template module\n";
        return false;
    }

    templates.pidFunction = module.getFunction("__leet_is_debugger_present_tracer_pid");
    templates.blacklistFunction = module.getFunction("__leet_is_debugger_present_blacklist");

    if (!templates.pidFunction || !templates.blacklistFunction)
    {
        llvm::errs() << "ERROR: antidebug template functions missing after link\n";
        return false;
    }

    templates.pidFunction->setLinkage(llvm::GlobalValue::InternalLinkage);
    templates.pidFunction->setName(templates.pidFunction->getName());

    templates.blacklistFunction->setLinkage(llvm::GlobalValue::InternalLinkage);
    templates.blacklistFunction->setName(templates.blacklistFunction->getName());

    return true;
}

llvm::BasicBlock *LeetObfuscator::AntiAnalysisPass::ChainBogusIntoBlockAntiDebug(llvm::BasicBlock *block, llvm::BasicBlock *bogusBlock, AntiDebugType antiDebugType, bool randomPos, std::shared_ptr<RandomNumberGenerator> generator, EmittedTemplate& templates, const SettingsParser::FunctionAttributes& attributes)
{
    llvm::Function* function = block->getParent();

    auto insertPoint = block->getFirstInsertionPt();
    
    uint32_t instructionCount = 0;
    for (auto it = block->getFirstInsertionPt(); it != block->end(); it++)
    {
        instructionCount++;
    }

    if (randomPos)
    {
        uint32_t t = generator->DrawRange(0u, (uint32_t)std::max(int(instructionCount) - 1, 0));
        std::advance(insertPoint, t);
    }

    if (insertPoint == block->end())
    {
        return nullptr;
    }

    llvm::Function* templateFunction;
    switch (antiDebugType)
    {
        case AntiDebugType::blacklist:
            templateFunction = templates.blacklistFunction;
            break;
        case AntiDebugType::pid:
            templateFunction = templates.pidFunction;
            break;
    }

    llvm::LLVMContext& context = function->getContext();
    llvm::Type* i32Type = llvm::Type::getInt32Ty(context);

    llvm::BasicBlock* newSplitBlock = block->splitBasicBlock(insertPoint);
    block->getTerminator()->eraseFromParent();

    llvm::IRBuilder<> originalBlockBuilder(block, block->end());
    llvm::CallInst* isDebuggingOn = originalBlockBuilder.CreateCall(templateFunction);

    bool usePoisoning = attributes.antiAnalysisDelayedPoisoning && (generator->DrawRange(1u, 100u) <= attributes.antiAnalysisPoisonProbability);
    if (usePoisoning)
    {
        llvm::Value* detectedCondition = originalBlockBuilder.CreateICmpNE(isDebuggingOn, llvm::ConstantInt::get(i32Type, 0));
        bool localSuccess = false;
        if (generator->DrawRange(1u, 100u) <= attributes.antiAnalysisLocalPoisonProbability)
        {
            DominatingPair inputPair = FindUsableInputPair(block, block->end(), generator);
            localSuccess = EmitLocalStatePoisoning(originalBlockBuilder, block, newSplitBlock, detectedCondition, inputPair, generator);
        }
        if (!localSuccess)
        {
            EmitRingStatePoisoning(originalBlockBuilder, *function->getParent(), detectedCondition, generator);
        }
        originalBlockBuilder.CreateBr(newSplitBlock);
    }
    else
    {
        llvm::Value* normalCondition = originalBlockBuilder.CreateICmpEQ(isDebuggingOn, llvm::ConstantInt::get(i32Type, 0));
        originalBlockBuilder.CreateCondBr(normalCondition, newSplitBlock, bogusBlock);
        llvm::IRBuilder<> bogusBlockBuilder(bogusBlock, bogusBlock->end());
        bogusBlockBuilder.CreateBr(newSplitBlock);
    }

    llvm::InlineFunctionInfo ifi;
    llvm::InlineResult res = llvm::InlineFunction(*isDebuggingOn, ifi);
    if (!res.isSuccess())
    {
        llvm::errs() << "WARNING: failed to inline anti debug helper: " << res.getFailureReason() << "\n";
        exit(1);
    }

    return newSplitBlock;
}

LeetObfuscator::AntiAnalysisPass::DominatingPair LeetObfuscator::AntiAnalysisPass::FindUsableInputPair(llvm::BasicBlock* block, llvm::BasicBlock::iterator insertIt, std::shared_ptr<RandomNumberGenerator> generator)
{
    DominatingPair pair;
    std::vector<llvm::Value*> candidates;

    auto consider = [&](llvm::Value* value)
    {
        if (!value || llvm::isa<llvm::PHINode>(value) || llvm::isa<llvm::AllocaInst>(value))
        {
            return;
        }

        int r = RankValue(value);
        if (r < 0)
        {
            return;
        }

        if (std::find(candidates.begin(), candidates.end(), value) == candidates.end())
        {
            candidates.push_back(value);
        }
    };

    uint32_t stepCount = 0;
    auto it = insertIt;
    while (it != block->begin())
    {
        --it;
        llvm::Instruction* inst = &*it;
        consider(inst);
        stepCount++;
        if (stepCount >= 15)
        {
            break;
        }
    }

    llvm::Function* function = block->getParent();
    for (llvm::Argument& Arg : function->args())
    {
        consider(&Arg);
    }

    std::sort(candidates.begin(), candidates.end(), [](llvm::Value* a, llvm::Value* b)
    {
        return RankValue(a) < RankValue(b);
    });

    if (!candidates.empty())
    {
        pair.first = candidates[0];

        if (candidates.size() > 1)
        {
            int bestRank = RankValue(candidates[0]);
            std::vector<llvm::Value*> sameRankCandidates;
            for (size_t i = 1; i < candidates.size(); i++)
            {
                if (RankValue(candidates[i]) == bestRank)
                {
                    sameRankCandidates.push_back(candidates[i]);
                }
            }

            if (!sameRankCandidates.empty())
            {
                uint32_t chosenIdx = generator->DrawRange(0u, (uint32_t)sameRankCandidates.size() - 1);
                pair.second = sameRankCandidates[chosenIdx];
            }
            else
            {
                pair.second = candidates[1];
            }
        }
        else
        {
            pair.second = nullptr;
        }
    }
    else
    {
        pair.first = nullptr;
        pair.second = nullptr;
    }

    return pair;
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

void LeetObfuscator::AntiDissasemblyEmitter::encodeInstruction(const llvm::MCInst& instruction, llvm::SmallVectorImpl<char>& bytes, llvm::SmallVectorImpl<llvm::MCFixup>& fixups, const llvm::MCSubtargetInfo& sti) const
{
    static std::shared_ptr<SettingsParser::GlobalAttributes> globalSettings = SettingsParser::ParseGlobalAttributes();
    static bool antiAnalysisEnabled = false;
    static bool alreadySearched = false;
    if (!antiAnalysisEnabled && !alreadySearched)
    {
        for (auto& pass : globalSettings->passes)
        {
            if (pass.type == SettingsParser::PassType::AntiAnalysisPass)
            {
                antiAnalysisEnabled = true;
                llvm::errs() << "Running AntiAnalysisCodeEmitterPass\n";
                break;
            }
        }
    }
    alreadySearched = true;

    llvm::SmallVector<char, 16> tmp;
    llvm::SmallVector<llvm::MCFixup, 4> tmpFixups;
    m_Real->encodeInstruction(instruction, tmp, tmpFixups, sti);

    if (antiAnalysisEnabled)
    {
        if (!tmp.empty() && (uint8_t)tmp[0] == 0xFF)
        {
            bytes.push_back((char)0xEB);
            for (auto &F : tmpFixups)
            {
                F.setOffset(F.getOffset() + 1);
            }
        }
    }
    bytes.append(tmp.begin(), tmp.end());
    fixups.append(tmpFixups.begin(), tmpFixups.end());
}