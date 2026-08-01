#include "NanomitesPass.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Attributes.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/IR/Verifier.h"

#include "SettingsParser.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

static const char* kIntArgRegs[6] = {
    "{rdi}", "{rsi}", "{rdx}", "{rcx}", "{r8}", "{r9}"
};

llvm::PreservedAnalyses LeetObfuscator::NanomitesPass::run(llvm::Module& module, llvm::ModuleAnalysisManager&)
{
    std::cout << "NANOMITES PASS" << std::endl;

    std::vector<llvm::Constant*> nanomitesEntries;

    srand(time(nullptr));
    for (auto& function : module)
    {
        ObfuscateFunction(&function, nanomitesEntries);
    }

    if (!nanomitesEntries.empty())
    {
        CreateGlobalNanomitesTable(module, nanomitesEntries);
    }

    return llvm::PreservedAnalyses::none();
}

bool LeetObfuscator::NanomitesPass::CanObfuscateCallSignature(llvm::CallInst* callInst)
{
    if (callInst->getFunctionType()->isVarArg())
        return false;

    unsigned intArgCount = 0;
    for (auto& use : callInst->args())
    {
        llvm::Type* argTy = use->getType();
        if (!argTy->isIntegerTy() && !argTy->isPointerTy())
            return false; // float/double/struct/vector not handled yet
        if (++intArgCount > 6)
            return false; // would need stack slots? Not supported yet
    }

    llvm::Type* retTy = callInst->getType();
    if (!retTy->isVoidTy() && !retTy->isIntegerTy() && !retTy->isPointerTy())
        return false; // floats not supported yet

    return true;
}

std::string LeetObfuscator::NanomitesPass::MakeIdTrailer(uint32_t nanomiteId)
{
    // E8 - call with garbage
    // 0x0F - extended opcode, will mess up next instructions creating some bullshit for the decompiler
    return ".byte 0xE8," + std::to_string(nanomiteId & 0xFF) + "," +
        std::to_string((nanomiteId >> 8) & 0xFF) + "," +
        std::to_string((nanomiteId >> 16) & 0xFF) + "," +
        std::to_string((nanomiteId >> 24) & 0xFF) + ",0x0F";
}

uint32_t LeetObfuscator::NanomitesPass::GenerateUniqueNanomiteId(RandomNumberGenerator& generator)
{
    static std::vector<uint32_t> allIds;

    uint32_t nanomiteId;
    do {
        nanomiteId = generator.DrawRange(1u, std::numeric_limits<uint32_t>::max());;
    } while (std::find(allIds.begin(), allIds.end(), nanomiteId) != allIds.end());

    allIds.push_back(nanomiteId);
    return nanomiteId;
}

llvm::Function* LeetObfuscator::NanomitesPass::CreateTrampoline(llvm::Module& module, llvm::Function* realFunc)
{
    llvm::LLVMContext& context = module.getContext();

    llvm::FunctionType* trampolineTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);

    // Use the actual function name in the trampoline name to ensure uniqueness
    std::string trampolineName = "__leet_trampoline_" + realFunc->getName().str();
    
    llvm::Function* trampoline = llvm::Function::Create(
        trampolineTy,
        llvm::GlobalValue::PrivateLinkage,
        trampolineName,
        module
    );

    trampoline->addFnAttr(llvm::Attribute::Naked);
    trampoline->addFnAttr(llvm::Attribute::NoInline);

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", trampoline);
    llvm::IRBuilder<> builder(entry);
    
    // Mangled names can contian $ in them, so change it to $$ so llvm doesn't cry
    std::string target = realFunc->getName().str();
    for (size_t pos = 0; (pos = target.find('$', pos)) != std::string::npos; pos += 2)
        target.replace(pos, 1, "$$");

    std::string asmText = 
        "call " + target + "\n\t" +
        "int3\n\t" + 
        MakeIdTrailer(0); // Empty ID

    llvm::InlineAsm* trampolineAsm = llvm::InlineAsm::get(
        llvm::FunctionType::get(builder.getVoidTy(), false),
        asmText,
        "",
        /*hasSideEffects*/ true
    );

    builder.CreateCall(trampolineAsm);
    builder.CreateUnreachable();

    // Trampoline block is technically orphaned, so make sure it's not opted out
    llvm::appendToCompilerUsed(module, {trampoline});

    return trampoline;
}

