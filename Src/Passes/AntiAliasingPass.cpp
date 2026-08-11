#include "AntiAliasingPass.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

#include "PermutationHelper.h"
#include "SettingsParser.h"

#include <vector>
#include <algorithm>

constexpr uint64_t s_MinCandidateSize = 1;
constexpr uint64_t s_MaxCandidateSize = 8; // Don't store any complex types, 8 bytes for a 64bit is enoguh

constexpr uint32_t s_MinSlotsToBother = 2; // doing 1 slot is completely pointless

bool TryGetAllocaTotalSize(llvm::AllocaInst* allocaInst, const llvm::DataLayout& dataLayout, uint64_t& outSize)
{
    if (!allocaInst->isStaticAlloca())
        return false;

    llvm::Type* allocatedType = allocaInst->getAllocatedType();
    if (!allocatedType->isSized())
        return false;

    llvm::TypeSize elemSize = dataLayout.getTypeAllocSize(allocatedType);
    if (elemSize.isScalable())
        return false; // scalable vectors have no fixed byte size

    auto* arraySizeCint = llvm::dyn_cast<llvm::ConstantInt>(allocaInst->getArraySize());
    if (!arraySizeCint)
        return false;

    uint64_t arraySize = arraySizeCint->getZExtValue();
    if (arraySize == 0)
        return false;

    outSize = elemSize.getFixedValue() * arraySize;
    return true;
}

bool IsEligibleAlloca(llvm::AllocaInst* allocaInst, const llvm::DataLayout& dataLayout)
{
    // skip leet allocas
    if (allocaInst->getName().starts_with("leet."))
        return false;

    // auto* type = allocaInst->getAllocatedType();
    // if (llvm::dyn_cast<llvm::StructType>(type) != nullptr)
    // {
    //     llvm::errs() << "SKIPPING STRUCT\n";
    //     return false;
    // }

    uint64_t fixedSize;
    if (!TryGetAllocaTotalSize(allocaInst, dataLayout, fixedSize))
        return false;

    if (fixedSize < s_MinCandidateSize || fixedSize > s_MaxCandidateSize)
        return false;

    return true;
}

// Throw these bitches out again same as in the dispatcher pass, it's just a debug info for LLVM and it will be invalid after this pass
void StripDebugAndLifetimeUsers(llvm::AllocaInst* allocaInst)
{
    std::vector<llvm::Instruction*> toErase;
    for (llvm::User* user : allocaInst->users())
    {
        if (auto* intrinsicInst = llvm::dyn_cast<llvm::IntrinsicInst>(user))
        {
            if (intrinsicInst->getIntrinsicID() == llvm::Intrinsic::lifetime_start ||
                intrinsicInst->getIntrinsicID() == llvm::Intrinsic::lifetime_end)
            {
                toErase.push_back(intrinsicInst);
            }
        }
    }
    for (llvm::Instruction* inst : toErase)
        inst->eraseFromParent();
}

