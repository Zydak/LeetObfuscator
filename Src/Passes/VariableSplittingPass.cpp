#include "VariableSplittingPass.h"
#include "Src/Passes/RandomNumberGenerator.h"
#include "Src/Passes/SettingsParser.h"
#include "llvm/IR/Analysis.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ADT/PostOrderIterator.h"
#include <algorithm>

extern bool TryGetAllocaTotalSize(llvm::AllocaInst* allocaInst, const llvm::DataLayout& dataLayout, uint64_t& outSize);

llvm::PreservedAnalyses LeetObfuscator::VariableSplittingPass::run(llvm::Module& module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running VariableSplittingPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    for (auto& function : module)
    {
        ObfuscateFunction(&function);
    }

    m_Logger.LogModule(module, "Summary: Replaced " + std::to_string(m_Stats.replacedAllocas) + " allocas, " + 
        std::to_string(m_Stats.replacedAdds) + " additions, " +
        std::to_string(m_Stats.replacedSubs) + " subtractions, " +
        std::to_string(m_Stats.replacedBitwise) + " bitwise ops, " +
        std::to_string(m_Stats.replacedICmps) + " icmps", 0);

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
        {
            m_Logger.LogInstruction(*allocaInstruction, "Couldn't replace alloca" + std::string(" in function '") + allocaInstruction->getFunction()->getName().str() + "' because it is not a byte-aligned integer type", 1);
            continue;
        }

        uint64_t allocaSize = 0;
        if (!TryGetAllocaTotalSize(allocaInstruction, module->getDataLayout(), allocaSize))
        {
            m_Logger.LogInstruction(*allocaInstruction, "Couldn't replace alloca" + std::string(" in function '") + allocaInstruction->getFunction()->getName().str() + "' because could not get total size", 1);
            continue;
        }
        if (allocaSize < 2 || allocaSize > 8)
        {
            m_Logger.LogInstruction(*allocaInstruction, "Couldn't replace alloca" + std::string(" in function '") + allocaInstruction->getFunction()->getName().str() + "' because its size is not between 2 and 8 bytes (" + std::to_string(allocaSize) + " bytes)", 1);
            continue;
        }
        
        if (!IsAllocaSplittable(allocaInstruction))
        {
            continue;
        }

        uint32_t pieceByteSize = ComputePartByteSize(allocaSize, attributes.variableSplittingCount);
        if (pieceByteSize >= allocaSize)
        {
            m_Logger.LogInstruction(*allocaInstruction, "Couldn't replace alloca" + std::string(" in function '") + allocaInstruction->getFunction()->getName().str() + "' because computed piece byte size >= alloca size", 1);
            continue;
        }

        m_Logger.LogInstruction(*allocaInstruction, "Replaced alloca" + std::string(" in function '") + allocaInstruction->getFunction()->getName().str() + "' with " + std::to_string(allocaSize / pieceByteSize) + " pieces of size " + std::to_string(pieceByteSize) + " bytes", 1);
        m_Stats.replacedAllocas++;
        SplitAlloca(allocaInstruction, intType, allocaSize, pieceByteSize, splitContext);
    }

    llvm::ReversePostOrderTraversal<llvm::Function*> rpot(function);

    // Collect everything first
    std::vector<llvm::Instruction*> instructions;
    for (auto& block : rpot)
    {
        for (auto& instruction : *block)
        {
            instructions.push_back(&instruction);
        }
    }

    std::vector<llvm::PHINode*> phiNodes;
    for (llvm::Instruction* instruction : instructions)
    {
        if (auto* phi = llvm::dyn_cast<llvm::PHINode>(instruction))
            phiNodes.push_back(phi);
        else
            RewriteInstruction(instruction, splitContext);
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
            {
                m_Logger.LogInstruction(*allocaInstruciton, "Couldn't replace alloca" + std::string(" in function '") + allocaInstruciton->getFunction()->getName().str() + "' because it has a volatile/atomic load user", 1);
                return false; // Don't touch anything that has to do with volatiles or atomics
            }
            continue;
        }
        
        if (auto* store = llvm::dyn_cast<llvm::StoreInst>(user))
        {
            if (store->isVolatile() || store->isAtomic())
            {
                m_Logger.LogInstruction(*allocaInstruciton, "Couldn't replace alloca" + std::string(" in function '") + allocaInstruciton->getFunction()->getName().str() + "' because it has a volatile/atomic store user", 1);
                return false; // Don't touch anything that has to do with volatiles or atomics
            }
            if (store->getValueOperand() == allocaInstruciton)
            {
                m_Logger.LogInstruction(*allocaInstruciton, "Couldn't replace alloca" + std::string(" in function '") + allocaInstruciton->getFunction()->getName().str() + "' because its pointer is stored directly", 1);
                return false; // Something is using pointer to this alloca, leave it alone
            }
            continue;
        }

        // used as GEP, call, bitcast, ptrtoint select or fucking whatever, just don't touch it if it's not a normal computation value
        // There's really no point in obfuscating it in that case, tho it could be handled
        m_Logger.LogInstruction(*allocaInstruciton, "Couldn't replace alloca" + std::string(" in function '") + allocaInstruciton->getFunction()->getName().str() + "' because it has an unhandled user type", 1);
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

        auto* operandType = llvm::cast<llvm::IntegerType>(phi->getIncomingValue(i)->getType());
        phi->setIncomingValue(i, GetMergedValue(opInst, operandType, splitContext));
    }
}

