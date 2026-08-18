#include "VariableSplittingPass.h"
#include "Src/Passes/RandomNumberGenerator.h"
#include "Src/Passes/SettingsParser.h"
#include "llvm/IR/Analysis.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ADT/PostOrderIterator.h"
#include <algorithm>

extern bool TryGetAllocaTotalSize(llvm::AllocaInst* allocaInst, const llvm::DataLayout& dataLayout, uint64_t& outSize);

llvm::PreservedAnalyses LeetObfuscator::VariableSplittingPass::run(llvm::Module& module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running VariableSplittingPass\n";

    for (auto& function : module)
    {
        ObfuscateFunction(&function);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::VariableSplittingPass::ObfuscateFunction(llvm::Function* function)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(*function, SettingsParser::PassType::VariableSplittingPass, m_Arguments);
    if (SettingsParser::ShouldSkipFunction(function, attributes))
        return;

    if (function->isDeclaration())
        return;

    llvm::Module* module = function->getParent();
    std::vector<llvm::AllocaInst*> allocaInstructions;

    for (auto& block : *function)
    {
        for (auto& instruction : block)
        {
            llvm::AllocaInst* allocaInstruction = llvm::dyn_cast<llvm::AllocaInst>(&instruction);
            if (allocaInstruction)
                allocaInstructions.push_back(allocaInstruction);
        }
    }

    SplitContext splitContext;
    splitContext.attributes = &attributes;

    for (auto& allocaInstruction : allocaInstructions)
    {
        llvm::Type* allocatedType = allocaInstruction->getAllocatedType();
        llvm::IntegerType* intType = llvm::dyn_cast<llvm::IntegerType>(allocatedType);
        if (!intType || intType->getBitWidth() % 8 != 0)
            continue;

        uint64_t allocaSize = 0;
        if (!TryGetAllocaTotalSize(allocaInstruction, module->getDataLayout(), allocaSize))
        {
            continue;
        }
        if (allocaSize < 2 || allocaSize > 8)
        {
            continue;
        }
        
        if (!IsAllocaSplittable(allocaInstruction))
        {
            continue;
        }

        uint32_t pieceByteSize = ComputePartByteSize(allocaSize, attributes.variableSplittingCount);
        if (pieceByteSize >= allocaSize)
        {
            continue;
        }

        SplitAlloca(allocaInstruction, intType, allocaSize, pieceByteSize, splitContext);
    }

    llvm::ReversePostOrderTraversal<llvm::Function*> rpot(function);

    std::vector<llvm::PHINode*> phiNodes;
    for (auto& block : rpot)
    {
        for (auto& instruction : *block)
        {
            if (auto* phi = llvm::dyn_cast<llvm::PHINode>(&instruction))
                phiNodes.push_back(phi);
            else
                RewriteInstruction(&instruction, splitContext);
        }
    }

    for (llvm::PHINode* phi : phiNodes)
        MaterializePhiOperands(phi, splitContext);

    for (auto it = splitContext.toErase.rbegin(); it != splitContext.toErase.rend(); ++it)
    {
        llvm::Instruction* dead = *it;
        if (!dead->use_empty())
        {
            llvm::errs() << "[VariableSplittingPass] variable still has uses: " << *dead << "\n";
            for (llvm::User* user : dead->users())
                llvm::errs() << "\tused by: " << *user << "\n";
        }
        assert(dead->use_empty() && "instruction still has real uses before erase wtf");
        dead->eraseFromParent();
    }

    // Verify the function at the end
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] VariableSplittingPass: Function '" << function->getName() << "' verification failed after transformation!\n";

        // Dump the function IR and terminate
        
        std::error_code ec;
        llvm::raw_fd_ostream logFile("error_log.txt", ec);
        if (!ec)
        {
            function->print(logFile);
            logFile.close();
            llvm::errs() << "VariableSplittingPass: Function IR dumped to error_log.txt\n";
        }
        else
        {
            llvm::errs() << "VariableSplittingPass: Failed to open error_log.txt for writing: " << ec.message() << "\n";
        }
        exit(1);
    }
}

