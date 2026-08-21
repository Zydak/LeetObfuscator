#include "NanomitesPass.h"

#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Mangler.h"

#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/ADT/Hashing.h"

#include "SettingsParser.h"
#include "Src/Passes/RandomNumberGenerator.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>

#include "llvm/IR/MDBuilder.h"

llvm::PreservedAnalyses LeetObfuscator::NanomitesPass::run(llvm::Module& module, llvm::ModuleAnalysisManager&)
{
    std::cout << "Running NanomitesPass" << std::endl;
    m_Logger.LogModule(module, "Obfuscating module", 0);

    std::vector<llvm::Constant*> nanomitesEntries;

    ObfuscateFunctionPointerTables(module, nanomitesEntries);

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
    // 15 bits module tag + 17 bits local ID, supports >100K IDs per module (should be enough?)

    // Stable 15-bit tag from module id
    std::string uid = llvm::getUniqueModuleId(&module);
    if (uid.empty())
        uid = module.getModuleIdentifier();

    llvm::MD5 md5;
    md5.update(uid);
    llvm::MD5::MD5Result res;
    md5.final(res);
    const uint32_t moduleTag = (res[0] | (res[1] << 8) | (res[2] << 16)) & 0x7FFFu;

    static std::vector<uint32_t> ids;

    // 17 bit local ID
    uint32_t localId;
    do
    {
        localId = generator.DrawRange(1u, (1u << 17) - 1);
    }
    while (std::find(ids.begin(), ids.end(), localId) != ids.end());

    ids.push_back(localId);
    return (moduleTag << 17) | localId;
}

static std::string MakeIdTrailer(uint32_t nanomiteId, bool isTrampoline)
{
    std::shared_ptr<LeetObfuscator::RandomNumberGenerator> generator = LeetObfuscator::SettingsParser::GetGenerator(); // TODO

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

    uint8_t primary = primaries[generator->DrawRange((size_t)0, std::size(primaries) - 1)];
    uint8_t modrm = longModRMs[generator->DrawRange((size_t)0, std::size(longModRMs) - 1)];
    uint8_t sib = generator->DrawRange(0u, 255u);

    std::string asmText;
    llvm::raw_string_ostream os(asmText);
    os 
        << "\t.byte 0xCC\n"
        << "\t.byte " << (int32_t)primary << "\n"
        << "\t.byte " << (int32_t)modrm   << "\n"
        << "\t.byte " << (int32_t)sib     << "\n"
        << "\t.byte " << ((nanomiteId >>  0) & 0xFF) << "\n"
        << "\t.byte " << ((nanomiteId >>  8) & 0xFF) << "\n"
        << "\t.byte " << ((nanomiteId >> 16) & 0xFF) << "\n"
        << "\t.byte " << ((nanomiteId >> 24) & 0xFF) << "\n"
        << "\t.byte " << (uint32_t(isTrampoline) & 0xFF);

    return asmText;
}

llvm::Constant* LeetObfuscator::NanomitesPass::MakeEntry(uint32_t id, llvm::Constant* addr, llvm::LLVMContext& context)
{
    llvm::StructType* entryType = llvm::StructType::get(
        context,
        {llvm::Type::getInt32Ty(context), llvm::PointerType::get(context, 0)}
    );

    return llvm::ConstantStruct::get(
        entryType,
        {llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), id), addr}
    );
}