void LeetObfuscator::NanomitesPass::ObfuscateFunction(llvm::Function *function, std::vector<llvm::Constant*>& nanomitesEntries)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(
        *function,
        SettingsParser::PassType::NanomitesPass,
        m_Arguments
    );

    if(SettingsParser::ShouldSkipFunction(function, attributes))
        return;

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    std::vector<llvm::CallInst*> instructions;

    if (function->getName().find("__leet_dispatcher_barrier") != std::string::npos ||
        function->getName().find("__leet_exception") != std::string::npos ||
        function->getName().find("_GLOBAL_") != std::string::npos || // TODO
        function->getName().find("__leet_trampoline") != std::string::npos
    )
        return;

    for (auto& basicBlock : *function)
    {
        for (auto& inst : basicBlock)
        {
            llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(&inst);
            if (callInst)
            {
                if (callInst->getCalledFunction() &&
                    callInst->getCalledFunction()->getName().find("llvm.") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("__leet_dispatcher_barrier") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("__leet_exception") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("__leet_trampoline") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("_GLOBAL_") == std::string::npos &&
                    callInst->getCalledFunction()->getName() != "sigaction" &&
                    callInst->getCalledFunction()->getName() != "sigemptyset"
                )
                {
                    if (!CanObfuscateCallSignature(callInst))
                    {
                        std::cout << "SKIPPING (unsupported signature) " << function->getName().str()
                            << " | " << callInst->getCalledFunction()->getName().str() << std::endl;
                        continue;
                    }

                    if (generator->DrawRange(0u, 100u) > attributes.NanomitesProbability)
                        continue;

                    instructions.push_back(callInst);
                    //std::cout << "FOUND ONE " << function->getName().str() << " | " << callInst->getCalledFunction()->getName().str() << std::endl;
                }
            }
        }
    }

    if (instructions.empty())
        return;

    // can't use redzone, the function in the trampoline will overwrite everything in there if it has it's own stack frame
    function->addFnAttr(llvm::Attribute::NoRedZone);

    llvm::LLVMContext& context = function->getContext();
    llvm::Module* module = function->getParent();

    llvm::StructType* entryType = llvm::StructType::get(context, {
        llvm::Type::getInt32Ty(context),
        llvm::PointerType::get(context, 0)
    });

    auto makeEntry = [&](uint32_t id, llvm::Constant* addr) -> llvm::Constant* {
        return llvm::ConstantStruct::get(entryType, {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), id),
            addr
        });
    };

    for (auto* callInst : instructions)
    {
        llvm::Function* realFunc = callInst->getCalledFunction();

        uint32_t callSiteId = GenerateUniqueNanomiteId(*generator);

        std::cout << "FOUND ONE " << callSiteId << " | " << function->getName().str() << " | " << callInst->getCalledFunction()->getName().str() << std::endl;

        llvm::IRBuilder<> builder(callInst);

        std::vector<llvm::Value*> argValues;
        std::vector<llvm::Type*> argTypes;
        for (unsigned i = 0; i < callInst->arg_size(); ++i)
        {
            argValues.push_back(callInst->getArgOperand(i));
            argTypes.push_back(callInst->getArgOperand(i)->getType());
        }
        std::string asmText = "int3\n\t" + MakeIdTrailer(callSiteId);

        bool hasRet = !callInst->getType()->isVoidTy();
        llvm::Type* retType = hasRet ? callInst->getType() : builder.getVoidTy();

        // Create an int3 to the trampoline
        std::vector<llvm::Type*> outFieldTypes;
        if (hasRet)
            outFieldTypes.push_back(retType);
        for (unsigned i = 0; i < argValues.size(); ++i)
            outFieldTypes.push_back(argTypes[i]);

        llvm::Type* asmRetType;
        if (outFieldTypes.empty())
            asmRetType = builder.getVoidTy();
        else if (outFieldTypes.size() == 1)
            asmRetType = outFieldTypes[0];
        else
            asmRetType = llvm::StructType::get(context, outFieldTypes);

        std::string constraints;
        bool firstConstraint = true;
        auto appendConstraint = [&](const std::string& c) {
            if (!firstConstraint)
                constraints += ",";
            constraints += c;
            firstConstraint = false;
        };

        // Outputs first
        if (hasRet)
        {
            appendConstraint("={rax}");
        }
        for (unsigned i = 0; i < argValues.size(); ++i)
        {
            // dummy output, still has to be added to constraints. Without it the arguments will only be marked as input
            // but in reality they can also get overwritten, by doing a dummy output the compiler will know the argument
            // possibly gets destroyed
            appendConstraint(std::string("=") + kIntArgRegs[i]);
        }

        // Also declare the dummy outputs as inputs, they're still arguments after all
        unsigned tiedBase = hasRet ? 1 : 0;
        for (unsigned i = 0; i < argValues.size(); ++i)
            appendConstraint(std::to_string(tiedBase + i));

        // Then clobbers. Clobber EVERYTHING we have no fucking clue what the function potentially does.
        if (!hasRet)
            appendConstraint("~{rax}");

        // usused args registers, can still be clobbered just to be safe tho
        for (unsigned i = argValues.size(); i < 6; ++i)
            appendConstraint(std::string("~") + kIntArgRegs[i]);

        appendConstraint("~{r10}");
        appendConstraint("~{r11}");
        for (unsigned i = 0; i < 16; ++i)
            appendConstraint("~{xmm" + std::to_string(i) + "}");
        appendConstraint("~{memory}");
        appendConstraint("~{dirflag}");
        appendConstraint("~{fpsr}");
        appendConstraint("~{flags}");

        llvm::FunctionType* asmFuncType = llvm::FunctionType::get(asmRetType, argTypes, false);
        llvm::InlineAsm* trapAsm = llvm::InlineAsm::get(asmFuncType, asmText, constraints, /*hasSideEffects*/true);

        llvm::CallInst* trapCall = builder.CreateCall(trapAsm, argValues);

        // Because the dummy outputs were declared, the function will return a structure of rax + dummy outputs
        // we don't care about dummy stuff so just extract rax from the struct
        if (hasRet)
        {
            llvm::Value* actualRet = (outFieldTypes.size() == 1) ? (llvm::Value*)trapCall : builder.CreateExtractValue(trapCall, {0});
            callInst->replaceAllUsesWith(actualRet);
        }
        callInst->eraseFromParent();

        llvm::Function* trampoline = CreateTrampoline(*module, realFunc);

        // Trampoline block is technically orphaned, so make sure it's not opted out
        llvm::appendToCompilerUsed(*module, {realFunc});

        nanomitesEntries.push_back(makeEntry(callSiteId, trampoline));
    }

    // Verify the function at the end
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] NanomitePass: Function '" << function->getName() << "' verification failed after transformation!\n";

        // Dump the function IR and terminate
        
        std::error_code ec;
        llvm::raw_fd_ostream logFile("error_log.txt", ec);
        if (!ec)
        {
            function->print(logFile);
            logFile.close();
            llvm::errs() << "NanomitePass: Function IR dumped to error_log.txt\n";
        }
        else
        {
            llvm::errs() << "NanomitePass: Failed to open error_log.txt for writing: " << ec.message() << "\n";
        }
        exit(1);
    }

    //std::cout << "REPLACED " << instructions.size() << " calls" << std::endl;

}

