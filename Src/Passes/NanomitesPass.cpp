#include "NanomitesPass.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/IR/Verifier.h"

#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/ADT/Hashing.h"

#include "SettingsParser.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

llvm::PreservedAnalyses LeetObfuscator::NanomitesPass::run(llvm::Module& module, llvm::ModuleAnalysisManager&)
{
    std::cout << "Running NanomitesPass" << std::endl;
    m_Logger.LogModule(module, "Obfuscating module", 0);

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

uint32_t LeetObfuscator::NanomitesPass::GenerateUniqueNanomiteId(llvm::Module& module, RandomNumberGenerator& generator)
{
    static std::vector<uint32_t> allIds;

    uint32_t nanomiteId;
    do
    {
        nanomiteId = generator.DrawRange(1u, std::numeric_limits<uint32_t>::max());

        nanomiteId = (uint32_t)llvm::hash_combine(module.getModuleIdentifier(), nanomiteId);
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

    m_Logger.LogFunction(*function, "Processing function", 1);

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    std::vector<llvm::CallInst*> instructions;

    // Don't obfuscate exception stuff because it will break, also skip dispatcher barriers because there's too
    // many of them and it will be slow, they're also empty calls so what's the point
    if (function->getName().find("__leet_dispatcher_barrier") != std::string::npos ||
        function->getName().find("__leet_exception") != std::string::npos ||
        function->getName().find("__leet_forward") != std::string::npos ||
        function->getName().find("__leet_split_mix_64") != std::string::npos ||
        function->getName().find("__leet_nanomite_marker") != std::string::npos ||
        function->getName().find("sigemptyset") != std::string::npos ||
        function->getName().find("sigaction") != std::string::npos
    )
    {
        m_Logger.LogFunction(*function, "Skipping helper/runtime function", 2);
        return;
    }

    std::vector<llvm::Instruction*> markerCallsToRemove;
    
    for (auto& basicBlock : *function)
    {
        bool nextCallMarked = false;
        
        for (auto& inst : basicBlock)
        {
            llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(&inst);
            if (callInst)
            {
                llvm::Function* calledFunction = callInst->getCalledFunction();
                if (!calledFunction)
                    continue;

                // Check if this is our marker function
                if (calledFunction->getName() == "__leet_nanomite_marker")
                {
                    nextCallMarked = true;
                    m_Logger.LogInstruction(*callInst, "Found nanomite marker, next call will be obfuscated", 4);
                    // Mark for removal as it's just a placeholder
                    markerCallsToRemove.push_back(callInst);
                    continue;
                }

                if(!nextCallMarked && SettingsParser::ShouldSkipFunction(calledFunction, attributes))
                {
                    m_Logger.LogFunction(*calledFunction, "Skipping function due to pass settings", 1);
                    continue;
                }
                
                // Again, skip llvm and our own stuff
                if (calledFunction->getName().find("llvm.") == std::string::npos &&
                    calledFunction->getName().find("__leet_dispatcher_barrier") == std::string::npos &&
                    calledFunction->getName().find("__leet_exception") == std::string::npos &&
                    calledFunction->getName().find("__leet_trampoline") == std::string::npos &&
                    calledFunction->getName().find("__leet_forward") == std::string::npos &&
                    calledFunction->getName().find("__leet_split_mix_64") == std::string::npos &&
                    calledFunction->getName().find("sigemptyset") == std::string::npos &&
                    calledFunction->getName().find("sigaction") == std::string::npos
                )
                {
                    // Check if this call site is explicitly marked for nanomite obfuscation
                    bool isMarked = nextCallMarked;
                    if (isMarked)
                    {
                        m_Logger.LogInstruction(*callInst, "Call site explicitly marked for nanomite obfuscation", 4);
                    }
                    
                    // Reset the marker after checking
                    nextCallMarked = false;
                    
                    // If not marked, use probability based selection
                    if (!isMarked && generator->DrawRange(1u, 100u) > attributes.NanomitesProbability)
                        continue;

                    instructions.push_back(callInst);
                }
            }
        }
    }
    
    // Remove marker calls as they're just placeholders
    for (auto* markerCall : markerCallsToRemove)
    {
        markerCall->eraseFromParent();
    }

    if (instructions.empty())
    {
        m_Logger.LogFunction(*function, "No eligible call sites found", 2);
        return;
    }

    m_Logger.LogFunction(*function, "Found eligible call sites for nanomite insertion", 2);

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

        uint32_t callSiteId = GenerateUniqueNanomiteId(*module, *generator);

        m_Logger.LogFunction(*function, "Injecting nanomite wrapper", 3);
        m_Logger.LogInstruction(*callInst, "Rewriting call site", 4);
        m_Logger.Log(callInst->getCalledFunction() ? std::string("Target call: ") + callInst->getCalledFunction()->getName().str() : "Target call: <unknown>", 5);

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
        m_Logger.LogFunction(*function, "Registered nanomite entry for call site", 4);
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

    m_Logger.LogFunction(*function, "Finished transforming function", 2);
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

void LeetObfuscator::NanomitesPass::CreateGlobalNanomitesTable(llvm::Module& module, std::vector<llvm::Constant*>& nanomitesEntries)
{
    llvm::LLVMContext& context = module.getContext();
    llvm::PointerType* ptrTy = llvm::PointerType::get(context, 0);

    llvm::StructType* entryType = llvm::StructType::get(context, {llvm::Type::getInt32Ty(context), ptrTy});
    llvm::ArrayType* tableType = llvm::ArrayType::get(entryType, nanomitesEntries.size());

    llvm::StructType* chunkType = llvm::StructType::get(context, {ptrTy, llvm::Type::getInt32Ty(context), ptrTy}); // entries, count, next

    auto* table = new llvm::GlobalVariable(
        module,
        tableType,
        true,
        llvm::GlobalValue::InternalLinkage,
        llvm::ConstantArray::get(tableType, nanomitesEntries),
        "__nanomites_local"
    );

    auto* chunk = new llvm::GlobalVariable(
        module,
        chunkType,
        false,
        llvm::GlobalValue::InternalLinkage,
        llvm::ConstantStruct::get(chunkType, {
            table,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), (uint32_t)nanomitesEntries.size()),
            llvm::ConstantPointerNull::get(ptrTy)
        }),
        "__nanomites_chunk"
    );

    // Linked list
    // before = head -> A -> B -> C
    // After this insert = head -> D -> A -> B -> C
    // So this inserts the new chunk at the begining of the chain and makes it point to the previous element at that slot
    // the exception handler finds all of this through head pointer

    llvm::GlobalVariable* head = llvm::cast<llvm::GlobalVariable>(module.getOrInsertGlobal("__nanomite_chunk_head", ptrTy));

    llvm::FunctionType* constructorType = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);
    llvm::Function* constructorFunction = llvm::Function::Create(constructorType, llvm::GlobalValue::InternalLinkage, "__leet_register_nanomites", module);

    llvm::IRBuilder<> builder(llvm::BasicBlock::Create(context, "entry", constructorFunction));
    llvm::Value* oldHead = builder.CreateLoad(ptrTy, head);
    builder.CreateStore(oldHead, builder.CreateStructGEP(chunkType, chunk, 2));
    builder.CreateStore(chunk, head);
    builder.CreateRetVoid();

    llvm::appendToGlobalCtors(module, constructorFunction, 0);
    llvm::appendToCompilerUsed(module, {constructorFunction});

    m_Logger.LogModule(module, "Registered nanomites chunk with " + std::to_string(nanomitesEntries.size()) + " entries", 0);
}