void LeetObfuscator::VariableSplittingPass::SplitAlloca(llvm::AllocaInst* allocaInstruction, llvm::IntegerType* intType, uint64_t byteSize, uint32_t pieceByteSize, SplitContext& splitContext)
{
    llvm::IRBuilder<> entryBuilder(allocaInstruction);

    llvm::IntegerType* pieceType = llvm::IntegerType::get(entryBuilder.getContext(), pieceByteSize * 8);
    uint32_t numPieces = uint32_t(byteSize / pieceByteSize);

    std::vector<llvm::AllocaInst*> pieceAllocas;
    for (uint32_t i = 0; i < numPieces; i++)
    {
        llvm::AllocaInst* pieceAlloca = entryBuilder.CreateAlloca(pieceType, nullptr, allocaInstruction->getName() + ".split." + std::to_string(i));
        pieceAlloca->setAlignment(llvm::Align(1));
        pieceAllocas.push_back(pieceAlloca);
    }

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
        return pieceAllocas[pieceIndex];
    };

    // Store individual pieces
    for (llvm::StoreInst* store : stores)
    {
        llvm::IRBuilder<> builder(store);
        llvm::Value* storedValue = store->getValueOperand();

        for (uint32_t part = 0; part < numPieces; part++)
        {
            llvm::Value* shifted = (part == 0) ? storedValue : builder.CreateLShr(storedValue, llvm::ConstantInt::get(intType, part * pieceByteSize * 8));
            llvm::Value* pieceValue = (pieceType == shifted->getType()) ? shifted : builder.CreateTrunc(shifted, pieceType);
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

llvm::Value* LeetObfuscator::VariableSplittingPass::MergeParts(llvm::IRBuilder<>& builder, const PartsInfo& info, llvm::IntegerType* type, std::shared_ptr<RandomNumberGenerator> generator)
{
    if (info.parts.size() == 1 && info.parts[0]->getType() == type)
        return info.parts[0]; // Already correct size

    llvm::Value* accumulatedValue = nullptr;
    uint32_t bitsPerPart = info.partByteSize * 8;
    for (size_t part = 0; part < info.parts.size(); part++)
    {
        llvm::Value* ext = (info.parts[part]->getType() == type) ? info.parts[part] : builder.CreateZExt(info.parts[part], type);
        
        llvm::Value* shiftedValue = nullptr;
        if (part == 0)
        {
            shiftedValue = ext;
        }
        else
        {
            uint32_t choice = generator->DrawRange(0u, 1u);
            if (choice == 0)
            {
                shiftedValue = builder.CreateShl(ext, part * bitsPerPart);
            }
            else
            {
                uint64_t multiplier = 1ULL << (part * bitsPerPart);
                shiftedValue = builder.CreateMul(ext, llvm::ConstantInt::get(type, multiplier));
            }
        }

        if (!accumulatedValue)
        {
            accumulatedValue = shiftedValue;
        }
        else
        {
            uint32_t choice = generator->DrawRange(0u, 1u);
            if (choice == 0)
            {
                accumulatedValue = builder.CreateOr(accumulatedValue, shiftedValue);
            }
            else
            {
                accumulatedValue = builder.CreateAdd(accumulatedValue, shiftedValue);
            }
        }
    }
    return accumulatedValue ? accumulatedValue : llvm::ConstantInt::get(type, 0);
}

llvm::Value* LeetObfuscator::VariableSplittingPass::GetMergedValue(llvm::Instruction* instruction, llvm::IntegerType* type, SplitContext& splitContext)
{
    auto it = splitContext.mergedMap.find(instruction);
    if (it != splitContext.mergedMap.end())
        return it->second;

    auto partsIt = splitContext.partsMap.find(instruction);
    if (partsIt == splitContext.partsMap.end())
        return instruction;

    llvm::IRBuilder<> builder(instruction);
    llvm::Value* mergedValue = MergeParts(builder, partsIt->second, type, SettingsParser::GetGenerator(*splitContext.attributes));
    splitContext.mergedMap[instruction] = mergedValue;
    return mergedValue;
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
        llvm::Value* shifted = (part == 0) ? value : builder.CreateLShr(value, part * pieceByteSize * 8);
        parts[part] = (pieceType == shifted->getType()) ? shifted : builder.CreateTrunc(shifted, pieceType);
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
                llvm::Value* shifted = (i == 0) ? part : builder.CreateLShr(part, i * targetByteSize * 8);
                newParts.push_back((targetType == shifted->getType()) ? shifted : builder.CreateTrunc(shifted, targetType));
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
    bool didMerge = false;
    for (uint32_t i = 0; i < instruction->getNumOperands(); ++i)
    {
        auto* opInst = llvm::dyn_cast<llvm::Instruction>(instruction->getOperand(i));
        if (!opInst) continue;
        auto it = splitContext.partsMap.find(opInst);
        if (it == splitContext.partsMap.end()) continue;

        auto* operandType = llvm::cast<llvm::IntegerType>(instruction->getOperand(i)->getType());
        instruction->setOperand(i, GetMergedValue(opInst, operandType, splitContext));
        didMerge = true;
    }
    if (didMerge)
    {
        m_Logger.LogInstruction(*instruction, "Merged split parts back together in function '" + instruction->getFunction()->getName().str() + "' for unhandled instruction opcode", 1);
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
    if (!IsValueSplit(instruction->getOperand(0), splitContext) && !IsValueSplit(instruction->getOperand(1), splitContext))
    {
        return false;
    }

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(*splitContext.attributes);
    if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingProbability)
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Add instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because probability check failed", 1);
        return false;
    }

    llvm::IRBuilder<> builder(instruction);
    PartsInfo lhs, rhs;

    if (!GetOperandAsParts(builder, instruction->getOperand(0), splitContext, lhs))
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Add instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because LHS could not be split", 1);
        return false;
    }
    if (!GetOperandAsParts(builder, instruction->getOperand(1), splitContext, rhs))
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Add instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because RHS could not be split", 1);
        return false;
    }

    AlignPartsToCommonSize(builder, lhs, rhs);
    if (lhs.parts.size() != rhs.parts.size())
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Add instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because parts could not be aligned", 1);
        return false; // couldn't line the two operands up cleanly, fall back to the plain merge path
    }

    uint32_t partBits = lhs.partByteSize * 8;
    llvm::Type* partTy = llvm::IntegerType::get(builder.getContext(), partBits);
    llvm::Type* wideTy = llvm::IntegerType::get(builder.getContext(), partBits * 2);

    PartsInfo result;
    result.partByteSize = lhs.partByteSize;
    result.parts.resize(lhs.parts.size());
    llvm::Value* carry = nullptr;
    for (size_t part = 0; part < lhs.parts.size(); part++)
    {
        llvm::Value* termSum = builder.CreateAdd(builder.CreateZExt(lhs.parts[part], wideTy), builder.CreateZExt(rhs.parts[part], wideTy));
        llvm::Value* sum = carry ? builder.CreateAdd(termSum, builder.CreateZExt(carry, wideTy)) : termSum;
        result.parts[part] = builder.CreateTrunc(sum, partTy);
        if (part + 1 < lhs.parts.size())
        {
            carry = builder.CreateTrunc(builder.CreateLShr(sum, partBits), partTy);
        }
    }
    m_Logger.LogInstruction(*instruction, "Replaced Add instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' with " + std::to_string(lhs.parts.size()) + " split additions", 1);
    m_Stats.replacedAdds++;
    splitContext.partsMap[instruction] = std::move(result);
    splitContext.toErase.push_back(instruction);
    return true;
}