llvm::PreservedAnalyses LeetObfuscator::AntiAliasingPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running AntiAliasingPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    for (llvm::Function &function : module)
    {
        if (function.isDeclaration())
        {
            continue;
        }

        // don't obfuscate our own runtime helper, this would create infinite recursion
        if (function.getName() == "__leet_permutation" || function.getName() == "__leet_split_mix_64")
        {
            continue;
        }

        ObfuscateFunction(function);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::AntiAliasingPass::ObfuscateFunction(llvm::Function &function)
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

    if (function.getName().find("__leet_exception") != std::string::npos || function.getName().find("__leet_dispatcher_barrier") != std::string::npos)
    {
        return;
    }

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    llvm::Module* module = function.getParent();

    llvm::Function* permFunction = GetOrEmitLeetPermutationWithDeps(*module);
    if (!permFunction)
    {
        llvm::errs() << "Failed to emit permutation func\n";
        return;
    }

    llvm::BasicBlock& entryBlock = function.getEntryBlock();
    const llvm::DataLayout& dataLayout = module->getDataLayout();

    std::vector<llvm::AllocaInst*> candidates;
    for (llvm::Instruction& inst : entryBlock)
    {
        if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(&inst))
        {
            if (generator->DrawRange(1u, 100u) > attributes.antiAliasingProbability)
                continue;
            
            if (IsEligibleAlloca(allocaInst, dataLayout))
                candidates.push_back(allocaInst);
        }
    }

    if (candidates.size() < s_MinSlotsToBother)
    {
        m_Logger.LogFunction(function, "Not enough stack slots to obfuscate", 2);
        return;
    }

    m_Logger.LogFunction(function, "Creating aliasing obfuscation state", 2);

    uint32_t numSlots = uint32_t(candidates.size());

    uint64_t maxSize = 0;
    llvm::Align maxAlign(1);
    for (llvm::AllocaInst* allocaInst : candidates)
    {
        uint64_t size;
        if (!TryGetAllocaTotalSize(allocaInst, dataLayout, size))
            continue;

        maxSize = std::max(maxSize, size);
        llvm::Align typeAlign = dataLayout.getPrefTypeAlign(allocaInst->getAllocatedType());
        maxAlign = std::max(maxAlign, std::max(allocaInst->getAlign(), typeAlign));
        
        // allocaInst->getAllocatedType()->print(llvm::errs());
        // llvm::errs() << "\n";
    }

    uint64_t slotSize = llvm::alignTo(maxSize, maxAlign.value());
    uint64_t totalBytes = slotSize * uint64_t(numSlots);
    m_Logger.LogFunction(function, "Allocating obfuscation buffer and permutation table", 3);

    // create the buffer and permutation table
    llvm::IRBuilder<> entryBuilder(&entryBlock, entryBlock.begin());

    llvm::ArrayType* bufferType = llvm::ArrayType::get(entryBuilder.getInt8Ty(), totalBytes);
    llvm::AllocaInst* leetBuffer = entryBuilder.CreateAlloca(bufferType, nullptr, "leet.buf");
    leetBuffer->setAlignment(maxAlign);

    llvm::ArrayType* permutationTableType = llvm::ArrayType::get(entryBuilder.getInt32Ty(), numSlots);
    llvm::AllocaInst* leetPermTable = entryBuilder.CreateAlloca(permutationTableType, nullptr, "leet.perm");
    leetPermTable->setAlignment(llvm::Align(4));

    for (uint32_t i = 0; i < numSlots; ++i)
    {
        llvm::Value* slotPtr = entryBuilder.CreateInBoundsGEP(
            permutationTableType, leetPermTable,
            {entryBuilder.getInt32(0), entryBuilder.getInt32(i)}, "leet.perm.init.ptr"
        );
        entryBuilder.CreateStore(entryBuilder.getInt32(i), slotPtr);
    }

    llvm::Value* permPtr = entryBuilder.CreateInBoundsGEP(
        permutationTableType, leetPermTable,
        {entryBuilder.getInt32(0), entryBuilder.getInt32(0)}, "leet.perm.ptr"
    );

    entryBuilder.CreateCall(permFunction, {permPtr, entryBuilder.getInt32(numSlots)});

    llvm::Value* bufferBase = entryBuilder.CreateInBoundsGEP(
        bufferType, leetBuffer,
        {entryBuilder.getInt32(0), entryBuilder.getInt32(0)}, "leet.buf.base"
    );

    for (uint32_t canonicalSlot = 0; canonicalSlot < numSlots; ++canonicalSlot)
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

        for (llvm::Use* use : usesToPatch)
        {
            // All users of an alloca should be instructions i think? dunno
            llvm::Instruction* userInst = llvm::dyn_cast<llvm::Instruction>(use->getUser());
            if (!userInst)
            {
                llvm::errs() << "Alloca user is not an instruction?\n";
                continue;
            }

            llvm::IRBuilder<> builder(userInst);

            // If the user is a PHI node, we must insert the calculation at the end
            // of the incoming block (before its terminator), not before the PHI.
            if (auto* phi = llvm::dyn_cast<llvm::PHINode>(userInst))
            {
                llvm::BasicBlock* incomingBlock = phi->getIncomingBlock(*use);
                builder.SetInsertPoint(incomingBlock->getTerminator());
            }

            // Recompute the permutation slot lookup dynamically
            llvm::Value* permSlotPtr = builder.CreateInBoundsGEP(
                permutationTableType,
                leetPermTable,
                {builder.getInt32(0), builder.getInt32(canonicalSlot)},
                "leet.permslot.ptr"
            );
            llvm::Value* physSlot = builder.CreateLoad(builder.getInt32Ty(), permSlotPtr, "leet.physslot");
            llvm::Value* byteOffset = builder.CreateMul(physSlot, builder.getInt32(slotSize), "leet.byteoff");

            llvm::Value* elementAddress = builder.CreateInBoundsGEP(builder.getInt8Ty(), bufferBase, byteOffset, "leet.addr");

            // TODO: just calculate offset with a correct type and delete this?
            llvm::Value* typedAddress = builder.CreateBitCast(elementAddress, allocaInst->getType(), "leet.typed");

            use->set(typedAddress);
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
