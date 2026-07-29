#include "AAMBAPass.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsX86.h"

#include "llvm/IR/Verifier.h"
#include "SettingsParser.h"

#include <random>

static constexpr const char *ARITHMETIC_TAG = "obfuscator.arithmetic";

llvm::PreservedAnalyses LeetObfuscator::AAMBAPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running AAMBAPass\n";

    for (auto& function : module)
    {
        ObfuscateFunction(function);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::AAMBAPass::ObfuscateFunction(llvm::Function& function)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(
        function, SettingsParser::PassType::AAMBAPass, m_Arguments);
    if (attributes.skip)
    {
        return;
    }

    // Find all binary instructions
    std::vector<llvm::Instruction*> instructionsToObfuscate;
    for (auto& basicBlock : function)
    {
        for (auto& instruction : basicBlock)
        {
            if (instruction.getMetadata(ARITHMETIC_TAG))
            {
                instructionsToObfuscate.push_back(&instruction); // Obfuscate only prev MBA pass
            }
        }
    }

    for (auto& instruction : instructionsToObfuscate)
    {
        ObfuscateInstruction(instruction);
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

void LeetObfuscator::AAMBAPass::ObfuscateInstruction(llvm::Instruction* instruction)
{
    llvm::LLVMContext& context = instruction->getContext();

    llvm::IRBuilder<llvm::NoFolder> b(instruction);
    
    llvm::Value* x = instruction->getOperand(0);
    llvm::Value* y = instruction->getOperand(1);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> dist(0, 1);

    auto AAMBA = [&](llvm::Value** value)
    {
        llvm::Type* vType = (*value)->getType();
        llvm::FunctionType *FTy = llvm::FunctionType::get(vType, {vType}, false);

        enum class ArchPrimitive : uint32_t { ADC = 0, SBB = 1 };
        ArchPrimitive primitive = static_cast<ArchPrimitive>(dist(rng));

        const char *AsmText = nullptr;

        switch (primitive)
        {
            case ArchPrimitive::ADC:
            {
                const char *Asm32 = R"(
                    mov $1, $0
                    setc %cl
                    movzbl %cl, %ecx
                    adc $$255, $0
                    sub $$255, $0
                    sub %ecx, $0
                )";

                const char *Asm64 = R"(
                    mov $1, $0
                    setc %cl
                    movzbl %cl, %ecx
                    adc $$255, $0
                    sub $$255, $0
                    sub %rcx, $0
                )";

                AsmText = vType->isIntegerTy(64) ? Asm64 : Asm32;
                break;
            }

            case ArchPrimitive::SBB:
            {
                const char *Asm32 = R"(
                    mov $1, $0
                    setc %cl
                    movzbl %cl, %ecx
                    sbb $$255, $0
                    add $$255, $0
                    add %ecx, $0
                )";

                const char *Asm64 = R"(
                    mov $1, $0
                    setc %cl
                    movzbl %cl, %ecx
                    sbb $$255, $0
                    add $$255, $0
                    add %rcx, $0
                )";

                AsmText = vType->isIntegerTy(64) ? Asm64 : Asm32;
                break;
            }
        }

        llvm::InlineAsm *Asm = llvm::InlineAsm::get(
            FTy, AsmText, "=r,r,~{rcx},~{cc}",
            /*hasSideEffects=*/true, /*isAlignStack=*/false,
            llvm::InlineAsm::AD_ATT
        );

        llvm::Value *result = b.CreateCall(Asm, {*value});
        *value = result;
    };
    
    int random = rand(); // TODO: proepr distribution with settings
    if (random % 2 == 0)
    {
        random = rand();
        if (random % 2 == 0 && (x->getType() == llvm::Type::getInt32Ty(context) || x->getType() == llvm::Type::getInt64Ty(context)))
        {
            AAMBA(&x);
            instruction->setOperand(0, x);
        }
        if (random % 2 != 0 && (y->getType() == llvm::Type::getInt32Ty(context) || y->getType() == llvm::Type::getInt64Ty(context)))
        {
            AAMBA(&y);
            instruction->setOperand(1, y);
        }
    }
}