bool LeetObfuscator::VariableSplittingPass::IsAllocaSplittable(llvm::AllocaInst* allocaInstruciton)
{
    for (llvm::User* user : allocaInstruciton->users())
    {
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(user))
        {
            if (load->isVolatile() || load->isAtomic())
                return false; // Don't touch anything that has to do with volatiles or atomics
            continue;
        }
        
        if (auto* store = llvm::dyn_cast<llvm::StoreInst>(user))
        {
            if (store->isVolatile() || store->isAtomic())
                return false; // Don't touch anything that has to do with volatiles or atomics
            if (store->getValueOperand() == allocaInstruciton)
                return false; // Something is using pointer to this alloca, leave it alone
            continue;
        }

        // used as GEP, call, bitcast, ptrtoint select or fucking whatever, just don't touch it if it's not a normal computation value
        // There's really no point in obfuscating it in that case, tho it could be handled
        return false;
    }
    return true;
}

void LeetObfuscator::VariableSplittingPass::MaterializePhiOperands(llvm::PHINode* phi, SplitContext& splitContext)
{
    for (uint32_t i = 0; i < phi->getNumIncomingValues(); i++)
    {
        auto* opInst = llvm::dyn_cast<llvm::Instruction>(phi->getIncomingValue(i));
        if (!opInst) continue;

        auto it = splitContext.partsMap.find(opInst);
        if (it == splitContext.partsMap.end())
            continue; // not split leave it alone

        llvm::BasicBlock* predBlock = phi->getIncomingBlock(i);
        llvm::IRBuilder<> builder(predBlock->getTerminator());
        auto* operandType = llvm::cast<llvm::IntegerType>(phi->getIncomingValue(i)->getType());
        phi->setIncomingValue(i, MergeParts(builder, it->second, operandType));
    }
}

void LeetObfuscator::VariableSplittingPass::SplitAlloca(llvm::AllocaInst* allocaInstruction, llvm::IntegerType* intType, uint64_t byteSize, uint32_t pieceByteSize, SplitContext& splitContext)
{
    llvm::IRBuilder<> entryBuilder(allocaInstruction);

    llvm::IntegerType* pieceType = llvm::IntegerType::get(entryBuilder.getContext(), pieceByteSize * 8);
    uint32_t numPieces = uint32_t(byteSize / pieceByteSize);

    llvm::ArrayType* splitArrayType = llvm::ArrayType::get(pieceType, numPieces);
    llvm::AllocaInst* splitArray = entryBuilder.CreateAlloca(splitArrayType, nullptr, allocaInstruction->getName() + ".split");
    splitArray->setAlignment(allocaInstruction->getAlign());

    std::vector<llvm::LoadInst*> loads;
    std::vector<llvm::StoreInst*> stores;

    for (auto* user : allocaInstruction->users())
    {
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(user))
            loads.push_back(load);
        if (auto* store = llvm::dyn_cast<llvm::StoreInst>(user))
            stores.push_back(store);
    }

    auto getPiecePtr = [&](llvm::IRBuilder<>& builder, uint32_t pieceIndex) -> llvm::Value*
    {
        llvm::Value* index = builder.getInt32(pieceIndex);
        return builder.CreateInBoundsGEP(splitArrayType, splitArray, {builder.getInt32(0), index});
    };

    // Store individual pieces
    for (llvm::StoreInst* store : stores)
    {
        llvm::IRBuilder<> builder(store);
        llvm::Value* storedValue = store->getValueOperand();

        for (uint32_t part = 0; part < numPieces; part++)
        {
            llvm::Value* shifted = builder.CreateLShr(storedValue, llvm::ConstantInt::get(intType, part * pieceByteSize * 8));
            llvm::Value* pieceValue = builder.CreateTrunc(shifted, pieceType);
            builder.CreateStore(pieceValue, getPiecePtr(builder, part));
        }
        store->eraseFromParent();
    }

    splitContext.toErase.push_back(allocaInstruction);
    
    for (llvm::LoadInst* load : loads)
    {
        llvm::IRBuilder<> builder(load);

        PartsInfo info;
        info.partByteSize = pieceByteSize;
        info.parts.reserve(numPieces);
        for (uint32_t part = 0; part < numPieces; part++)
        {
            llvm::Value* pieceVal = builder.CreateLoad(pieceType, getPiecePtr(builder, part));
            info.parts.push_back(pieceVal);
        }

        splitContext.partsMap[load] = std::move(info);
        splitContext.toErase.push_back(load);
    }
}

llvm::Value* LeetObfuscator::VariableSplittingPass::MergeParts(llvm::IRBuilder<>& builder, const PartsInfo& info, llvm::IntegerType* type)
{
    if (info.parts.size() == 1 && info.parts[0]->getType() == type)
        return info.parts[0]; // Already correct size

    llvm::Value* accumulatedValue = llvm::ConstantInt::get(type, 0);
    uint32_t bitsPerPart = info.partByteSize * 8;
    for (size_t part = 0; part < info.parts.size(); part++)
    {
        llvm::Value* ext = builder.CreateZExt(info.parts[part], type);
        accumulatedValue = builder.CreateOr(accumulatedValue, builder.CreateShl(ext, part * bitsPerPart));
    }
    return accumulatedValue;
}

