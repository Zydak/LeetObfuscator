#include "AntiAliasingPass.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

#include "SettingsParser.h"

#include <vector>
#include <map>
#include <algorithm>

bool TryGetAllocaTotalSize(llvm::AllocaInst* allocaInst, const llvm::DataLayout& dataLayout, uint64_t& outSize)
{
    if (!allocaInst->isStaticAlloca())
    {
        return false;
    }

    llvm::Type* allocatedType = allocaInst->getAllocatedType();
    if (!allocatedType->isSized())
    {
        return false;
    }

    llvm::TypeSize elemSize = dataLayout.getTypeAllocSize(allocatedType);
    if (elemSize.isScalable())
    {
        return false;
    }

    auto* arraySizeCint = llvm::dyn_cast<llvm::ConstantInt>(allocaInst->getArraySize());
    if (!arraySizeCint)
    {
        return false;
    }

    uint64_t arraySize = arraySizeCint->getZExtValue();
    if (arraySize == 0)
    {
        return false;
    }

    outSize = elemSize.getFixedValue() * arraySize;
    return true;
}

bool IsEligibleAlloca(llvm::AllocaInst* allocaInst, const llvm::DataLayout& dataLayout, const LeetObfuscator::SettingsParser::FunctionAttributes& attributes)
{
    if (allocaInst->getName().starts_with("leet."))
    {
        return false;
    }

    uint64_t fixedSize = 0;
    if (!TryGetAllocaTotalSize(allocaInst, dataLayout, fixedSize))
    {
        return false;
    }

    if (fixedSize < attributes.antiAliasingMinCandidateSize)
    {
        return false;
    }

    if (attributes.antiAliasingMaxCandidateSize != 0 && fixedSize > attributes.antiAliasingMaxCandidateSize)
    {
        return false;
    }

    return true;
}

void StripDebugAndLifetimeUsers(llvm::AllocaInst* allocaInst)
{
    std::vector<llvm::Instruction*> toErase;
    for (llvm::User* user : allocaInst->users())
    {
        if (auto* intrinsicInst = llvm::dyn_cast<llvm::IntrinsicInst>(user))
        {
            llvm::Intrinsic::ID intrinsicId = intrinsicInst->getIntrinsicID();
            if (intrinsicId == llvm::Intrinsic::lifetime_start ||
                intrinsicId == llvm::Intrinsic::lifetime_end ||
                intrinsicId == llvm::Intrinsic::dbg_declare ||
                intrinsicId == llvm::Intrinsic::dbg_value ||
                intrinsicId == llvm::Intrinsic::dbg_assign ||
                intrinsicId == llvm::Intrinsic::dbg_label)
            {
                toErase.push_back(intrinsicInst);
            }
        }
    }
    for (llvm::Instruction* inst : toErase)
    {
        inst->eraseFromParent();
    }
}