bool LeetObfuscator::VariableSplittingPass::TryRewriteSub(llvm::Instruction* instruction, SplitContext& splitContext)
{
    if (!IsValueSplit(instruction->getOperand(0), splitContext) && !IsValueSplit(instruction->getOperand(1), splitContext))
    {
        return false;
    }

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(*splitContext.attributes);
    if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingProbability)
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Sub instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because probability check failed", 1);
        return false;
    }

    llvm::IRBuilder<> builder(instruction);
    PartsInfo lhs, rhs;

    if (!GetOperandAsParts(builder, instruction->getOperand(0), splitContext, lhs))
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Sub instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because LHS could not be split", 1);
        return false;
    }
    if (!GetOperandAsParts(builder, instruction->getOperand(1), splitContext, rhs))
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Sub instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because RHS could not be split", 1);
        return false;
    }

    AlignPartsToCommonSize(builder, lhs, rhs);
    if (lhs.parts.size() != rhs.parts.size())
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Sub instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because parts could not be aligned", 1);
        return false; // couldn't line the two operands up cleanly, fall back to the plain merge path
    }

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
        if (part + 1 < lhs.parts.size())
        {
            carry = builder.CreateTrunc(builder.CreateLShr(sum, partBits), partTy);
        }
    }
    m_Logger.LogInstruction(*instruction, "Replaced Sub instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' with " + std::to_string(lhs.parts.size()) + " split subtractions", 1);
    m_Stats.replacedSubs++;
    splitContext.partsMap[instruction] = std::move(result);
    splitContext.toErase.push_back(instruction);
    return true;
}