void LeetObfuscator::NanomitesPass::CreateGlobalNanomitesTable(llvm::Module& module, std::vector<llvm::Constant*>& nanomitesEntries)
{
    llvm::LLVMContext& context = module.getContext();

    llvm::StructType* entryType = llvm::StructType::get(context, {
        llvm::Type::getInt32Ty(context),
        llvm::PointerType::get(context, 0)
    });

    llvm::ArrayType* tableType = llvm::ArrayType::get(entryType, nanomitesEntries.size());

    llvm::GlobalVariable* nanomitesTable = module.getGlobalVariable("__nanomites_table");
    if (!nanomitesTable)
    {
        nanomitesTable = new llvm::GlobalVariable(
            module,
            tableType,
            false, // not constant - may need to be modified at runtime
            llvm::GlobalValue::ExternalLinkage, // External linkage so it can be accessed from other compilation units
            nullptr, // Initializer set later
            "__nanomites_table"
        );
    }

    nanomitesTable->setInitializer(llvm::ConstantArray::get(tableType, nanomitesEntries));

    nanomitesTable->setSection(".nanomites");

    nanomitesTable->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::None);
    nanomitesTable->setVisibility(llvm::GlobalValue::DefaultVisibility);
    nanomitesTable->setDSOLocal(false);

    llvm::GlobalVariable* nanomitesTableSize = module.getGlobalVariable("__nanomites_table_size");
    if (!nanomitesTableSize)
    {
        nanomitesTableSize = new llvm::GlobalVariable(
            module,
            llvm::Type::getInt32Ty(context),
            true, // is constant
            llvm::GlobalValue::ExternalLinkage,
            nullptr, // Initializer set later
            "__nanomites_table_size"
        );
    }

    nanomitesTableSize->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), nanomitesEntries.size()));

    nanomitesTableSize->setSection(".nanomites");
    nanomitesTableSize->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::None);
    nanomitesTableSize->setVisibility(llvm::GlobalValue::DefaultVisibility);
    nanomitesTableSize->setDSOLocal(false);

    std::cout << "Created global nanomites table with " << nanomitesEntries.size() << " entries in section .nanomites" << std::endl;
    std::cout << "Table global: " << nanomitesTable << " Size global: " << nanomitesTableSize << std::endl;
}