char LeetObfuscator::NanomitesMachineCodePass::ID = 0;

void LeetObfuscator::NanomitesMachineCodePass::InsertTrap(uint32_t id, llvm::MachineInstr& machineInstruction)
{
    llvm::MachineBasicBlock* machineBlock = machineInstruction.getParent();
    llvm::MachineFunction* machineFunction = machineBlock->getParent();
    const llvm::TargetInstrInfo* instructionInfo = machineFunction->getSubtarget().getInstrInfo();

    // Insert some invalid opcodes as always
    // This will eat 4 ID bytes + the next instruction bytes after that as part of this instruction
    static const uint8_t primaries[] = {
        0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, // duplicated for better distribution
        0xC7, // MOV
        0x69, // IMUL
        0xF7 // TEST
    };

    static const uint8_t longModRMs[] = {
        0x84, 0x8C, 0x94, 0x9C, 0xA4, 0xAC, 0xB4, 0xBC
    };
    
    static thread_local std::mt19937 rng{std::random_device{}()}; // TODO generator
    std::uniform_int_distribution<size_t> primDist(0, std::size(primaries) - 1);
    std::uniform_int_distribution<size_t> modDist(0, std::size(longModRMs) - 1);
    std::uniform_int_distribution<uint8_t> byteDist(0, 255);

    uint8_t primary = primaries[primDist(rng)];
    uint8_t modrm = longModRMs[modDist(rng)];
    uint8_t sib = byteDist(rng);

    std::string asmText;
    llvm::raw_string_ostream os(asmText);
    os 
        << "\t.byte 0xCC\n"
        << "\t.byte " << (int32_t)primary << "\n"
        << "\t.byte " << (int32_t)modrm   << "\n"
        << "\t.byte " << (int32_t)sib     << "\n"
        << "\t.byte " << ((id >>  0) & 0xFF) << "\n"
        << "\t.byte " << ((id >>  8) & 0xFF) << "\n"
        << "\t.byte " << ((id >> 16) & 0xFF) << "\n"
        << "\t.byte " << ((id >> 24) & 0xFF) << "\n";
    
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
    static bool printed = false;
    if (!printed)
    {
        llvm::errs() << "Running NanomitesMachineFunctionPass\n";
        printed = true;
    }
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