bool LeetObfuscator::VariableSplittingPass::TryRewriteBitwise(llvm::Instruction* instruction, SplitContext& splitContext)
{
    if (!IsValueSplit(instruction->getOperand(0), splitContext) && !IsValueSplit(instruction->getOperand(1), splitContext))
    {
        return false;
    }

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(*splitContext.attributes);
    if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingProbability)
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Bitwise instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because probability check failed", 1);
        return false;
    }

    llvm::IRBuilder<> builder(instruction);
    PartsInfo lhs, rhs;

    if (!GetOperandAsParts(builder, instruction->getOperand(0), splitContext, lhs))
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Bitwise instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because LHS could not be split", 1);
        return false;
    }
    if (!GetOperandAsParts(builder, instruction->getOperand(1), splitContext, rhs))
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Bitwise instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because RHS could not be split", 1);
        return false;
    }

    AlignPartsToCommonSize(builder, lhs, rhs);
    if (lhs.parts.size() != rhs.parts.size())
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace Bitwise instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because parts could not be aligned", 1);
        return false; // couldn't line the two operands up cleanly, fall back to the plain merge path
    }

    PartsInfo result;
    result.partByteSize = lhs.partByteSize;
    result.parts.resize(lhs.parts.size());
    std::vector<size_t> indices(lhs.parts.size());
    std::iota(indices.begin(), indices.end(), 0);
    generator->Shuffle(indices.begin(), indices.end());

    for (size_t part : indices)
    {
        // Pseudo-random choice based on pointer value to avoid adding SplitContext here
        uint32_t choice = generator->DrawRange(0u, 2u);
        switch (instruction->getOpcode())
        {
            case llvm::Instruction::And:
            {
                if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingMBAProbability)
                {
                    // No mba
                    result.parts[part] = builder.CreateAnd(lhs.parts[part], rhs.parts[part]); break;
                    continue;
                }

                // MBA
                if (choice == 0) // a & b = (a | b) - (a ^ b)
                    result.parts[part] = builder.CreateSub(builder.CreateOr(lhs.parts[part], rhs.parts[part]), builder.CreateXor(lhs.parts[part], rhs.parts[part]));
                else if (choice == 1) // a & b = ~(~a | ~b)
                    result.parts[part] = builder.CreateNot(builder.CreateOr(builder.CreateNot(lhs.parts[part]), builder.CreateNot(rhs.parts[part])));
                else 
                    result.parts[part] = builder.CreateAnd(lhs.parts[part], rhs.parts[part]);
                break;
            }
            case llvm::Instruction::Or:
            {
                if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingMBAProbability)
                {
                    // No mba
                    result.parts[part] = builder.CreateOr(lhs.parts[part], rhs.parts[part]); break;
                    continue;
                }

                // MBA
                if (choice == 0) // a | b = (a & b) + (a ^ b)
                    result.parts[part] = builder.CreateAdd(builder.CreateAnd(lhs.parts[part], rhs.parts[part]), builder.CreateXor(lhs.parts[part], rhs.parts[part]));
                else if (choice == 1) // a | b = ~(~a & ~b)
                    result.parts[part] = builder.CreateNot(builder.CreateAnd(builder.CreateNot(lhs.parts[part]), builder.CreateNot(rhs.parts[part])));
                else
                    result.parts[part] = builder.CreateOr (lhs.parts[part], rhs.parts[part]);
                break;
            }
            case llvm::Instruction::Xor:
            {
                if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingMBAProbability)
                {
                    // No mba
                    result.parts[part] = builder.CreateXor(lhs.parts[part], rhs.parts[part]); break;
                    continue;
                }

                // MBA
                if (choice == 0) // a ^ b = (a | b) - (a & b)
                    result.parts[part] = builder.CreateSub(builder.CreateOr(lhs.parts[part], rhs.parts[part]), builder.CreateAnd(lhs.parts[part], rhs.parts[part]));
                else if (choice == 1) // a ^ b = (a & ~b) | (~a & b)
                    result.parts[part] = builder.CreateOr(builder.CreateAnd(lhs.parts[part], builder.CreateNot(rhs.parts[part])), builder.CreateAnd(builder.CreateNot(lhs.parts[part]), rhs.parts[part]));
                else
                    result.parts[part] = builder.CreateXor(lhs.parts[part], rhs.parts[part]);
                break;
            }
            default: return false;
        }
    }
    m_Logger.LogInstruction(*instruction, "Replaced Bitwise instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' with " + std::to_string(lhs.parts.size()) + " split bitwise operations", 1);
    m_Stats.replacedBitwise++;
    splitContext.partsMap[instruction] = std::move(result);
    splitContext.toErase.push_back(instruction);
    return true;
}