llvm::PreservedAnalyses LeetObfuscator::AntiAliasingPass::run(llvm::Module& module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running AntiAliasingPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    m_Statistics = PassStatistics();

    for (llvm::Function& function : module)
    {
        if (function.isDeclaration())
        {
            continue;
        }

        ObfuscateFunction(function);
    }

    m_Logger.LogModule(module, "Summary: Obfuscated " + std::to_string(m_Statistics.obfuscatedFunctions) + " functions, replaced " + std::to_string(m_Statistics.replacedAllocas) + " allocas, patched " + std::to_string(m_Statistics.patchedUses) + " uses (" + std::to_string(m_Statistics.freshCalculations) + " fresh address computations, " + std::to_string(m_Statistics.reusedCalculations) + " reused computations)", 0);

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::AntiAliasingPass::ObfuscateFunction(llvm::Function& function)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(
        function, SettingsParser::PassType::AntiAliasingPass, m_Arguments
    );

    if (SettingsParser::ShouldSkipFunction(&function, attributes))
    {
        m_Logger.LogFunction(function, "Skipping function due to settings", 1);
        return;
    }

    m_Logger.LogFunction(function, "Processing function", 1);

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    if (generator->DrawRange(1u, 100u) > attributes.antiAliasingProbability)
    {
        m_Logger.LogFunction(function, "Skipping function due to probability roll", 2);
        return;
    }

    llvm::BasicBlock& entryBlock = function.getEntryBlock();
    llvm::Module* module = function.getParent();
    const llvm::DataLayout& dataLayout = module->getDataLayout();

    uint32_t totalAllocasFound = 0;
    std::vector<llvm::AllocaInst*> candidates;
    for (llvm::Instruction& inst : entryBlock)
    {
        if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(&inst))
        {
            totalAllocasFound++;
            if (IsEligibleAlloca(allocaInst, dataLayout, attributes))
            {
                candidates.push_back(allocaInst);
            }
        }
    }

    uint32_t minCandidatesNeeded = std::max(attributes.antiAliasingMinCandidates, 1u);
    if (candidates.size() < minCandidatesNeeded)
    {
        m_Logger.LogFunction(function, "Not enough stack slots to obfuscate: found " + std::to_string(candidates.size()) + " eligible out of " + std::to_string(totalAllocasFound) + " allocas (minimum required: " + std::to_string(minCandidatesNeeded) + ")", 2);
        return;
    }

    m_Logger.LogFunction(function, "Found " + std::to_string(candidates.size()) + " eligible stack slots out of " + std::to_string(totalAllocasFound) + " allocas", 2);

    generator->Shuffle(candidates.begin(), candidates.end());

    uint32_t initialCandidateCount = (uint32_t)candidates.size();
    if (attributes.antiAliasingMaxCandidates != 0 && candidates.size() > attributes.antiAliasingMaxCandidates)
    {
        candidates.resize(attributes.antiAliasingMaxCandidates);
        m_Logger.LogFunction(function, "Capped candidates from " + std::to_string(initialCandidateCount) + " to " + std::to_string(candidates.size()) + " (maxCandidates limit: " + std::to_string(attributes.antiAliasingMaxCandidates) + ")", 2);
    }

    uint32_t numSlots = (uint32_t)candidates.size();

    // Calculate byte offsets with random decoy padding
    uint64_t currentOffset = (uint64_t)generator->DrawRange(0u, 16u);
    llvm::Align maxAlign(1);

    std::vector<uint64_t> candidateOffsets(numSlots, 0);
    for (uint32_t i = 0; i < numSlots; i++)
    {
        llvm::AllocaInst* allocaInst = candidates[i];
        uint64_t size = 0;
        TryGetAllocaTotalSize(allocaInst, dataLayout, size);
        llvm::Align typeAlign = dataLayout.getPrefTypeAlign(allocaInst->getAllocatedType());
        llvm::Align reqAlign = std::max(allocaInst->getAlign(), typeAlign);
        maxAlign = std::max(maxAlign, reqAlign);

        currentOffset = llvm::alignTo(currentOffset, reqAlign.value());
        candidateOffsets[i] = currentOffset;
        currentOffset += size;

        uint64_t decoyPadding = (uint64_t)generator->DrawRange(4u, 16u);
        currentOffset += decoyPadding;
    }

    uint64_t totalBytes = llvm::alignTo(currentOffset, maxAlign.value());
    if (totalBytes == 0)
    {
        totalBytes = 16;
    }

    // Expanded table size with multi slot variable aliasing and decoy entries, rounded up to power of two
    uint32_t slotsPerCandidate = attributes.antiAliasingSlotsPerCandidate;
    if (slotsPerCandidate == 0)
    {
        slotsPerCandidate = 1;
    }

    uint32_t neededSlots = numSlots * slotsPerCandidate + 4;
    uint32_t rawTableSize = std::max(neededSlots, 16u);
    uint32_t tableSize = 16;
    while (tableSize < rawTableSize)
    {
        tableSize <<= 1;
    }

    if (attributes.antiAliasingMaxTableSize != 0 && tableSize > attributes.antiAliasingMaxTableSize)
    {
        uint32_t powerOfTwo = 16;
        while ((powerOfTwo << 1) <= attributes.antiAliasingMaxTableSize)
        {
            powerOfTwo <<= 1;
        }
        tableSize = powerOfTwo;

        uint32_t preAdjustSlots = numSlots;
        while (numSlots > 1 && (numSlots * slotsPerCandidate >= tableSize))
        {
            numSlots--;
        }
        candidates.resize(numSlots);
        candidateOffsets.resize(numSlots);
        if (numSlots < preAdjustSlots)
        {
            m_Logger.LogFunction(function, "Adjusted candidates from " + std::to_string(preAdjustSlots) + " to " + std::to_string(numSlots) + " to fit maxTableSize " + std::to_string(tableSize), 2);
        }
    }
    uint32_t jitterMask = tableSize - 1;

    std::vector<uint32_t> availableSlots(tableSize);
    for (uint32_t i = 0; i < tableSize; i++)
    {
        availableSlots[i] = i;
    }
    generator->Shuffle(availableSlots.begin(), availableSlots.end());

    std::vector<std::vector<uint32_t>> candidateSlotPool(numSlots);
    uint32_t slotCursor = 0;
    for (uint32_t i = 0; i < numSlots; i++)
    {
        for (uint32_t k = 0; k < slotsPerCandidate; k++)
        {
            candidateSlotPool[i].push_back(availableSlots[slotCursor++]);
        }
    }

    std::vector<uint32_t> tableValues(tableSize, 0);
    for (uint32_t i = 0; i < numSlots; i++)
    {
        for (uint32_t slot : candidateSlotPool[i])
        {
            tableValues[slot] = (uint32_t)candidateOffsets[i];
        }
    }

    std::vector<uint32_t> decoySlots;
    for (uint32_t i = slotCursor; i < tableSize; i++)
    {
        uint32_t decoySlot = availableSlots[i];
        decoySlots.push_back(decoySlot);
        uint32_t dummyOffset = (uint32_t)generator->DrawRange(0u, (uint32_t)totalBytes - 1);
        tableValues[decoySlot] = dummyOffset;
    }

    std::vector<uint32_t> slotSalts(tableSize);
    std::vector<uint32_t> slotXorSalts(tableSize);
    for (uint32_t i = 0; i < tableSize; i++)
    {
        slotSalts[i] = generator->DrawRange(0x10000000u, 0xEFFFFFFFu);
        slotXorSalts[i] = generator->DrawRange(0x10000000u, 0xEFFFFFFFu);
    }

    uint32_t totalCandidateSlots = numSlots * slotsPerCandidate;
    uint32_t decoyCount = (uint32_t)decoySlots.size();

    m_Logger.LogFunction(function, "Allocating offset buffer: size " + std::to_string(totalBytes) + " bytes, alignment " + std::to_string(maxAlign.value()) + " bytes for " + std::to_string(numSlots) + " candidates", 2);

    m_Logger.LogFunction(function, "Allocating permutation table of size " + std::to_string(tableSize) + " with " + std::to_string(numSlots) + " candidates (" + std::to_string(totalCandidateSlots) + " candidate slots, " + std::to_string(slotsPerCandidate) + " slots per candidate) and " + std::to_string(decoyCount) + " decoys (jitter mask " + std::to_string(jitterMask) + ")", 2);

    for (uint32_t i = 0; i < numSlots; i++)
    {
        llvm::AllocaInst* candidateAlloca = candidates[i];
        uint64_t candidateByteSize = 0;
        TryGetAllocaTotalSize(candidateAlloca, dataLayout, candidateByteSize);
        std::string variableName = candidateAlloca->getName().empty() ? ("unnamed." + std::to_string(i)) : candidateAlloca->getName().str();

        std::string slotsString = "[";
        for (size_t slotIndex = 0; slotIndex < candidateSlotPool[i].size(); slotIndex++)
        {
            if (slotIndex > 0)
            {
                slotsString += ", ";
            }
            slotsString += std::to_string(candidateSlotPool[i][slotIndex]);
        }
        slotsString += "]";

        m_Logger.Log("Candidate " + std::to_string(i) + " '" + variableName + "': size " + std::to_string(candidateByteSize) + " bytes, buffer offset " + std::to_string(candidateOffsets[i]) + ", perm table slots " + slotsString, 3);
    }

    std::string decoySlotsString = "[";
    for (size_t decoyIndex = 0; decoyIndex < decoySlots.size(); decoyIndex++)
    {
        if (decoyIndex > 0)
        {
            decoySlotsString += ", ";
        }
        decoySlotsString += std::to_string(decoySlots[decoyIndex]);
    }
    decoySlotsString += "]";

    m_Logger.Log("Decoy slots (" + std::to_string(decoyCount) + " total): " + decoySlotsString, 3);

    llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.begin());

    llvm::ArrayType* bufferType = llvm::ArrayType::get(entryBuilder.getInt8Ty(), totalBytes);
    llvm::AllocaInst* leetBuffer = entryBuilder.CreateAlloca(bufferType, nullptr, "leet.buf");
    leetBuffer->setAlignment(maxAlign);

    llvm::ArrayType* permutationTableType = llvm::ArrayType::get(entryBuilder.getInt32Ty(), tableSize);
    llvm::AllocaInst* leetPermTable = entryBuilder.CreateAlloca(permutationTableType, nullptr, "leet.perm");
    leetPermTable->setAlignment(llvm::Align(4));

    // Dynamic stack entropy mask and jitter
    llvm::Value* ptrInt = entryBuilder.CreatePtrToInt(leetBuffer, entryBuilder.getInt64Ty(), "leet.buf.ptr.int");
    llvm::Value* ptrShift = entryBuilder.CreateLShr(ptrInt, entryBuilder.getInt64(4), "leet.buf.entropy");
    llvm::Value* ptrTrunc = entryBuilder.CreateTrunc(ptrShift, entryBuilder.getInt32Ty(), "leet.buf.entropy.32");
    llvm::Value* slotJitter = entryBuilder.CreateAnd(ptrTrunc, entryBuilder.getInt32(jitterMask), "leet.slot.jitter");

    auto SynthesizeShiftedSlot = [&](llvm::IRBuilder<>& builder, uint32_t slotIndex) -> llvm::Value*
    {
        uint32_t synthesisChoice = generator->DrawRange(0u, 3u);
        uint32_t randomMultiple = generator->DrawRange(1u, 15u);
        uint32_t expandedTarget = slotIndex + (randomMultiple * tableSize);

        llvm::Value* actualSlot = nullptr;
        if (synthesisChoice == 0)
        {
            uint32_t randomSplit = generator->DrawRange(1u, 0x7FFFFFFFu);
            uint32_t remainderSplit = expandedTarget - randomSplit;

            llvm::Value* stageOne = builder.CreateAdd(slotJitter, builder.getInt32(randomSplit), "leet.slot.stage1");
            llvm::Value* maskedStageOne = builder.CreateAnd(stageOne, builder.getInt32(jitterMask), "leet.slot.masked1");
            llvm::Value* stageTwo = builder.CreateAdd(maskedStageOne, builder.getInt32(remainderSplit), "leet.slot.stage2");
            actualSlot = builder.CreateAnd(stageTwo, builder.getInt32(jitterMask), "leet.target.actual.slot");
        }
        else if (synthesisChoice == 1)
        {
            uint32_t randomSplit = generator->DrawRange(1u, 0x7FFFFFFFu);
            uint32_t remainderSplit = expandedTarget + randomSplit;

            llvm::Value* stageOne = builder.CreateSub(slotJitter, builder.getInt32(randomSplit), "leet.slot.stage1");
            llvm::Value* maskedStageOne = builder.CreateAnd(stageOne, builder.getInt32(jitterMask), "leet.slot.masked1");
            llvm::Value* stageTwo = builder.CreateAdd(maskedStageOne, builder.getInt32(remainderSplit), "leet.slot.stage2");
            actualSlot = builder.CreateAnd(stageTwo, builder.getInt32(jitterMask), "leet.target.actual.slot");
        }
        else if (synthesisChoice == 2)
        {
            llvm::Value* expandedVal = builder.getInt32(expandedTarget);
            llvm::Value* xorVal = builder.CreateXor(slotJitter, expandedVal, "leet.slot.xor");
            llvm::Value* andVal = builder.CreateAnd(slotJitter, expandedVal, "leet.slot.and");
            llvm::Value* shlVal = builder.CreateShl(andVal, builder.getInt32(1), "leet.slot.shl");
            llvm::Value* sumVal = builder.CreateAdd(xorVal, shlVal, "leet.slot.sum");
            actualSlot = builder.CreateAnd(sumVal, builder.getInt32(jitterMask), "leet.target.actual.slot");
        }
        else
        {
            llvm::Value* expandedVal = builder.getInt32(expandedTarget);
            llvm::Value* orVal = builder.CreateOr(slotJitter, expandedVal, "leet.slot.or");
            llvm::Value* andVal = builder.CreateAnd(slotJitter, expandedVal, "leet.slot.and");
            llvm::Value* sumVal = builder.CreateAdd(orVal, andVal, "leet.slot.sum");
            actualSlot = builder.CreateAnd(sumVal, builder.getInt32(jitterMask), "leet.target.actual.slot");
        }

        return actualSlot;
    };

    // Initialize the expanded masked table in the entry block
    for (uint32_t s = 0; s < tableSize; s++)
    {
        uint32_t slotConstant = (s * 0x9E3779B9u) + slotSalts[s];
        llvm::Value* slotKey = entryBuilder.CreateAdd(ptrTrunc, entryBuilder.getInt32(slotConstant), "leet.slot.key");
        llvm::Value* withTableVal = entryBuilder.CreateAdd(slotKey, entryBuilder.getInt32(tableValues[s]), "leet.with.val");
        llvm::Value* maskedVal = entryBuilder.CreateXor(withTableVal, entryBuilder.getInt32(slotXorSalts[s]), "leet.table.init.val");

        //llvm::Value* sVal = entryBuilder.getInt32(s);
        //llvm::Value* shiftedSlot = entryBuilder.CreateAdd(sVal, slotJitter, "leet.shifted.slot");
        //llvm::Value* actualSlot = entryBuilder.CreateAnd(shiftedSlot, entryBuilder.getInt32(jitterMask), "leet.actual.slot");
        llvm::Value* actualSlot = SynthesizeShiftedSlot(entryBuilder, s);

        llvm::Value* slotPtr = entryBuilder.CreateInBoundsGEP(
            permutationTableType, leetPermTable,
            {entryBuilder.getInt32(0), actualSlot}, "leet.table.init.ptr"
        );
        entryBuilder.CreateStore(maskedVal, slotPtr);
    }

    llvm::Value* bufferBase = entryBuilder.CreateInBoundsGEP(
        bufferType, leetBuffer,
        {entryBuilder.getInt32(0), entryBuilder.getInt32(0)}, "leet.buf.base"
    );

    uint32_t functionPatchedUses = 0;
    uint32_t functionFreshAddresses = 0;
    uint32_t functionReusedAddresses = 0;

    for (uint32_t canonicalSlot = 0; canonicalSlot < numSlots; canonicalSlot++)
    {
        llvm::AllocaInst* allocaInst = candidates[canonicalSlot];

        StripDebugAndLifetimeUsers(allocaInst);
        if (allocaInst->use_empty())
        {
            m_Logger.Log("Candidate " + std::to_string(canonicalSlot) + " has no active uses, erased", 3);
            allocaInst->eraseFromParent();
            continue;
        }

        std::vector<llvm::Use*> usesToPatch;
        for (llvm::Use& use : allocaInst->uses())
        {
            usesToPatch.push_back(&use);
        }
        // Group uses by insertion basic block
        std::map<llvm::BasicBlock*, std::vector<llvm::Use*>> usesByBlock;
        for (llvm::Use* use : usesToPatch)
        {
            llvm::Instruction* userInst = llvm::dyn_cast<llvm::Instruction>(use->getUser());
            if (!userInst)
            {
                continue;
            }

            llvm::BasicBlock* targetBlock = nullptr;
            if (auto* phi = llvm::dyn_cast<llvm::PHINode>(userInst))
            {
                targetBlock = phi->getIncomingBlock(*use);
            }
            else
            {
                targetBlock = userInst->getParent();
            }
            usesByBlock[targetBlock].push_back(use);
        }

        auto GetInsertionInstruction = [](llvm::Use* use) -> llvm::Instruction*
        {
            auto* userInst = llvm::dyn_cast<llvm::Instruction>(use->getUser());
            if (auto* phi = llvm::dyn_cast<llvm::PHINode>(userInst))
            {
                llvm::BasicBlock* incomingBlock = phi->getIncomingBlock(*use);
                return incomingBlock->getTerminator();
            }
            return userInst;
        };

        auto DecodeOffset = [&](llvm::IRBuilder<>& builder, uint32_t targetSlotIndex) -> llvm::Value*
        {
            // llvm::Value* targetSlotConst = builder.getInt32(targetSlotIndex);
            // llvm::Value* shiftedSlot = builder.CreateAdd(targetSlotConst, slotJitter, "leet.target.shifted.slot");
            // llvm::Value* actualSlot = builder.CreateAnd(shiftedSlot, builder.getInt32(jitterMask), "leet.target.actual.slot");
            llvm::Value* actualSlot = SynthesizeShiftedSlot(builder, targetSlotIndex);

            llvm::Value* permSlotPtr = builder.CreateInBoundsGEP(
                permutationTableType,
                leetPermTable,
                {builder.getInt32(0), actualSlot},
                "leet.permslot.ptr"
            );
            llvm::Value* maskedVal = builder.CreateLoad(builder.getInt32Ty(), permSlotPtr, "leet.masked.offset");

            uint32_t slotConstant = (targetSlotIndex * 0x9E3779B9u) + slotSalts[targetSlotIndex];
            llvm::Value* slotKey = builder.CreateAdd(ptrTrunc, builder.getInt32(slotConstant), "leet.decode.slot.key");

            bool applyMBA = (generator->DrawRange(1u, 100u) <= attributes.antiAliasingMBAProbability);

            llvm::Value* stepOne = nullptr;
            llvm::Value* slotXorSaltVal = builder.getInt32(slotXorSalts[targetSlotIndex]);

            if (applyMBA)
            {
                uint32_t xorChoice = generator->DrawRange(0u, 2u);
                if (xorChoice == 0)
                {
                    llvm::Value* orVal = builder.CreateOr(maskedVal, slotXorSaltVal, "leet.mba.xor.or");
                    llvm::Value* andVal = builder.CreateAnd(maskedVal, slotXorSaltVal, "leet.mba.xor.and");
                    stepOne = builder.CreateSub(orVal, andVal, "leet.mba.xor.res");
                }
                else if (xorChoice == 1)
                {
                    llvm::Value* notB = builder.CreateNot(slotXorSaltVal, "leet.mba.not.b");
                    llvm::Value* notA = builder.CreateNot(maskedVal, "leet.mba.not.a");
                    llvm::Value* aAndNotB = builder.CreateAnd(maskedVal, notB, "leet.mba.a.notb");
                    llvm::Value* notAAndB = builder.CreateAnd(notA, slotXorSaltVal, "leet.mba.nota.b");
                    stepOne = builder.CreateOr(aAndNotB, notAAndB, "leet.mba.xor.res");
                }
                else
                {
                    llvm::Value* addVal = builder.CreateAdd(maskedVal, slotXorSaltVal, "leet.mba.xor.add");
                    llvm::Value* andVal = builder.CreateAnd(maskedVal, slotXorSaltVal, "leet.mba.xor.and");
                    llvm::Value* mulVal = builder.CreateMul(andVal, builder.getInt32(2), "leet.mba.xor.mul");
                    stepOne = builder.CreateSub(addVal, mulVal, "leet.mba.xor.res");
                }
            }
            else
            {
                stepOne = builder.CreateXor(maskedVal, slotXorSaltVal, "leet.real.offset.step1");
            }

            llvm::Value* realByteOffset = nullptr;
            if (applyMBA)
            {
                uint32_t subChoice = generator->DrawRange(0u, 2u);
                if (subChoice == 0)
                {
                    llvm::Value* notB = builder.CreateNot(slotKey, "leet.mba.sub.notb");
                    llvm::Value* notBPlusOne = builder.CreateAdd(notB, builder.getInt32(1), "leet.mba.sub.neg");
                    realByteOffset = builder.CreateAdd(stepOne, notBPlusOne, "leet.mba.sub.res");
                }
                else if (subChoice == 1)
                {
                    llvm::Value* xorVal = builder.CreateXor(stepOne, slotKey, "leet.mba.sub.xor");
                    llvm::Value* notA = builder.CreateNot(stepOne, "leet.mba.sub.nota");
                    llvm::Value* andVal = builder.CreateAnd(notA, slotKey, "leet.mba.sub.and");
                    llvm::Value* mulVal = builder.CreateMul(andVal, builder.getInt32(2), "leet.mba.sub.mul");
                    realByteOffset = builder.CreateSub(xorVal, mulVal, "leet.mba.sub.res");
                }
                else
                {
                    llvm::Value* notA = builder.CreateNot(stepOne, "leet.mba.sub.nota");
                    llvm::Value* notAPlusB = builder.CreateAdd(notA, slotKey, "leet.mba.sub.add");
                    realByteOffset = builder.CreateNot(notAPlusB, "leet.mba.sub.res");
                }
            }
            else
            {
                realByteOffset = builder.CreateSub(stepOne, slotKey, "leet.real.offset");
            }

            return realByteOffset;
        };

        auto ComputeFreshAddress = [&](llvm::IRBuilder<>& builder) -> llvm::Value*
        {
            uint32_t slotPick = (uint32_t)generator->DrawRange(0u, (uint32_t)candidateSlotPool[canonicalSlot].size() - 1);
            uint32_t chosenSlotIndex = candidateSlotPool[canonicalSlot][slotPick];

            // Real address computation with dynamic unmasking
            llvm::Value* realByteOffset = DecodeOffset(builder, chosenSlotIndex);
            llvm::Value* realByteOffset64 = builder.CreateZExt(realByteOffset, builder.getInt64Ty(), "leet.real.offset.64");
            llvm::Value* realElementAddress = builder.CreateInBoundsGEP(builder.getInt8Ty(), bufferBase, realByteOffset64, "leet.real.addr");

            // Probability based decision to apply opaque predicate
            if (generator->DrawRange(1u, 100u) > attributes.antiAliasingOpaqueProbability)
            {
                return realElementAddress;
            }

            // Decoy address computation to create static alias ambiguity
            uint32_t decoySlot = (canonicalSlot + 1) % numSlots;
            uint32_t decoySlotPick = (uint32_t)generator->DrawRange(0u, (uint32_t)candidateSlotPool[decoySlot].size() - 1);
            uint32_t chosenDecoySlotIndex = candidateSlotPool[decoySlot][decoySlotPick];

            llvm::Value* decoyByteOffset = DecodeOffset(builder, chosenDecoySlotIndex);
            llvm::Value* decoyByteOffset64 = builder.CreateZExt(decoyByteOffset, builder.getInt64Ty(), "leet.decoy.offset.64");
            llvm::Value* decoyElementAddress = builder.CreateInBoundsGEP(builder.getInt8Ty(), bufferBase, decoyByteOffset64, "leet.decoy.addr");

            // Diverse opaque predicates with alternating truth values
            uint32_t variant = generator->DrawRange(0u, 7u);
            llvm::Value* condition = nullptr;
            bool isTrueCondition = true;

            if (variant == 0)
            {
                // Variant 0 (Evaluates to True): v * (v + 1) is always even so ((v * (v + 1)) & 1) == 0
                llvm::Value* vPlus1 = builder.CreateAdd(realByteOffset, builder.getInt32(1), "leet.v.plus1");
                llvm::Value* vProd = builder.CreateMul(realByteOffset, vPlus1, "leet.v.prod");
                llvm::Value* vLsb = builder.CreateAnd(vProd, builder.getInt32(1), "leet.v.lsb");
                condition = builder.CreateICmpEQ(vLsb, builder.getInt32(0), "leet.opaque.cond");
                isTrueCondition = true;
            }
            else if (variant == 1)
            {
                // Variant 1 (Evaluates to False): v * (v + 1) is always even so ((v * (v + 1)) & 1) == 1 is never true
                llvm::Value* vPlus1 = builder.CreateAdd(realByteOffset, builder.getInt32(1), "leet.v.plus1");
                llvm::Value* vProd = builder.CreateMul(realByteOffset, vPlus1, "leet.v.prod");
                llvm::Value* vLsb = builder.CreateAnd(vProd, builder.getInt32(1), "leet.v.lsb");
                condition = builder.CreateICmpEQ(vLsb, builder.getInt32(1), "leet.opaque.cond");
                isTrueCondition = false;
            }
            else if (variant == 2)
            {
                // Variant 2 (Evaluates to True): (v ^ (v + 1)) has LSB 1 so ((v ^ (v + 1)) & 1) == 1
                llvm::Value* vPlus1 = builder.CreateAdd(realByteOffset, builder.getInt32(1), "leet.v.plus1");
                llvm::Value* vXor = builder.CreateXor(realByteOffset, vPlus1, "leet.v.xor");
                llvm::Value* vLsb = builder.CreateAnd(vXor, builder.getInt32(1), "leet.v.lsb");
                condition = builder.CreateICmpEQ(vLsb, builder.getInt32(1), "leet.opaque.cond");
                isTrueCondition = true;
            }
            else if (variant == 3)
            {
                // Variant 3 (Evaluates to False): (v ^ (v + 1)) has LSB 1 so ((v ^ (v + 1)) & 1) == 0 is never true
                llvm::Value* vPlus1 = builder.CreateAdd(realByteOffset, builder.getInt32(1), "leet.v.plus1");
                llvm::Value* vXor = builder.CreateXor(realByteOffset, vPlus1, "leet.v.xor");
                llvm::Value* vLsb = builder.CreateAnd(vXor, builder.getInt32(1), "leet.v.lsb");
                condition = builder.CreateICmpEQ(vLsb, builder.getInt32(0), "leet.opaque.cond");
                isTrueCondition = false;
            }
            else if (variant == 4)
            {
                // Variant 4 (Evaluates to True): (v | 1) != 0 is always true
                llvm::Value* vOr = builder.CreateOr(realByteOffset, builder.getInt32(1), "leet.v.or");
                condition = builder.CreateICmpNE(vOr, builder.getInt32(0), "leet.opaque.cond");
                isTrueCondition = true;
            }
            else if (variant == 5)
            {
                // Variant 5 (Evaluates to False): (v | 1) == 0 is never true
                llvm::Value* vOr = builder.CreateOr(realByteOffset, builder.getInt32(1), "leet.v.or");
                condition = builder.CreateICmpEQ(vOr, builder.getInt32(0), "leet.opaque.cond");
                isTrueCondition = false;
            }
            else if (variant == 6)
            {
                // Variant 6 (Evaluates to True): (v & ~v) == 0 is always true
                llvm::Value* notV = builder.CreateXor(realByteOffset, builder.getInt32(0xFFFFFFFFu), "leet.v.not");
                llvm::Value* vAnd = builder.CreateAnd(realByteOffset, notV, "leet.v.and");
                condition = builder.CreateICmpEQ(vAnd, builder.getInt32(0), "leet.opaque.cond");
                isTrueCondition = true;
            }
            else
            {
                // Variant 7 (Evaluates to False): (v & ~v) != 0 is never true
                llvm::Value* notV = builder.CreateXor(realByteOffset, builder.getInt32(0xFFFFFFFFu), "leet.v.not");
                llvm::Value* vAnd = builder.CreateAnd(realByteOffset, notV, "leet.v.and");
                condition = builder.CreateICmpNE(vAnd, builder.getInt32(0), "leet.opaque.cond");
                isTrueCondition = false;
            }

            llvm::Value* trueVal = isTrueCondition ? realElementAddress : decoyElementAddress;
            llvm::Value* falseVal = isTrueCondition ? decoyElementAddress : realElementAddress;

            return builder.CreateSelect(condition, trueVal, falseVal, "leet.chosen.addr");
        };

        for (auto& pair : usesByBlock)
        {
            std::vector<llvm::Use*>& blockUses = pair.second;

            // Sort uses in program order within this basic block
            std::sort(blockUses.begin(), blockUses.end(), [&](llvm::Use* useA, llvm::Use* useB)
            {
                llvm::Instruction* instA = GetInsertionInstruction(useA);
                llvm::Instruction* instB = GetInsertionInstruction(useB);
                if (instA == instB)
                {
                    return useA < useB;
                }
                return instA->comesBefore(instB);
            });

            llvm::Value* currentAddress = nullptr;
            for (llvm::Use* use : blockUses)
            {
                llvm::Instruction* insertInst = GetInsertionInstruction(use);
                llvm::IRBuilder<> builder(insertInst);

                bool shouldReuse = (currentAddress != nullptr) && (generator->DrawRange(1u, 100u) <= attributes.antiAliasingReuseProbability);

                if (shouldReuse)
                {
                    functionReusedAddresses++;
                    use->set(currentAddress);
                }
                else
                {
                    functionFreshAddresses++;
                    currentAddress = ComputeFreshAddress(builder);
                    use->set(currentAddress);
                }
                functionPatchedUses++;
            }
        }

        allocaInst->eraseFromParent();
    }

    m_Logger.LogFunction(function, "Finished obfuscating function: patched " + std::to_string(functionPatchedUses) + " uses across " + std::to_string(numSlots) + " candidates (" + std::to_string(functionFreshAddresses) + " fresh address calculations, " + std::to_string(functionReusedAddresses) + " reused addresses)", 2);

    m_Statistics.obfuscatedFunctions++;
    m_Statistics.replacedAllocas += numSlots;
    m_Statistics.patchedUses += functionPatchedUses;
    m_Statistics.freshCalculations += functionFreshAddresses;
    m_Statistics.reusedCalculations += functionReusedAddresses;


    if (llvm::verifyFunction(function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] AntiAliasingPass: Function '" << function.getName() << "' verification failed after transformation!\n";
        std::error_code ec;
        llvm::raw_fd_ostream logFile("error_log.txt", ec);
        if (!ec)
        {
            function.print(logFile);
            logFile.close();
            llvm::errs() << "AntiAliasingPass: Function IR dumped to error_log.txt\n";
        }
        else
        {
            llvm::errs() << "AntiAliasingPass: Failed to open error_log.txt for writing: " << ec.message() << "\n";
        }
        exit(1);
    }
}
