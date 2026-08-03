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

    for (unsigned i = 0; i < callInst->arg_size(); i++)
    {
        if (callInst->paramHasAttr(i, llvm::Attribute::ByVal) ||
            callInst->paramHasAttr(i, llvm::Attribute::StructRet) ||
            callInst->paramHasAttr(i, llvm::Attribute::InAlloca) ||
            callInst->paramHasAttr(i, llvm::Attribute::Preallocated))
            return false;
    }
 
    for (auto& use : callInst->args())
    {
        llvm::Type* argTy = use->getType();
        if (argTy->isFloatTy() || argTy->isDoubleTy())
        {
            continue;
        }
        else if (argTy->isIntegerTy() || argTy->isPointerTy())
        {
            continue;
        }
        else
        {
            return false; // vector, long double, aggregate, or some other bullshit
        }
    }
 
    llvm::Type* retTy = callInst->getType();
    if (!retTy->isVoidTy() && !retTy->isIntegerTy() && !retTy->isPointerTy() && !retTy->isFloatTy() && !retTy->isDoubleTy())
        return false;
 
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
    do
    {
        nanomiteId = generator.DrawRange(1u, std::numeric_limits<uint32_t>::max());;
    }
    while (std::find(allIds.begin(), allIds.end(), nanomiteId) != allIds.end());

    allIds.push_back(nanomiteId);
    return nanomiteId;
}