bool LeetObfuscator::VariableSplittingPass::TryRewriteIcmpEq(llvm::Instruction* instruction, SplitContext& splitContext)
{
    auto* icmp = llvm::cast<llvm::ICmpInst>(instruction);
    if (icmp->getPredicate() != llvm::CmpInst::ICMP_EQ &&
        icmp->getPredicate() != llvm::CmpInst::ICMP_NE)
    {
        // Don't log this one, too noisy for all non-eq/ne icmps
        return false;
    }

    if (!IsValueSplit(instruction->getOperand(0), splitContext) && !IsValueSplit(instruction->getOperand(1), splitContext))
    {
        return false;
    }

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(*splitContext.attributes);
    if (generator->DrawRange(1u, 100u) > splitContext.attributes->variableSplittingProbability)
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace ICmp instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because probability check failed", 1);
        return false;
    }

    llvm::IRBuilder<> builder(instruction);
    PartsInfo lhs, rhs;
    if (!GetOperandAsParts(builder, instruction->getOperand(0), splitContext, lhs))
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace ICmp instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because LHS could not be split", 1);
        return false;
    }
    if (!GetOperandAsParts(builder, instruction->getOperand(1), splitContext, rhs))
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace ICmp instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because RHS could not be split", 1);
        return false;
    }

    AlignPartsToCommonSize(builder, lhs, rhs);
    if (lhs.parts.size() != rhs.parts.size())
    {
        m_Logger.LogInstruction(*instruction, "Couldn't replace ICmp instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' because parts could not be aligned", 1);
        return false; // couldn't line the two operands up cleanly, fall back to the plain merge path
    }

    llvm::Value* allEq = nullptr;
    std::vector<size_t> indices(lhs.parts.size());
    std::iota(indices.begin(), indices.end(), 0);
    generator->Shuffle(indices.begin(), indices.end());

    for (size_t part : indices)
    {
        uint32_t choice = generator->DrawRange(0u, 2u);
        llvm::Value* eq;
        if (choice == 0)
        {
            eq = builder.CreateICmpEQ(lhs.parts[part], rhs.parts[part]);
        }
        else if (choice == 1)
        {
            eq = builder.CreateNot(builder.CreateICmpNE(lhs.parts[part], rhs.parts[part]));
        }
        else
        {
            llvm::Value* xorVal = builder.CreateXor(lhs.parts[part], rhs.parts[part]);
            eq = builder.CreateICmpEQ(xorVal, llvm::ConstantInt::get(xorVal->getType(), 0));
        }
        
        if (allEq)
        {
            uint32_t mergeChoice = generator->DrawRange(0u, 1u);
            if (mergeChoice == 0)
                allEq = builder.CreateAnd(allEq, eq);
            else
                allEq = builder.CreateNot(builder.CreateOr(builder.CreateNot(allEq), builder.CreateNot(eq)));
        }
        else
        {
            allEq = eq;
        }
    }
    llvm::Value* result = icmp->getPredicate() == llvm::CmpInst::ICMP_EQ ? allEq : builder.CreateNot(allEq);

    m_Logger.LogInstruction(*instruction, "Replaced ICmp instruction" + std::string(" in function '") + instruction->getFunction()->getName().str() + "' with " + std::to_string(lhs.parts.size()) + " split ICmps", 1);
    m_Stats.replacedICmps++;
    instruction->replaceAllUsesWith(result);
    splitContext.toErase.push_back(instruction);
    return true;
}