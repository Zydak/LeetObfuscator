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

#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/InlineAsm.h"

#include "SettingsParser.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

llvm::PreservedAnalyses LeetObfuscator::NanomitesPass::run(llvm::Module& module, llvm::ModuleAnalysisManager&)
{
    std::cout << "NANOMITES PASS" << std::endl;

    std::vector<llvm::Constant*> nanomitesEntries;

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
        function->getName().find("__leet_forward") != std::string::npos ||
        function->getName().find("__leet_split_mix_64") != std::string::npos ||
        function->getName().find("__leet_forward") != std::string::npos ||
        function->getName().find("sigemptyset") != std::string::npos ||
        function->getName().find("sigaction") != std::string::npos
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
                    callInst->getCalledFunction()->getName().find("__leet_forward") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("__leet_split_mix_64") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("sigemptyset") == std::string::npos &&
                    callInst->getCalledFunction()->getName().find("sigaction") == std::string::npos
                )
                {
                    if (generator->DrawRange(0u, 100u) > attributes.NanomitesProbability)
                        continue;

                    instructions.push_back(callInst);
                }
            }
        }
    }

    if (instructions.empty())
        return;

    function->addFnAttr(llvm::Attribute::NoInline);
    function->addFnAttr(llvm::Attribute::NoDuplicate);

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

        llvm::Function* forwardFunc = CreateForwardFunction(*module, realFunc, callSiteId);

        std::vector<llvm::Value*> args;
        args.reserve(callInst->arg_size());
        for (uint32_t i = 0; i < callInst->arg_size(); ++i)
        {
            args.push_back(callInst->getArgOperand(i));
        }
        
        llvm::Value* forwardCallResult = builder.CreateCall(forwardFunc, args);

        if (!forwardFunc->getReturnType()->isVoidTy())
        {
            callInst->replaceAllUsesWith(forwardCallResult);
        }
        callInst->eraseFromParent();

        // call inside the trampoline block is technically orphaned, so make sure it's not opted out
        llvm::appendToCompilerUsed(*module, {realFunc});

        nanomitesEntries.push_back(makeEntry(callSiteId, forwardFunc));
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

llvm::Function* LeetObfuscator::NanomitesPass::CreateForwardFunction(llvm::Module& module, llvm::Function* realFunc, uint32_t id)
{
    llvm::LLVMContext& context = module.getContext();
    std::string forwardFuncName = "__leet_forward_func_" + std::to_string(id);
    
    llvm::Function* forwardFunc = llvm::Function::Create(
        realFunc->getFunctionType(),
        llvm::GlobalValue::InternalLinkage,
        forwardFuncName,
        module
    );

    forwardFunc->addFnAttr(llvm::Attribute::NoInline);
    forwardFunc->addFnAttr(llvm::Attribute::NoDuplicate);

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", forwardFunc);

    llvm::IRBuilder<> builder(entry);
    std::vector<llvm::Value*> args;
    for (auto& arg : forwardFunc->args())
    {
        args.push_back(&arg);
    }

    llvm::Value* result = builder.CreateCall(realFunc, args);
    
    if (forwardFunc->getReturnType()->isVoidTy())
    {
        builder.CreateRetVoid();
    }
    else
    {
        builder.CreateRet(result);
    }

    return forwardFunc;
}

void LeetObfuscator::NanomitesPass::CreateGlobalNanomitesTable( llvm::Module& module, std::vector<llvm::Constant*>& nanomitesEntries)
{
    llvm::LLVMContext& context = module.getContext();

    llvm::StructType* entryType = llvm::StructType::get(context, {
        llvm::Type::getInt32Ty(context),
        llvm::PointerType::get(context, 0)
    });

    llvm::ArrayType* tableType = llvm::ArrayType::get(entryType, nanomitesEntries.size());

    llvm::GlobalVariable* oldTable = module.getGlobalVariable("__nanomites_table", /*AllowLocal=*/true);

    auto* newTable = new llvm::GlobalVariable(
        module,
        tableType,
        true,
        llvm::GlobalValue::ExternalLinkage,
        llvm::ConstantArray::get(tableType, nanomitesEntries),
        "__nanomites_table"
    );

    newTable->setSection(".nanomites");
    newTable->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::None);
    newTable->setVisibility(llvm::GlobalValue::DefaultVisibility);
    newTable->setDSOLocal(false);

    if (oldTable)
    {
        oldTable->replaceAllUsesWith(newTable);
        oldTable->eraseFromParent();
    }

    llvm::GlobalVariable* oldSize =module.getGlobalVariable("__nanomites_table_size", true);

    auto* newSize = new llvm::GlobalVariable(
        module,
        llvm::Type::getInt32Ty(context),
        true,
        llvm::GlobalValue::ExternalLinkage,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), nanomitesEntries.size()),
        "__nanomites_table_size"
    );

    newSize->setSection(".nanomites");
    newSize->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::None);
    newSize->setVisibility(llvm::GlobalValue::DefaultVisibility);
    newSize->setDSOLocal(false);

    if (oldSize)
    {
        oldSize->replaceAllUsesWith(newSize);
        oldSize->eraseFromParent();
    }

    std::cout << "Created global nanomites table with " << nanomitesEntries.size() << " entries in section .nanomites\n";
}