std::vector<llvm::Value*> LeetObfuscator::VariableSplittingPass::SplitValue(llvm::IRBuilder<>& builder, llvm::Value* value, uint32_t pieceByteSize)
{
    auto* intType = llvm::cast<llvm::IntegerType>(value->getType());
    uint32_t totalByteSize = intType->getBitWidth() / 8;

    if (pieceByteSize >= totalByteSize)
        return { value }; // nothing to split

    llvm::IntegerType* pieceType = llvm::IntegerType::get(builder.getContext(), pieceByteSize * 8);
    uint32_t numPieces = totalByteSize / pieceByteSize;
    std::vector<llvm::Value*> parts(numPieces);
    for (uint32_t part = 0; part < numPieces; part++)
    {
        llvm::Value* shifted = builder.CreateLShr(value, part * pieceByteSize * 8);
        parts[part] = builder.CreateTrunc(shifted, pieceType);
    }
    return parts;
}

bool LeetObfuscator::VariableSplittingPass::GetOperandAsParts(llvm::IRBuilder<>& builder, llvm::Value* operand, SplitContext& splitContext, PartsInfo& outInfo)
{
    if (auto* operandInstruction = llvm::dyn_cast<llvm::Instruction>(operand))
    {
        auto it = splitContext.partsMap.find(operandInstruction);
        if (it != splitContext.partsMap.end())
        {
            // This was already split
            outInfo = it->second;
            return true;
        }
    }
        
    auto* intType = llvm::dyn_cast<llvm::IntegerType>(operand->getType());
    if (!intType || intType->getBitWidth() % 8 != 0)
        return false; // Pointer, float, or not normal witdth, just skip

    uint32_t totalByteSize = intType->getBitWidth() / 8;
    uint32_t pieceByteSize = ComputePartByteSize(totalByteSize, splitContext.attributes->variableSplittingCount);

    outInfo.partByteSize = pieceByteSize;
    outInfo.parts = SplitValue(builder, operand, pieceByteSize);
    return true;
}

uint32_t LeetObfuscator::VariableSplittingPass::ComputePartByteSize(uint64_t totalByteSize, uint32_t splitCount)
{
    uint64_t pieceSize = totalByteSize;
    for (uint32_t i = 0; i < splitCount; i++)
    {
        if (pieceSize <= 1 || pieceSize % 2 != 0)
            break;
        pieceSize /= 2;
    }
    return (uint32_t)pieceSize;
}

void LeetObfuscator::VariableSplittingPass::AlignPartsToCommonSize(llvm::IRBuilder<>& builder, PartsInfo& lhs, PartsInfo& rhs)
{
    if (lhs.partByteSize == rhs.partByteSize)
        return;

    // split to the smallest size
    uint32_t targetByteSize = std::min(lhs.partByteSize, rhs.partByteSize);

    auto resplit = [&](PartsInfo& info)
    {
        if (info.partByteSize == targetByteSize || targetByteSize == 0)
            return;
        if (info.partByteSize % targetByteSize != 0)
        {
            return;
        }

        uint32_t ratio = info.partByteSize / targetByteSize;
        llvm::IntegerType* targetType = llvm::IntegerType::get(builder.getContext(), targetByteSize * 8);

        std::vector<llvm::Value*> newParts;
        newParts.reserve(info.parts.size() * ratio);
        for (llvm::Value* part : info.parts)
        {
            for (uint32_t i = 0; i < ratio; i++)
            {
                llvm::Value* shifted = builder.CreateLShr(part, i * targetByteSize * 8);
                newParts.push_back(builder.CreateTrunc(shifted, targetType));
            }
        }
        info.parts = std::move(newParts);
        info.partByteSize = targetByteSize;
    };

    resplit(lhs);
    resplit(rhs);
}