void LeetObfuscator::NanomitesPass::ObfuscateFunction(llvm::Function *function, std::vector<llvm::Constant*>& nanomitesEntries)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(
        *function,
        SettingsParser::PassType::NanomitesPass,
        m_Arguments
    );

    // Even if the function shouldn't be parsed, there can still be explicit markers inside, so don't exit immediately
    bool shouldParseAllCalls = !SettingsParser::ShouldSkipFunction(function, attributes);

    m_Logger.LogFunction(*function, "Processing function", 1);

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator(attributes);

    std::vector<llvm::CallInst*> callInstructions;

    // Don't obfuscate exception stuff because it will break, also skip dispatcher barriers because there's too
    // many of them and it will be slow, they're also empty calls so what's the point
    if (function->getName().find("__leet_dispatcher_barrier") != std::string::npos ||
        function->getName().find("__leet_exception") != std::string::npos ||
        function->getName().find("__leet_forward") != std::string::npos ||
        function->getName().find("__leet_split_mix_64") != std::string::npos ||
        function->getName().find("__leet_nanomite_marker") != std::string::npos ||
        function->getName().find("sigemptyset") != std::string::npos ||
        function->getName().find("sigaction") != std::string::npos ||
        function->getName().find("AddVectoredExceptionHandler") != std::string::npos ||
        function->getName().find("__leet_is_debugger_present_tracer_pid") != std::string::npos ||
        function->getName().find("__leet_is_debugger_present_blacklist") != std::string::npos ||
        function->getName().find("RemoveVectoredExceptionHandler") != std::string::npos
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

                if(!nextCallMarked && !shouldParseAllCalls)
                {
                    m_Logger.LogFunction(*calledFunction, "Skipping call due to pass settings", 1);
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
                    if (!isMarked && generator->DrawRange(1u, 100u) > attributes.nanomitesProbability)
                        continue;

                    callInstructions.push_back(callInst);
                }
            }
        }
    }
    
    // Remove marker calls as they're just placeholders
    for (auto* markerCall : markerCallsToRemove)
    {
        markerCall->eraseFromParent();
    }

    if (callInstructions.empty())
    {
        m_Logger.LogFunction(*function, "No eligible call sites found", 2);
        return;
    }

    m_Logger.LogFunction(*function, "Found eligible call sites for nanomite insertion", 2);

    function->addFnAttr(llvm::Attribute::NoInline);

    llvm::LLVMContext& context = function->getContext();
    llvm::Module* module = function->getParent();

    for (auto* callInst : callInstructions)
    {
        llvm::Function* realFunc = callInst->getCalledFunction();
        realFunc->addFnAttr(llvm::Attribute::NoInline);

        uint32_t callSiteId = GenerateUniqueNanomiteId(*module, *generator);

        m_Logger.LogFunction(*function, "Injecting nanomite wrapper", 3);
        m_Logger.LogInstruction(*callInst, "Rewriting call site", 4);
        m_Logger.Log(callInst->getCalledFunction() ? std::string("Target call: ") + callInst->getCalledFunction()->getName().str() : "Target call: <unknown>", 5);

        if (generator->DrawRange(1u, 100u) <= attributes.nanomitesTrampolineProbability)
        {
            // Trampoline
            llvm::Function* trampoline = CreateTrampoline(*module, realFunc, callSiteId);

            llvm::IRBuilder<> builder(callInst);
            llvm::InlineAsm *IA = llvm::InlineAsm::get(
                llvm::FunctionType::get(llvm::Type::getVoidTy(context), false), 
                "/*__nanomite_call_marker_" + std::to_string(callSiteId) + "*/",
                "",
                true
            );
            builder.CreateCall(IA);

            nanomitesEntries.push_back(MakeEntry(callSiteId, trampoline, context));
            callInst->setTailCallKind(llvm::CallInst::TCK_NoTail);
            callInst->addFnAttr(llvm::Attribute::NoInline);
            realFunc->addFnAttr(llvm::Attribute::OptimizeNone);
        }
        else
        {
            // Forward func
            llvm::Function* forwardFunc = CreateForwardFunction(*module, realFunc, callSiteId);
            callInst->setCalledFunction(forwardFunc);
            nanomitesEntries.push_back(MakeEntry(callSiteId, realFunc, context));
        }

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

void LeetObfuscator::NanomitesPass::ObfuscateFunctionPointerTables(llvm::Module& module, std::vector<llvm::Constant*>& nanomitesEntries)
{
    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator();
    llvm::LLVMContext& context = module.getContext();

    for (auto& global : module.globals())
    {
        if (!global.hasInitializer())
            continue;

        auto* constArr = llvm::dyn_cast<llvm::ConstantArray>(global.getInitializer());
        if (!constArr)
            continue; // TODO?

        bool changed = false;
        std::vector<llvm::Constant*> newElems;
        newElems.reserve(constArr->getNumOperands());

        for (llvm::Use& op : constArr->operands())
        {
            llvm::Constant* elem = llvm::cast<llvm::Constant>(op.get());
            llvm::Function* realFunc = llvm::dyn_cast<llvm::Function>(elem->stripPointerCasts());

            if (!realFunc)
            {
                newElems.push_back(elem);
                continue;
            }

            uint32_t id = GenerateUniqueNanomiteId(module, *generator);
            llvm::Function* forwarder = CreateForwardFunction(module, realFunc, id);
            realFunc->addFnAttr(llvm::Attribute::NoInline);

            newElems.push_back(forwarder);
            nanomitesEntries.push_back(MakeEntry(id, realFunc, context));
            changed = true;
        }

        if (changed)
            global.setInitializer(llvm::ConstantArray::get(constArr->getType(), newElems));
    }
}

// Fuck win32
static std::string GetMangledSymbolName(llvm::Function* function)
{
    llvm::SmallString<128> buffer;
    llvm::raw_svector_ostream os(buffer);
    llvm::Mangler mangler;
    mangler.getNameWithPrefix(os, function, false);
    return std::string(buffer.str());
}

llvm::Function* LeetObfuscator::NanomitesPass::CreateTrampoline(llvm::Module& module, llvm::Function* realFunc, uint32_t callSiteId)
{
    llvm::LLVMContext& context = module.getContext();

    llvm::FunctionType* trampolineTy = llvm::FunctionType::get(llvm::Type::getVoidTy(context), false);

    std::string trampolineName = "__leet_trampoline_" + std::to_string(callSiteId);
    
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
    std::string target = GetMangledSymbolName(realFunc);
    for (size_t pos = 0; (pos = target.find('$', pos)) != std::string::npos; pos += 2)
        target.replace(pos, 1, "$$");

    std::string asmText = "call " + target + "\n\t" + MakeIdTrailer(0, true); // Empty ID, exception handler will pop the address from the stack

    llvm::InlineAsm* trampolineAsm = llvm::InlineAsm::get(
        llvm::FunctionType::get(builder.getVoidTy(), false),
        asmText,
        "",
        true
    );

    builder.CreateCall(trampolineAsm);
    builder.CreateUnreachable();

    // Trampoline block is technically orphaned, so make sure it's not opted out
    llvm::appendToCompilerUsed(module, {trampoline});
    llvm::appendToCompilerUsed(module, {realFunc});

    return trampoline;
}

llvm::Function* LeetObfuscator::NanomitesPass::CreateForwardFunction(llvm::Module& module, llvm::Function* realFunc, uint32_t id)
{
    llvm::LLVMContext& ctx = module.getContext();

    std::string name = "__leet_forwarding_function_" + std::to_string(id);

    llvm::FunctionType* fnType = realFunc->getFunctionType();
    llvm::Function* forwardFunc = llvm::Function::Create(
        fnType,
        realFunc->getLinkage(),
        name,
        module
    );

    forwardFunc->setCallingConv(realFunc->getCallingConv());
    forwardFunc->setAttributes(realFunc->getAttributes());

    forwardFunc->addFnAttr(llvm::Attribute::NoInline);

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", forwardFunc);
    llvm::IRBuilder<> builder(entry);

    std::vector<llvm::Value*> args;
    args.reserve(forwardFunc->arg_size());
    for (llvm::Argument& arg : forwardFunc->args())
        args.push_back(&arg);

    llvm::CallInst* call = builder.CreateCall(realFunc, args);
    call->setCallingConv(realFunc->getCallingConv());
    call->setAttributes(realFunc->getAttributes());
    call->setTailCallKind(llvm::CallInst::TCK_MustTail);

    if (fnType->getReturnType()->isVoidTy())
        builder.CreateRetVoid();
    else
        builder.CreateRet(call);

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

void LeetObfuscator::NanomitesMachineCodePass::InsertTrap(uint32_t id, llvm::MachineInstr& machineInstruction, bool isTrampoline)
{
    llvm::MachineBasicBlock* machineBlock = machineInstruction.getParent();
    llvm::MachineFunction* machineFunction = machineBlock->getParent();
    const llvm::TargetInstrInfo* instructionInfo = machineFunction->getSubtarget().getInstrInfo();

    std::string asmText = MakeIdTrailer(id, isTrampoline);
    
    const char *AsmSym = machineFunction->createExternalSymbolName(asmText);

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
    llvm::StringRef prefix = "__leet_forwarding_function_";
    if (!name.starts_with(prefix))
        return false;
    return !name.drop_front(prefix.size()).getAsInteger(10, id);
}

bool LeetObfuscator::NanomitesMachineCodePass::runOnMachineFunction(llvm::MachineFunction& machineFunction)
{
    static bool printed = false;
    if (!printed)
    {
        llvm::errs() << "Running NanomitesMachineFunctionPass\n";
        printed = true;
    }

    std::shared_ptr<RandomNumberGenerator> generator = SettingsParser::GetGenerator();

    bool changed = false;

    bool isForwardCall = false;
    if (machineFunction.getName().starts_with("__leet_forwarding_function_"))
        isForwardCall = true;

    uint32_t nanomiteID;
    if(isForwardCall && !ParseLeetID(machineFunction.getName(), nanomiteID))
    {
        llvm::errs() << "Failed to parse id????\n";
        return changed;
    }

    bool nextCallIsNanomite = false;
    for (auto& machineBlock : machineFunction)
    {
        for (auto iterator = machineBlock.begin(); iterator != machineBlock.end();)
        {
            llvm::MachineInstr& machineInstruction = *iterator;
            iterator++;

            if (isForwardCall)
            {
                // Tails show up as both call and return
                if (!machineInstruction.isCall() && !machineInstruction.isReturn())
                    continue;
    
                InsertTrap(nanomiteID, machineInstruction, false);
                machineInstruction.eraseFromParent();
                changed = true;
            }
            else
            {
                // Look for marker
                if (machineInstruction.getOpcode() == llvm::TargetOpcode::INLINEASM)
                {
                    const char *AsmStr = machineInstruction.getOperand(0).getSymbolName();
                    llvm::StringRef Name(AsmStr);
                    if (Name.consume_front("/*__nanomite_call_marker_"))
                    {
                        uint32_t id = 0;
                        if (!Name.consumeInteger(10, id))
                        {
                            if (nextCallIsNanomite)
                            {
                                llvm::errs() << "This should never happen! Something reordered the markers really badly!\n";
                                exit(1);
                            }
                            nextCallIsNanomite = true;
                            nanomiteID = id;
                            machineInstruction.eraseFromParent();
                            continue;
                        }
                    }
                }

                if (!machineInstruction.isCall() || !nextCallIsNanomite)
                    continue;

                if (machineInstruction.isReturn())
                {
                    std::cout << "TAIL" << std::endl;
                    exit(1);
                }

                std::string trampolineName = "__leet_trampoline_" + std::to_string(nanomiteID);
                llvm::Function* trampolineFunction = machineFunction.getFunction().getParent()->getFunction(trampolineName);

                if (!trampolineFunction)
                {
                    std::cout << "Counldn't find trampoline!" << std::endl;
                    nextCallIsNanomite = false;
                    continue;
                }

                nextCallIsNanomite = false;

                InsertTrap(nanomiteID, machineInstruction, true);
                machineInstruction.eraseFromParent();
                changed = true;
            }
        }
    }

    return changed;
}