char LeetObfuscator::NanomitesMachineCodePass::ID = 0;

void LeetObfuscator::NanomitesMachineCodePass::InsertTrap(uint32_t id, llvm::MachineInstr& machineInstruction)
{
    llvm::MachineBasicBlock* machineBlock = machineInstruction.getParent();
    llvm::MachineFunction* machineFunction = machineBlock->getParent();
    const llvm::TargetInstrInfo* instructionInfo = machineFunction->getSubtarget().getInstrInfo();

    std::string asmText;
    llvm::raw_string_ostream os(asmText);
    os << 
        "\t.byte 0xCC\n" <<
        "\t.byte 0xE8\n" <<
        "\t.byte " << ((id >>  0) & 0xFF) << "\n" <<
        "\t.byte " << ((id >>  8) & 0xFF) << "\n" <<
        "\t.byte " << ((id >> 16) & 0xFF) << "\n" <<
        "\t.byte " << ((id >> 24) & 0xFF) << "\n" <<
        "\t.byte 0x0F\n";

    const char *AsmSym = machineFunction->createExternalSymbolName(os.str());

    llvm::MachineInstrBuilder builder = llvm::BuildMI(
        *machineBlock,
        machineInstruction.getIterator(),
        machineInstruction.getDebugLoc(),
        instructionInfo->get(llvm::TargetOpcode::INLINEASM)
    );
    builder.addExternalSymbol(AsmSym); // operand 0: the asm text
    builder.addImm(llvm::InlineAsm::Extra_HasSideEffects);  // operand 1: flags

    // This will set the clobbers, I'm not sure if it's necessary since this is pretty much the last pass that ever runs
    builder.copyImplicitOps(machineInstruction);
}

bool LeetObfuscator::NanomitesMachineCodePass::ParseLeetID(llvm::StringRef name, uint32_t& id)
{
    llvm::StringRef prefix = "__leet_forward_func_";
    if (!name.starts_with(prefix))
        return false;
    return !name.drop_front(prefix.size()).getAsInteger(10, id);
}

bool LeetObfuscator::NanomitesMachineCodePass::runOnMachineFunction(llvm::MachineFunction &machineFunction)
{
    bool changed = false;

    // Patch every call to __leet_forward_function_ID with a trap
    for (auto& machineBlock : machineFunction)
    {
        for (auto iterator = machineBlock.begin(); iterator != machineBlock.end(); )
        {
            llvm::MachineInstr& machineInstruction = *iterator;
            iterator++;
            if (!machineInstruction.isCall())
                continue;

            llvm::StringRef callee;
            if (machineInstruction.getOperand(0).isGlobal())
                callee = machineInstruction.getOperand(0).getGlobal()->getName();
            else if (machineInstruction.getOperand(0).isSymbol())
                callee = machineInstruction.getOperand(0).getSymbolName();
            else
                continue;

            uint32_t id;
            if (!ParseLeetID(callee, id))
                continue; // a normal call, leave it alone

            InsertTrap(id, machineInstruction);
            machineInstruction.eraseFromParent();
            changed = true;
        }
    }

    // Patch __leet_forward_function_ID itself and replace retn with a trap
    uint32_t id;
    if (ParseLeetID(machineFunction.getName(), id))
    {
        for (auto& machineBlock : machineFunction)
        {
            for (auto iterator = machineBlock.begin(); iterator != machineBlock.end(); )
            {
                llvm::MachineInstr &machineInstruction = *iterator;
                iterator++;

                if (!machineInstruction.isReturn())
                    continue;

                InsertTrap(0, machineInstruction);
                machineInstruction.eraseFromParent();
                changed = true;
            }
        }
    }

    return changed;
}