void LeetObfuscator::VariableSplittingPass::RewriteInstruction(llvm::Instruction* instruction, SplitContext& splitContext)
{
    if (llvm::isa<llvm::PHINode>(instruction))
        return;

    switch (instruction->getOpcode())
    {
        case llvm::Instruction::And:
        case llvm::Instruction::Or:
        case llvm::Instruction::Xor:
            if (TryRewriteBitwise(instruction, splitContext)) return;
            break;
        case llvm::Instruction::Add:
            if (TryRewriteAdd(instruction, splitContext)) return;
            break;
        case llvm::Instruction::Sub:
            if (TryRewriteSub(instruction, splitContext)) return;
            break;
        case llvm::Instruction::ICmp:
            if (TryRewriteIcmpEq(instruction, splitContext)) return;
            break;
        default:
            break;
    }

    // Unhandled opcode, just merge the parts at this point
    for (uint32_t i = 0; i < instruction->getNumOperands(); ++i)
    {
        auto* opInst = llvm::dyn_cast<llvm::Instruction>(instruction->getOperand(i));
        if (!opInst) continue;
        auto it = splitContext.partsMap.find(opInst);
        if (it == splitContext.partsMap.end()) continue;

        llvm::IRBuilder<> builder(instruction);
        auto* operandType = llvm::cast<llvm::IntegerType>(instruction->getOperand(i)->getType());
        instruction->setOperand(i, MergeParts(builder, it->second, operandType));
    }
}

bool LeetObfuscator::VariableSplittingPass::IsValueSplit(llvm::Value* value, const SplitContext& splitContext)
{
    if (auto* operandInstruction = llvm::dyn_cast<llvm::Instruction>(value))
    {
        auto it = splitContext.partsMap.find(operandInstruction);
        if (it != splitContext.partsMap.end())
        {
            return true;
        }
    }
    return false;
}

bool LeetObfuscator::VariableSplittingPass::TryRewriteAdd(llvm::Instruction* instruction, SplitContext& splitContext)
{
    llvm::IRBuilder<> builder(instruction);
    PartsInfo lhs, rhs;

    if (!IsValueSplit(instruction->getOperand(0), splitContext) && !IsValueSplit(instruction->getOperand(1), splitContext))
    {
        std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(*splitContext.attributes);
        if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingProbability)
        {
            return false;
        }
    }

    if (!GetOperandAsParts(builder, instruction->getOperand(0), splitContext, lhs)) return false;
    if (!GetOperandAsParts(builder, instruction->getOperand(1), splitContext, rhs)) return false;

    AlignPartsToCommonSize(builder, lhs, rhs);
    if (lhs.parts.size() != rhs.parts.size())
        return false; // couldn't line the two operands up cleanly, fall back to the plain merge path

    uint32_t partBits = lhs.partByteSize * 8;
    llvm::Type* partTy = llvm::IntegerType::get(builder.getContext(), partBits);
    llvm::Type* wideTy = llvm::IntegerType::get(builder.getContext(), partBits * 2);

    PartsInfo result;
    result.partByteSize = lhs.partByteSize;
    result.parts.resize(lhs.parts.size());
    llvm::Value* carry = llvm::ConstantInt::get(partTy, 0);
    for (size_t part = 0; part < lhs.parts.size(); part++)
    {
        llvm::Value* sum = builder.CreateAdd(
            builder.CreateAdd(builder.CreateZExt(lhs.parts[part], wideTy), builder.CreateZExt(rhs.parts[part], wideTy)),
            builder.CreateZExt(carry, wideTy)
        );
        result.parts[part] = builder.CreateTrunc(sum, partTy);
        carry = builder.CreateTrunc(builder.CreateLShr(sum, partBits), partTy);
    }
    splitContext.partsMap[instruction] = std::move(result);
    splitContext.toErase.push_back(instruction);
    return true;
}

bool LeetObfuscator::VariableSplittingPass::TryRewriteSub(llvm::Instruction* instruction, SplitContext& splitContext)
{
    llvm::IRBuilder<> builder(instruction);
    PartsInfo lhs, rhs;

    if (!IsValueSplit(instruction->getOperand(0), splitContext) && !IsValueSplit(instruction->getOperand(1), splitContext))
    {
        std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(*splitContext.attributes);
        if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingProbability)
        {
            return false;
        }
    }

    if (!GetOperandAsParts(builder, instruction->getOperand(0), splitContext, lhs)) return false;
    if (!GetOperandAsParts(builder, instruction->getOperand(1), splitContext, rhs)) return false;

    AlignPartsToCommonSize(builder, lhs, rhs);
    if (lhs.parts.size() != rhs.parts.size())
        return false; // couldn't line the two operands up cleanly, fall back to the plain merge path

    uint32_t partBits = lhs.partByteSize * 8;
    llvm::Type* partTy = llvm::IntegerType::get(builder.getContext(), partBits);
    llvm::Type* wideTy = llvm::IntegerType::get(builder.getContext(), partBits * 2);
    llvm::Value* allOnes = llvm::ConstantInt::get(partTy, ~uint64_t(0));

    PartsInfo result;
    result.partByteSize = lhs.partByteSize;
    result.parts.resize(lhs.parts.size());
    llvm::Value* carry = llvm::ConstantInt::get(partTy, 1);
    for (size_t part = 0; part < lhs.parts.size(); part++)
    {
        llvm::Value* invertedRhs = builder.CreateXor(rhs.parts[part], allOnes);
        llvm::Value* sum = builder.CreateAdd(
            builder.CreateAdd(builder.CreateZExt(lhs.parts[part], wideTy), builder.CreateZExt(invertedRhs, wideTy)),
            builder.CreateZExt(carry, wideTy)
        );
        result.parts[part] = builder.CreateTrunc(sum, partTy);
        carry = builder.CreateTrunc(builder.CreateLShr(sum, partBits), partTy);
    }
    splitContext.partsMap[instruction] = std::move(result);
    splitContext.toErase.push_back(instruction);
    return true;
}