llvm::Function* LeetObfuscator::NanomitesPass::CreateTrampoline(llvm::Module& module, llvm::Function* realFunc)
{
    llvm::LLVMContext& context = module.getContext();

    llvm::FunctionType* trampolineTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);

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
        MakeIdTrailer(0); // Empty ID, exception handler will pop the address from the stack

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

    // Don't obfuscate exception stuff because it will break, also skip dispatcher barriers because there's too
    // many of them and it will be slow, they're also empty calls so what's the point
    if (function->getName().find("__leet_dispatcher_barrier") != std::string::npos ||
        function->getName().find("__leet_exception") != std::string::npos ||
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
                // Again, skip llvm and our own stuff
                if (callInst->getCalledFunction() &&
                    callInst->getCalledFunction()->getName().find("llvm.") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("__leet_dispatcher_barrier") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("__leet_exception") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("__leet_trampoline") == std::string::npos &&
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

    llvm::StructType* entryType = llvm::StructType::get(
        context,
        {llvm::Type::getInt32Ty(context), llvm::PointerType::get(context, 0)}
    );

    auto makeEntry = [&](uint32_t id, llvm::Constant* addr) -> llvm::Constant*
    {
        return llvm::ConstantStruct::get(
            entryType,
            {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), id), addr}
        );
    };

    for (auto* callInst : instructions)
    {
        llvm::Function* realFunc = callInst->getCalledFunction();

        uint32_t callSiteId = GenerateUniqueNanomiteId(*generator);

        std::cout << "FOUND ONE " << callSiteId << " | " << function->getName().str() << " | " << callInst->getCalledFunction()->getName().str() << std::endl;

        llvm::IRBuilder<> builder(callInst);

        // Get all arguments
        std::vector<llvm::Value*> argValues;
        std::vector<llvm::Type*> argTypes;
        std::vector<llvm::Value*> stackArgValues;
        std::vector<llvm::Type*> stackArgTypes;
        uint32_t intCounter = 0;
        uint32_t floatCounter = 0;
        for (uint32_t i = 0; i < callInst->arg_size(); i++)
        {
            llvm::Type* type = callInst->getArgOperand(i)->getType();

            if (type->isFloatTy() || type->isDoubleTy())
            {
                if (floatCounter >= 8)
                {
                    stackArgValues.push_back(callInst->getArgOperand(i));
                    stackArgTypes.push_back(type);
                }
                else
                {
                    argValues.push_back(callInst->getArgOperand(i));
                    argTypes.push_back(type);
                }
                floatCounter++;
            }
            else
            {
                if (intCounter >= 6)
                {
                    stackArgValues.push_back(callInst->getArgOperand(i));
                    stackArgTypes.push_back(type);
                }
                else
                {
                    argValues.push_back(callInst->getArgOperand(i));
                    argTypes.push_back(type);
                }
                intCounter++;
            }
        }
        std::cout << "Number of stack args: " << stackArgValues.size() << std::endl;
        bool hasRet = !callInst->getType()->isVoidTy();
        llvm::Type* retType = hasRet ? callInst->getType() : builder.getVoidTy();
        bool isRetFloatOrDouble = retType->isFloatTy() || retType->isDoubleTy();

        uint32_t stackArgCount = (uint32_t)stackArgValues.size();
        uint32_t stackPad = (stackArgCount % 2 != 0) ? 8u : 0u;
        uint32_t stackAdjust = stackArgCount * 8u + stackPad;

        bool needGprScratch = false;
        bool needXmmScratch = false;
        for (llvm::Type* t : stackArgTypes)
        {
            if (t->isFloatTy() || t->isDoubleTy())
                needXmmScratch = true;
            else
                needGprScratch = true;
        }

        // Dummy outputs are needed for the arguments, otherwise LLVM will assume that their value didn't change
        std::vector<llvm::Type*> outFieldTypes;
        if (hasRet)
            outFieldTypes.push_back(retType);
        for (uint32_t i = 0; i < argValues.size(); i++)
            outFieldTypes.push_back(argTypes[i]);
        
        if (needGprScratch)
            outFieldTypes.push_back(llvm::Type::getInt64Ty(context));
        if (needXmmScratch)
            outFieldTypes.push_back(llvm::Type::getDoubleTy(context));

        // These dummy outputs will all be in a single struct
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
            appendConstraint(isRetFloatOrDouble ? "={xmm0}" : "={rax}");

        floatCounter = 0;
        intCounter = 0;
        for (uint32_t i = 0; i < argValues.size(); i++)
        {
            if (argTypes[i]->isFloatTy() || argTypes[i]->isDoubleTy())
            {
                appendConstraint("={xmm" + std::to_string(floatCounter) + "}");
                floatCounter++;
            }
            else
            {
                appendConstraint(std::string("=") + kIntArgRegs[intCounter]);
                intCounter++;
            }
        }

        // Add constraint for clobber and save its index in the constraint
        uint32_t scratchGprIdx = 0;
        uint32_t scratchXmmIdx = 0;
        if (needGprScratch)
        {
            scratchGprIdx = (hasRet ? 1u : 0u) + (uint32_t)argValues.size();
            appendConstraint("=&r");
        }
        if (needXmmScratch)
        {
            scratchXmmIdx = (hasRet ? 1u : 0u) + (uint32_t)argValues.size() + (needGprScratch ? 1u : 0u);
            appendConstraint("=&x");
        }

        uint32_t numOutputs = (hasRet ? 1u : 0u) + (uint32_t)argValues.size() + (needGprScratch ? 1u : 0u) + (needXmmScratch ? 1u : 0u);

        // Also declare the dummy outputs as inputs, they're still arguments after all
        uint32_t tiedBase = hasRet ? 1 : 0;
        for (uint32_t i = 0; i < argValues.size(); i++)
            appendConstraint(std::to_string(tiedBase + i));

        uint32_t memInputBase = numOutputs + static_cast<uint32_t>(argValues.size());
        for (uint32_t i = 0; i < stackArgCount; i++)
            appendConstraint("m");

        // Then clobbers. Clobber EVERYTHING that's left, we have no fucking clue what the function potentially does
        // and what the values of these registers will be after the call

        if (!hasRet || isRetFloatOrDouble) // clobber rax if it's not an output
            appendConstraint("~{rax}");

        for (uint32_t i = intCounter; i < 6; i++)
            appendConstraint(std::string("~") + kIntArgRegs[i]);

        // Leave one xmm register out of the clobber list when we need it as scratch -
        // otherwise, whenever floats overflow to the stack (meaning xmm0-7 are all
        // already pinned by real float args), this loop would clobber xmm8-15 too and
        // leave nothing for "=&x" to allocate.
        uint32_t floatClobberLimit = needXmmScratch ? 15u : 16u;
        for (uint32_t i = floatCounter; i < floatClobberLimit; i++)
        {
            if (i == 0 && isRetFloatOrDouble)
                continue;
            appendConstraint("~{xmm" + std::to_string(i) + "}");
        }

        appendConstraint("~{r10}");
        appendConstraint("~{r11}");
        appendConstraint("~{memory}");
        appendConstraint("~{dirflag}");
        appendConstraint("~{fpsr}");
        appendConstraint("~{flags}");

        // Build the asm text
        std::string asmText;
        if (stackAdjust > 0)
            asmText += "sub $$" + std::to_string(stackAdjust) + ", %rsp\n\t";

        // All stack arugments have to be written below RSP
        uint32_t offset = 0;
        for (uint32_t i = 0; i < stackArgCount; i++)
        {
            uint32_t memIdx = memInputBase + i;
            llvm::Type* t = stackArgTypes[i];

            llvm::Value* v = stackArgValues[i];
            bool isImmiediate = false;
            std::string valueText;

            if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(v))
            {
                isImmiediate = true;
                llvm::raw_string_ostream os(valueText);
                ci->getValue().print(os, /*isSigned=*/true);
            }

            if (isImmiediate)
            {
                // Integers and pointers can be passed to the stack without a scratch register
                uint32_t bits = t->isPointerTy() ? 64 : t->getIntegerBitWidth();
                std::string mnemonic = (bits <= 8) ? "movb" : (bits <= 16) ? "movw" : (bits <= 32) ? "movl" : "movq";

                // mov $imm, offset(%rsp)
                asmText += mnemonic + " $$" + valueText + ", " + std::to_string(offset) + "(%rsp)\n\t";
                std::cout << mnemonic + " $$" + valueText + ", " + std::to_string(offset) + "(%rsp)" << std::endl;
            }
            else if (t->isFloatTy())
            {
                asmText += "movss $" + std::to_string(memIdx) + ", $" + std::to_string(scratchXmmIdx) + "\n\t";
                asmText += "movss $" + std::to_string(scratchXmmIdx) + ", " + std::to_string(offset) + "(%rsp)\n\t";
            }
            else if (t->isDoubleTy())
            {
                asmText += "movsd $" + std::to_string(memIdx) + ", $" + std::to_string(scratchXmmIdx) + "\n\t";
                asmText += "movsd $" + std::to_string(scratchXmmIdx) + ", " + std::to_string(offset) + "(%rsp)\n\t";
            }
            else if (t->isPointerTy())
            {
                asmText += "movq $" + std::to_string(memIdx) + ", $" + std::to_string(scratchGprIdx) + "\n\t";
                asmText += "movq $" + std::to_string(scratchGprIdx) + ", " + std::to_string(offset) + "(%rsp)\n\t";
            }
            else
            {
                uint32_t bits = t->getIntegerBitWidth();
                std::string mnemonic = (bits <= 8) ? "movb" : (bits <= 16) ? "movw" : (bits <= 32) ? "movl" : "movq";
                std::string mod = (bits <= 8) ? "b" : (bits <= 16) ? "w" : (bits <= 32) ? "k" : "";
                std::string scratchRef = mod.empty()
                    ? ("$" + std::to_string(scratchGprIdx))
                    : ("${" + std::to_string(scratchGprIdx) + ":" + mod + "}"); // ${N:b/w/k} = 8/16/32 bit view of the scratch reg

                asmText += mnemonic + " $" + std::to_string(memIdx) + ", " + scratchRef + "\n\t";
                asmText += mnemonic + " " + scratchRef + ", " + std::to_string(offset) + "(%rsp)\n\t";
            }

            offset += 8;
        }

        asmText += "int3\n\t" + MakeIdTrailer(callSiteId);

        if (stackAdjust > 0)
            asmText += "\n\tadd $$" + std::to_string(stackAdjust) + ", %rsp";

        llvm::FunctionType* asmFuncType;
        {
            std::vector<llvm::Type*> allInputTypes = argTypes;
            allInputTypes.insert(allInputTypes.end(), stackArgTypes.begin(), stackArgTypes.end());
            asmFuncType = llvm::FunctionType::get(asmRetType, allInputTypes, false);
        }

        llvm::InlineAsm* trapAsm = llvm::InlineAsm::get(asmFuncType, asmText, constraints, /*hasSideEffects*/true);

        std::vector<llvm::Value*> allInputValues = argValues;
        allInputValues.insert(allInputValues.end(), stackArgValues.begin(), stackArgValues.end());

        llvm::CallInst* trapCall = builder.CreateCall(trapAsm, allInputValues);

        // Because the dummy outputs were declared, the function will return a structure of rax + dummy outputs
        // we don't care about dummy stuff so just extract rax from the struct
        if (hasRet)
        {
            llvm::Value* actualRet = (outFieldTypes.size() == 1) ? (llvm::Value*)trapCall : builder.CreateExtractValue(trapCall, {0});
            callInst->replaceAllUsesWith(actualRet);
        }
        callInst->eraseFromParent();

        llvm::Function* trampoline = CreateTrampoline(*module, realFunc);

        // call inside the trampoline block is technically orphaned, so make sure it's not opted out
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