#include "MBAPass.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"

#include "llvm/IR/Verifier.h"
#include "SettingsParser.h"
#include "iostream"
#include "llvm/IR/InstIterator.h"
#include "RandomNumberGenerator.h"

static constexpr const char *ARITHMETIC_TAG = "obfuscator.arithmetic";

llvm::PreservedAnalyses LeetObfuscator::MBAPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running MBAPass\n";

    for (auto& function : module)
    {
        ObfuscateFunction(function);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::MBAPass::ObfuscateFunction(llvm::Function& function)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(
        function,
        SettingsParser::PassType::MBAPass,
        m_Arguments
    );
    if (attributes.skip)
    {
        return;
    }

    size_t instructionCount = std::distance(llvm::inst_begin(function), llvm::inst_end(function));
    if ((attributes.maxFunctionSize != 0 && instructionCount > attributes.maxFunctionSize) ||
        (attributes.minFunctionSize != 0 && instructionCount < attributes.minFunctionSize)
    )
    {
        return;
    }

    std::shared_ptr<RandomNumberGenerator> generator = RandomNumberGenerator::GetGlobalRandomNumberGenerator();
    if (generator->GetSeed() != attributes.runtimeSeed)
        generator = std::make_shared<RandomNumberGenerator>(attributes.runtimeSeed); // This function has unique seed

    // Find all binary instructions
    std::vector<llvm::Instruction*> instructionsToObfuscate;
    for (auto& basicBlock : function)
    {
        if ((attributes.maxBlockSize != 0 && basicBlock.size() > attributes.maxBlockSize) ||
            (attributes.minBlockSize != 0 && basicBlock.size() < attributes.minBlockSize)
        )
        {
            continue;
        }

        std::cout << basicBlock.size() << " | " << attributes.minBlockSize << std::endl;

        for (auto& instruction : basicBlock)
        {
            if (instruction.getMetadata(ARITHMETIC_TAG))
                continue; // Already marked as done
            
            if (auto* binaryOperation = llvm::dyn_cast<llvm::BinaryOperator>(&instruction))
            {
                auto op = binaryOperation->getOpcode();
                if (op == llvm::Instruction::Xor ||
                    op == llvm::Instruction::And ||
                    op == llvm::Instruction::Or  ||
                    op == llvm::Instruction::Sub ||
                    op == llvm::Instruction::Add ||
                    op == llvm::Instruction::Mul
                )
                {
                    instructionsToObfuscate.push_back(binaryOperation);
                }
            }
            if (auto* icmp = llvm::dyn_cast<llvm::ICmpInst>(&instruction))
            {
                llvm::CmpInst::Predicate pred = icmp->getPredicate();
                if (pred == llvm::CmpInst::ICMP_EQ || pred == llvm::CmpInst::ICMP_NE)
                {
                    instructionsToObfuscate.push_back(icmp);
                }
            }
        }
    }

    if (instructionsToObfuscate.empty())
    {
        return; // nothing to do
    }

    for (auto& instruction : instructionsToObfuscate)
    {
        if (generator->DrawRange(0u, 100u) > attributes.mbaProbability)
            continue;
        
        ObfuscateInstruction(instruction, attributes.mbaExpansionCount);
    }

    // Verify the function at the end
    if (llvm::verifyFunction(function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] MBAPass: Function '" << function.getName() << "' verification failed after transformation!\n";

        // Dump the function IR and terminate
        
        std::error_code ec;
        llvm::raw_fd_ostream logFile("error_log.txt", ec);
        if (!ec)
        {
            function.print(logFile);
            logFile.close();
            llvm::errs() << "MBAPass: Function IR dumped to error_log.txt\n";
        }
        else
        {
            llvm::errs() << "MBAPass: Failed to open error_log.txt for writing: " << ec.message() << "\n";
        }
        exit(1);
    }
}

void LeetObfuscator::MBAPass::ObfuscateInstruction(llvm::Instruction *instruction, uint32_t expansionCount)
{
    if (auto* binaryOp = llvm::dyn_cast<llvm::BinaryOperator>(instruction))
    {
        ObfuscateBinaryOperation(binaryOp, expansionCount);
    }
    else if (auto* icmp = llvm::dyn_cast<llvm::ICmpInst>(instruction))
    {
        ObfuscateCompareOperation(icmp, expansionCount);
    }
}

