#include "AAMBAPass.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsX86.h"

#include "llvm/IR/Verifier.h"
#include "SettingsParser.h"

#include <random>
#include "RandomNumberGenerator.h"

llvm::PreservedAnalyses LeetObfuscator::AAMBAPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running AAMBAPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    for (auto& function : module)
    {
        ObfuscateFunction(function);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::AAMBAPass::ObfuscateFunction(llvm::Function& function)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(
        function, SettingsParser::PassType::AAMBAPass, m_Arguments
    );

    if (SettingsParser::ShouldSkipFunction(&function, attributes))
    {
        m_Logger.LogFunction(function, "Skipping function due to settings", 1);
        return;
    }

    m_Logger.LogFunction(function, "Processing function", 1);

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    // Find all binary instructions
    std::vector<llvm::Instruction*> instructionsToObfuscate;
    for (auto& basicBlock : function)
    {
        if (SettingsParser::ShouldSkipBlock(&basicBlock, attributes))
            continue;
        
        for (auto& instruction : basicBlock)
        {
            if (auto* binOp = llvm::dyn_cast<llvm::BinaryOperator>(&instruction))
            {
                if (generator->DrawRange(1u, 100u) > attributes.aambaProbability)
                    continue;

                instructionsToObfuscate.push_back(binOp);
            }
        }
    }

    m_Logger.LogFunction(function, "Found instructions to transform", 2);
    for (auto& instruction : instructionsToObfuscate)
    {
        m_Logger.LogInstruction(*instruction, "Obfuscating instruction", 3);
        ObfuscateInstruction(instruction, generator);
    }

    // Verify the function at the end
    if (llvm::verifyFunction(function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] AAMBAPass: Function '" << function.getName() << "' verification failed after transformation!\n";

        // Dump the function IR and terminate
        
        std::error_code ec;
        llvm::raw_fd_ostream logFile("error_log.txt", ec);
        if (!ec)
        {
            function.print(logFile);
            logFile.close();
            llvm::errs() << "AAMBAPass: Function IR dumped to error_log.txt\n";
        }
        else
        {
            llvm::errs() << "AAMBAPass: Failed to open error_log.txt for writing: " << ec.message() << "\n";
        }
        exit(1);
    }

}

void LeetObfuscator::AAMBAPass::ObfuscateInstruction(llvm::Instruction* instruction, std::shared_ptr<RandomNumberGenerator> generator)
{
    llvm::IRBuilder<llvm::NoFolder> b(instruction);
    
    llvm::Value* x = instruction->getOperand(0);
    llvm::Value* y = instruction->getOperand(1);

    auto AAMBA = [&](llvm::Value** value) -> bool
    {
        llvm::Value* orig = *value;
        llvm::Type* vType = orig->getType();
        llvm::LLVMContext& ctx = instruction->getContext();

        llvm::FunctionType* setcTy = llvm::FunctionType::get(llvm::Type::getInt8Ty(ctx), {}, false);
        std::string setcConstraints = "=q,~{cc}"; // any byte addressable GPR
        llvm::InlineAsm* setcAsm = llvm::InlineAsm::get(
            setcTy,
            "setc $0",
            setcConstraints,
            true,
            false,
            llvm::InlineAsm::AD_ATT
        );
        llvm::Value* rawBit = b.CreateCall(setcAsm, {});
        llvm::Value* bit = b.CreateZExt(rawBit, vType);

        // opacity barrier
        llvm::FunctionType* barrierTy = llvm::FunctionType::get(vType, {vType}, false);
        std::string barrierConstraints = "=r,0"; // output tied to input's register
        llvm::InlineAsm* barrierAsm = llvm::InlineAsm::get(
            barrierTy,
            "",
            barrierConstraints,
            true,
            false,
            llvm::InlineAsm::AD_ATT
        );
        auto opaque = [&](llvm::Value* v) { return b.CreateCall(barrierAsm, {v}); };

        llvm::Constant* C = llvm::ConstantInt::get(vType, 255); // TODO random constant

        enum class Shape : uint32_t
        {
            AddFirst = 0,
            SubFirst = 1
        };
        Shape shape = (Shape)generator->DrawRange<uint32_t>(0, 1);

        llvm::Value* result = nullptr;
        if (shape == Shape::AddFirst)
        {
            // ((x + C) + CF) - C - opaque(CF)  ==  x
            llvm::Value* t1 = b.CreateAdd(orig, C);
            llvm::Value* t2 = b.CreateAdd(t1, bit);
            llvm::Value* bitBar = opaque(bit);
            llvm::Value* t3 = b.CreateSub(t2, C);
            result = b.CreateSub(t3, bitBar);
        }
        else
        {
            // ((x - C) - CF) + C + opaque(CF)  ==  x
            llvm::Value* t1 = b.CreateSub(orig, C);
            llvm::Value* t2 = b.CreateSub(t1, bit);
            llvm::Value* bitBar = opaque(bit);
            llvm::Value* t3 = b.CreateAdd(t2, C);
            result = b.CreateAdd(t3, bitBar);
        }

        *value = result;
        return true;
    };

    llvm::Module* M = instruction->getModule();
    bool is64BitTarget = llvm::Triple(M->getTargetTriple()).isArch64Bit();

    auto isLegalForAsm = [&](llvm::Type* Ty) {
        if (Ty->isIntegerTy(32))
            return true;
        if (Ty->isIntegerTy(64) && is64BitTarget)
            return true;
        return false; // skip i64 on 32-bit, i128, etc.
    };

    uint32_t random = generator->DrawRange(0u, 1u);
    if (random % 2 == 0 && isLegalForAsm(x->getType()))
    {
        if (AAMBA(&x))
            instruction->setOperand(0, x);
    }
    if (random % 2 != 0 && isLegalForAsm(y->getType()))
    {
        if (AAMBA(&y))
            instruction->setOperand(1, y);
    }
}