bool LeetObfuscator::VariableSplittingPass::TryRewriteBitwise(llvm::Instruction* instruction, SplitContext& splitContext)
{
    llvm::IRBuilder<> builder(instruction);
    PartsInfo lhs, rhs;

    if (!IsValueSplit(instruction->getOperand(0), splitContext) && !IsValueSplit(instruction->getOperand(1), splitContext))
    {
        std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(*splitContext.attributes);
        if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingProbability)
        {
            return false;
        }
    }

    if (!GetOperandAsParts(builder, instruction->getOperand(0), splitContext, lhs)) return false;
    if (!GetOperandAsParts(builder, instruction->getOperand(1), splitContext, rhs)) return false;

    AlignPartsToCommonSize(builder, lhs, rhs);
    if (lhs.parts.size() != rhs.parts.size())
        return false; // couldn't line the two operands up cleanly, fall back to the plain merge path

    PartsInfo result;
    result.partByteSize = lhs.partByteSize;
    result.parts.resize(lhs.parts.size());
    for (size_t part = 0; part < lhs.parts.size(); part++)
    {
        switch (instruction->getOpcode()) {
            case llvm::Instruction::And: result.parts[part] = builder.CreateAnd(lhs.parts[part], rhs.parts[part]); break;
            case llvm::Instruction::Or:  result.parts[part] = builder.CreateOr (lhs.parts[part], rhs.parts[part]); break;
            case llvm::Instruction::Xor: result.parts[part] = builder.CreateXor(lhs.parts[part], rhs.parts[part]); break;
            default: return false;
        }
    }
    splitContext.partsMap[instruction] = std::move(result);
    splitContext.toErase.push_back(instruction);
    return true;
}

bool LeetObfuscator::VariableSplittingPass::TryRewriteIcmpEq(llvm::Instruction* instruction, SplitContext& splitContext)
{
    auto* icmp = llvm::cast<llvm::ICmpInst>(instruction);
    if (icmp->getPredicate() != llvm::CmpInst::ICMP_EQ &&
        icmp->getPredicate() != llvm::CmpInst::ICMP_NE)
        return false;

    if (!IsValueSplit(instruction->getOperand(0), splitContext) && !IsValueSplit(instruction->getOperand(1), splitContext))
    {
        std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(*splitContext.attributes);
        if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingProbability)
        {
            return false;
        }
    }

    llvm::IRBuilder<> builder(instruction);
    PartsInfo lhs, rhs;
    if (!GetOperandAsParts(builder, instruction->getOperand(0), splitContext, lhs)) return false;
    if (!GetOperandAsParts(builder, instruction->getOperand(1), splitContext, rhs)) return false;

    AlignPartsToCommonSize(builder, lhs, rhs);
    if (lhs.parts.size() != rhs.parts.size())
        return false; // couldn't line the two operands up cleanly, fall back to the plain merge path

    llvm::Value* allEq = nullptr;
    for (size_t part = 0; part < lhs.parts.size(); part++)
    {
        llvm::Value* eq = builder.CreateICmpEQ(lhs.parts[part], rhs.parts[part]);
        allEq = allEq ? builder.CreateAnd(allEq, eq) : eq;
    }
    llvm::Value* result = icmp->getPredicate() == llvm::CmpInst::ICMP_EQ ? allEq : builder.CreateNot(allEq);

    instruction->replaceAllUsesWith(result);
    splitContext.toErase.push_back(instruction);
    return true;
}