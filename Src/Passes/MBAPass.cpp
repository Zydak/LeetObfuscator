#include "MBAPass.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"

static constexpr const char *ARITHMETIC_TAG = "obfuscator.arithmetic";

llvm::PreservedAnalyses LeetObfuscator::MBAPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    InstructionHolder createdInstructions;
    ObfuscateModule(module);

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::MBAPass::ObfuscateModule(llvm::Module &module)
{
    // Find all binary instructions
    std::vector<llvm::Instruction*> instructionsToObfuscate;
    for (auto& function : module)
    {
        for (auto& basicBlock : function)
        {
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
            }
        }
    }

    if (instructionsToObfuscate.empty())
    {
        return; // nothing to do
    }

    for (auto& instruction : instructionsToObfuscate)
    {
        ObfuscateInstruction(instruction);
    }
}

void LeetObfuscator::MBAPass::ObfuscateInstruction(llvm::Instruction *instruction)
{
    if (auto* binaryOp = llvm::dyn_cast<llvm::BinaryOperator>(instruction))
    {
        ObfuscateBinaryInstruction(binaryOp);
    }
}

void LeetObfuscator::MBAPass::ObfuscateBinaryInstruction(llvm::Instruction *instruction, uint32_t iteration)
{
    if (iteration >= m_MaxPassCount)
        return;

    uint32_t opcode = instruction->getOpcode();

    llvm::Value* x = instruction->getOperand(0);
    llvm::Value* y = instruction->getOperand(1);

    llvm::IRBuilder<llvm::NoFolder> b(instruction);

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
        ObfuscateBinaryInstruction(createdInstruction, iteration+1);
    }
}

llvm::Value *LeetObfuscator::MBAPass::InstructionHolder::Add(llvm::Value *instruction)
{
    if (llvm::Instruction* inst = llvm::dyn_cast<llvm::Instruction>(instruction))
        m_CreatedInstructions.push_back(inst);

    return instruction;
}
