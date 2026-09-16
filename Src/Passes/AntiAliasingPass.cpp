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

constexpr uint64_t s_MinCandidateSize = 1;
constexpr uint64_t s_MaxCandidateSize = 64;

constexpr uint32_t s_MinSlotsToBother = 2;

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

bool IsEligibleAlloca(llvm::AllocaInst* allocaInst, const llvm::DataLayout& dataLayout)
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

    if (fixedSize < s_MinCandidateSize || fixedSize > s_MaxCandidateSize)
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

    for (llvm::Function& function : module)
    {
        if (function.isDeclaration())
        {
            continue;
        }

        ObfuscateFunction(function);
    }

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

    std::vector<llvm::AllocaInst*> candidates;
    for (llvm::Instruction& inst : entryBlock)
    {
        if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(&inst))
        {
            if (IsEligibleAlloca(allocaInst, dataLayout))
            {
                candidates.push_back(allocaInst);
            }
        }
    }

    if (candidates.size() < s_MinSlotsToBother)
    {
        m_Logger.LogFunction(function, "Not enough stack slots to obfuscate", 2);
        return;
    }

    m_Logger.LogFunction(function, "Creating aliasing obfuscation state", 2);

    uint32_t numSlots = (uint32_t)candidates.size();

    generator->Shuffle(candidates.begin(), candidates.end());

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

    // Expanded table size with decoy entries
    uint32_t tableSize = std::max(numSlots * 2, 8u);

    std::vector<uint32_t> availableSlots(tableSize);
    for (uint32_t i = 0; i < tableSize; i++)
    {
        availableSlots[i] = i;
    }
    generator->Shuffle(availableSlots.begin(), availableSlots.end());

    std::vector<uint32_t> tableSlotForCandidate(numSlots);
    for (uint32_t i = 0; i < numSlots; i++)
    {
        tableSlotForCandidate[i] = availableSlots[i];
    }

    std::vector<uint32_t> tableValues(tableSize, 0);
    for (uint32_t i = 0; i < numSlots; i++)
    {
        tableValues[tableSlotForCandidate[i]] = (uint32_t)candidateOffsets[i];
    }
    for (uint32_t i = numSlots; i < tableSize; i++)
    {
        uint32_t dummyOffset = (uint32_t)generator->DrawRange(0u, (uint32_t)totalBytes - 1);
        tableValues[availableSlots[i]] = dummyOffset;
    }

    m_Logger.LogFunction(function, "Allocating obfuscation buffer and masked table", 3);

    llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.begin());

    llvm::ArrayType* bufferType = llvm::ArrayType::get(entryBuilder.getInt8Ty(), totalBytes);
    llvm::AllocaInst* leetBuffer = entryBuilder.CreateAlloca(bufferType, nullptr, "leet.buf");
    leetBuffer->setAlignment(maxAlign);

    llvm::ArrayType* permutationTableType = llvm::ArrayType::get(entryBuilder.getInt32Ty(), tableSize);
    llvm::AllocaInst* leetPermTable = entryBuilder.CreateAlloca(permutationTableType, nullptr, "leet.perm");
    leetPermTable->setAlignment(llvm::Align(4));

    // Dynamic stack mask
    llvm::Value* ptrInt = entryBuilder.CreatePtrToInt(leetBuffer, entryBuilder.getInt64Ty(), "leet.buf.ptr.int");
    llvm::Value* ptrShift = entryBuilder.CreateLShr(ptrInt, entryBuilder.getInt64(4), "leet.buf.entropy");
    llvm::Value* ptrTrunc = entryBuilder.CreateTrunc(ptrShift, entryBuilder.getInt32Ty(), "leet.buf.entropy.32");
    uint32_t compileTimeSalt = generator->DrawRange(0x10000000u, 0xEFFFFFFFu);
    llvm::Value* runtimeMask = entryBuilder.CreateXor(ptrTrunc, entryBuilder.getInt32(compileTimeSalt), "leet.runtime.mask");

    // Initialize the expanded masked table in the entry block
    for (uint32_t s = 0; s < tableSize; s++)
    {
        llvm::Value* maskedVal = entryBuilder.CreateXor(runtimeMask, entryBuilder.getInt32(tableValues[s]), "leet.table.init.val");
        llvm::Value* slotPtr = entryBuilder.CreateInBoundsGEP(
            permutationTableType, leetPermTable,
            {entryBuilder.getInt32(0), entryBuilder.getInt32(s)}, "leet.table.init.ptr"
        );
        entryBuilder.CreateStore(maskedVal, slotPtr);
    }

    llvm::Value* bufferBase = entryBuilder.CreateInBoundsGEP(
        bufferType, leetBuffer,
        {entryBuilder.getInt32(0), entryBuilder.getInt32(0)}, "leet.buf.base"
    );

    for (uint32_t canonicalSlot = 0; canonicalSlot < numSlots; canonicalSlot++)
    {
        llvm::AllocaInst* allocaInst = candidates[canonicalSlot];

        StripDebugAndLifetimeUsers(allocaInst);
        if (allocaInst->use_empty())
        {
            allocaInst->eraseFromParent();
            continue;
        }

        std::vector<llvm::Use*> usesToPatch;
        for (llvm::Use& use : allocaInst->uses())
        {
            usesToPatch.push_back(&use);
        }

        uint32_t tableSlotIndex = tableSlotForCandidate[canonicalSlot];
        uint32_t decoySlot = (canonicalSlot + 1) % numSlots;
        uint32_t decoyTableSlotIndex = tableSlotForCandidate[decoySlot];

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

        auto ComputeFreshAddress = [&](llvm::IRBuilder<>& builder) -> llvm::Value*
        {
            // Real address computation with dynamic unmasking
            llvm::Value* permSlotPtr = builder.CreateInBoundsGEP(
                permutationTableType,
                leetPermTable,
                {builder.getInt32(0), builder.getInt32(tableSlotIndex)},
                "leet.permslot.ptr"
            );
            llvm::Value* maskedOffset = builder.CreateLoad(builder.getInt32Ty(), permSlotPtr, "leet.masked.offset");
            llvm::Value* realByteOffset = builder.CreateXor(maskedOffset, runtimeMask, "leet.real.offset");
            llvm::Value* realByteOffset64 = builder.CreateZExt(realByteOffset, builder.getInt64Ty(), "leet.real.offset.64");
            llvm::Value* realElementAddress = builder.CreateInBoundsGEP(builder.getInt8Ty(), bufferBase, realByteOffset64, "leet.real.addr");

            // Probability based decision to apply opaque predicate
            if (generator->DrawRange(1u, 100u) > attributes.antiAliasingOpaqueProbability)
            {
                return realElementAddress;
            }

            // Decoy address computation to create static alias ambiguity
            llvm::Value* decoyPermSlotPtr = builder.CreateInBoundsGEP(
                permutationTableType,
                leetPermTable,
                {builder.getInt32(0), builder.getInt32(decoyTableSlotIndex)},
                "leet.decoy.permslot.ptr"
            );
            llvm::Value* decoyMaskedOffset = builder.CreateLoad(builder.getInt32Ty(), decoyPermSlotPtr, "leet.decoy.masked.offset");
            llvm::Value* decoyByteOffset = builder.CreateXor(decoyMaskedOffset, runtimeMask, "leet.decoy.offset");
            llvm::Value* decoyByteOffset64 = builder.CreateZExt(decoyByteOffset, builder.getInt64Ty(), "leet.decoy.offset.64");
            llvm::Value* decoyElementAddress = builder.CreateInBoundsGEP(builder.getInt8Ty(), bufferBase, decoyByteOffset64, "leet.decoy.addr");

            // Diverse opaque predicates with alternating truth values
            uint32_t variant = generator->DrawRange(0u, 7u);
            llvm::Value* condition = nullptr;
            bool isTrueCondition = true;

            if (variant == 0)
            {
                // Variant 0 (Evaluates to True): v * (v + 1) is always even -> ((v * (v + 1)) & 1) == 0
                llvm::Value* vPlus1 = builder.CreateAdd(realByteOffset, builder.getInt32(1), "leet.v.plus1");
                llvm::Value* vProd = builder.CreateMul(realByteOffset, vPlus1, "leet.v.prod");
                llvm::Value* vLsb = builder.CreateAnd(vProd, builder.getInt32(1), "leet.v.lsb");
                condition = builder.CreateICmpEQ(vLsb, builder.getInt32(0), "leet.opaque.cond");
                isTrueCondition = true;
            }
            else if (variant == 1)
            {
                // Variant 1 (Evaluates to False): v * (v + 1) is always even -> ((v * (v + 1)) & 1) == 1 is never true
                llvm::Value* vPlus1 = builder.CreateAdd(realByteOffset, builder.getInt32(1), "leet.v.plus1");
                llvm::Value* vProd = builder.CreateMul(realByteOffset, vPlus1, "leet.v.prod");
                llvm::Value* vLsb = builder.CreateAnd(vProd, builder.getInt32(1), "leet.v.lsb");
                condition = builder.CreateICmpEQ(vLsb, builder.getInt32(1), "leet.opaque.cond");
                isTrueCondition = false;
            }
            else if (variant == 2)
            {
                // Variant 2 (Evaluates to True): (v ^ (v + 1)) has LSB 1 -> ((v ^ (v + 1)) & 1) == 1
                llvm::Value* vPlus1 = builder.CreateAdd(realByteOffset, builder.getInt32(1), "leet.v.plus1");
                llvm::Value* vXor = builder.CreateXor(realByteOffset, vPlus1, "leet.v.xor");
                llvm::Value* vLsb = builder.CreateAnd(vXor, builder.getInt32(1), "leet.v.lsb");
                condition = builder.CreateICmpEQ(vLsb, builder.getInt32(1), "leet.opaque.cond");
                isTrueCondition = true;
            }
            else if (variant == 3)
            {
                // Variant 3 (Evaluates to False): (v ^ (v + 1)) has LSB 1 -> ((v ^ (v + 1)) & 1) == 0 is never true
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
                    use->set(currentAddress);
                }
                else
                {
                    currentAddress = ComputeFreshAddress(builder);
                    use->set(currentAddress);
                }
            }
        }

        allocaInst->eraseFromParent();
    }


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