void LeetObfuscator::MBAPass::ObfuscateBinaryOperation(llvm::Instruction *instruction, uint32_t expansionCount, uint32_t iteration)
{
    if (iteration >= expansionCount)
        return;

    uint32_t opcode = instruction->getOpcode();

    llvm::IRBuilder<llvm::NoFolder> b(instruction);
    
    llvm::Value* x = instruction->getOperand(0);
    llvm::Value* y = instruction->getOperand(1);

    InstructionHolder createdInstructions;
    llvm::Value* result = nullptr;
    switch(opcode)
    {
        case llvm::Instruction::Xor:
        {
            // x ^ y == (x | y) - (x & y)
            auto* orInst = createdInstructions.Add(b.CreateOr(x, y));
            auto* andInst = createdInstructions.Add(b.CreateAnd(x, y));
            result = createdInstructions.Add(b.CreateSub(orInst, andInst));

            break;
        }
        case llvm::Instruction::And:
        {
            // x & y == (x + y) - (x | y)
            auto* addInst = createdInstructions.Add(b.CreateAdd(x, y));
            auto* orInst = createdInstructions.Add(b.CreateOr(x, y));
            result = createdInstructions.Add(b.CreateSub(addInst, orInst));

            break;
        }
        case llvm::Instruction::Or:
        {
            // x | y == (x + y) - (x & y)
            auto* addInst = createdInstructions.Add(b.CreateAdd(x, y));
            auto* andInst = createdInstructions.Add(b.CreateAnd(x, y));
            result = createdInstructions.Add(b.CreateSub(addInst, andInst));

            break;
        }
        case llvm::Instruction::Sub:
        {
            // x - y == x + (~y + 1)
            auto* notInst = createdInstructions.Add(b.CreateNot(y));
            auto* addOneInst = createdInstructions.Add(b.CreateAdd(notInst, llvm::ConstantInt::get(y->getType(), 1)));
            result = createdInstructions.Add(b.CreateAdd(x, addOneInst));

            break;
        }
        case llvm::Instruction::Add:
        {
            // x + y == (x ^ y) + 2 * (x & y)
            auto* xorInst = createdInstructions.Add(b.CreateXor(x, y));
            auto* andInst = createdInstructions.Add(b.CreateAnd(x, y));
            auto* mulInst = createdInstructions.Add(b.CreateMul(andInst, llvm::ConstantInt::get(x->getType(), 2)));
            result = createdInstructions.Add(b.CreateAdd(xorInst, mulInst));

            break;
        }
        case llvm::Instruction::Mul:
        {
            // x * y == ((x | y) * (x & y)) + ((x & ~y) * (y & ~x))
            auto* orInst = createdInstructions.Add(b.CreateOr(x, y));
            auto* andInst = createdInstructions.Add(b.CreateAnd(x, y));
            auto* noty = createdInstructions.Add(b.CreateNot(y));
            auto* notx = createdInstructions.Add(b.CreateNot(x));
            auto* andNotx = createdInstructions.Add(b.CreateAnd(x, noty));
            auto* andNoty = createdInstructions.Add(b.CreateAnd(y, notx));
            auto* mul1 = createdInstructions.Add(b.CreateMul(orInst, andInst));
            auto* mul2 = createdInstructions.Add(b.CreateMul(andNotx, andNoty));
            result = createdInstructions.Add(b.CreateAdd(mul1, mul2));

            break;
        }
    }

    if (result == nullptr)
    {
        llvm::errs() << "failed to obfuscate binary op with opcode" << instruction->getOpcodeName() << "\n";
    }

    instruction->replaceAllUsesWith(result);
    instruction->eraseFromParent();

    for(auto* createdInstruction : createdInstructions.GetInstructions())
    {
        // mark it as done for future passes
        createdInstruction->setMetadata(ARITHMETIC_TAG, llvm::MDNode::get(createdInstruction->getContext(), {}));
    
        // Expand it further
        ObfuscateBinaryOperation(createdInstruction, expansionCount, iteration+1);
    }
}

void LeetObfuscator::MBAPass::ObfuscateCompareOperation(llvm::Instruction *instruction, uint32_t expansionCount, uint32_t iteration)
{
    if (iteration >= expansionCount)
        return;

    llvm::Value* x = instruction->getOperand(0);
    llvm::Value* y = instruction->getOperand(1);

    if (!x->getType()->isIntOrIntVectorTy())
        return;
    if (!y->getType()->isIntOrIntVectorTy())
        return;

    llvm::IRBuilder<llvm::NoFolder> b(instruction);
    InstructionHolder createdInstructions;
    llvm::Value* result = nullptr;

    switch (llvm::cast<llvm::ICmpInst>(instruction)->getPredicate())
    {
        case llvm::CmpInst::ICMP_EQ:
        {
            // a == b  <=>  (a ^ b) == 0
            auto* xorInst = createdInstructions.Add(b.CreateXor(x, y));
            result = createdInstructions.Add(
                b.CreateICmpEQ(xorInst, llvm::ConstantInt::get(x->getType(), 0)));
            break;
        }
        case llvm::CmpInst::ICMP_NE:
        {
            // a != b  <=>  (a ^ b) != 0
            auto* xorInst = createdInstructions.Add(b.CreateXor(x, y));
            result = createdInstructions.Add(
                b.CreateICmpNE(xorInst, llvm::ConstantInt::get(x->getType(), 0)));
            break;
        }
        default:
            return; // not handled
    }

    instruction->replaceAllUsesWith(result);
    instruction->eraseFromParent();

    for (auto* createdInstruction : createdInstructions.GetInstructions())
    {
        // mark it as done for future passes
        createdInstruction->setMetadata(ARITHMETIC_TAG, llvm::MDNode::get(createdInstruction->getContext(), {}));
    
        // Expand it further
        if (auto* binaryOp = llvm::dyn_cast<llvm::BinaryOperator>(createdInstruction))
        {
            ObfuscateBinaryOperation(binaryOp, expansionCount, iteration); // No +1 is intentional here
        }
        else if (auto* icmp = llvm::dyn_cast<llvm::ICmpInst>(createdInstruction))
        {
            ObfuscateCompareOperation(icmp, expansionCount, iteration + 1);
        }
    }
}

llvm::Value *LeetObfuscator::MBAPass::InstructionHolder::Add(llvm::Value *instruction)
{
    if (llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(instruction))
        m_CreatedInstructions.push_back(inst);

    return instruction;
}
