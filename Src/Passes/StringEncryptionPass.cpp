#include "StringEncryptionPass.h"

#include "llvm/IR/Analysis.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/ReplaceConstant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Module.h"
#include "SettingsParser.h"
#include "RandomNumberGenerator.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/IR/Intrinsics.h"

#include "../../build/LeetObfuscator/LeetRuntimeString.x64.inc" // template bitcode
#include "../../build/LeetObfuscator/LeetRuntimeString.x86.inc"

llvm::PreservedAnalyses LeetObfuscator::StringEncryptionPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    std::shared_ptr<RandomNumberGenerator> generator = RandomNumberGenerator::GetGlobalRandomNumberGenerator();

    if (!m_StartCallback)
    {
        llvm::errs() << "Running StringEncryptionPass\n";

        std::vector<llvm::Function*> functionsToInline;
        for (auto& function : module)
        {
            if (function.getName().starts_with("__leet_decrypt_string"))
            {
                functionsToInline.push_back(&function);
            }
        }

        uint32_t inlineProbability = 50;
        const auto* probArg = SettingsParser::FindArgument(m_Arguments, "inlineProbability");
        if (probArg && !probArg->empty())
            inlineProbability = std::stoul(probArg->front());

        for (auto& function : functionsToInline)
        {
            if (generator->DrawRange(1u, 100u) > inlineProbability)
                continue;
            
            std::vector<llvm::CallInst*> callSites;
            for (llvm::User* user : function->users())
            {
                if (auto* call = llvm::dyn_cast<llvm::CallInst>(user))
                    callSites.push_back(call);
            }

            for (llvm::CallInst* call : callSites)
            {
                llvm::InlineFunctionInfo ifi;
                llvm::InlineResult res = llvm::InlineFunction(*call, ifi);
                if (!res.isSuccess())
                {
                    llvm::errs() << "WARNING: failed to inline string decrypt function '" << function->getName() << "': " << res.getFailureReason() << "\n";
                }
            }

            if (function->use_empty())
                function->eraseFromParent();
            else
                llvm::errs() << "WARNING: function '" << function->getName() << "' still has uses after inlining, leaving it in place\n";

        }
        return llvm::PreservedAnalyses::none();
    }

    llvm::errs() << "Running StringEncryptionPass (Pre process)\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    EmittedTemplate templates = GetTemplateFunctions(module);

    if (!templates.decryptFunction || !templates.getKeyFunction || !templates.module)
    {
        llvm::errs() << "ERROR: Failed to import template functions for string obfuscation. Skipping the pass.\n";
        return llvm::PreservedAnalyses::all();
    }

    uint32_t stringEncryptionProbability = 100;
    const auto* probArg = SettingsParser::FindArgument(m_Arguments, "probability");
    if (probArg && !probArg->empty())
        stringEncryptionProbability = std::stoul(probArg->front());

    std::vector<StringGlobalInfo> stringGlobals;
    for (auto& global : module.globals())
    {
        if (IsEncryptableStringGlobal(&global))
        {
            if (generator->DrawRange(1u, 100u) > stringEncryptionProbability)
                continue;

            uint32_t key;
            
            while(true)
            {
                key = generator->DrawRange(1u, std::numeric_limits<uint32_t>::max());

                // Check for collision
                bool collision = false;
                for (const auto& existingGlobal : stringGlobals)
                {
                    if (existingGlobal.key == key)
                    {
                        collision = true;
                        break;
                    }
                }
                if (collision)
                    continue;
                else
                    break;
            }
            
            m_Logger.LogValue(global, "Encrypting string global", 1);
            stringGlobals.push_back({&global, key});
        }
    }

    if (stringGlobals.empty())
    {
        m_Logger.Log("No encryptable string globals found", 1);
        return llvm::PreservedAnalyses::all();
    }

    for(auto& stringGlobal : stringGlobals)
    {
        m_Logger.LogValue(*stringGlobal.globalVar, "Encrypting and patching uses", 2);
        EncryptGlobalAndPatchAllUses(stringGlobal, templates);
    }

    templates.decryptFunction->eraseFromParent();
    templates.getKeyFunction->eraseFromParent();

    return llvm::PreservedAnalyses::none();
}

bool LeetObfuscator::StringEncryptionPass::IsEncryptableStringGlobal(llvm::GlobalVariable *global)
{
    if (!global->hasInitializer() || !global->isConstant())
        return false;

    if (global->getName().starts_with("llvm.") || global->getName().starts_with("leet."))
        return false;

    llvm::ConstantDataArray* dataArray = llvm::dyn_cast<llvm::ConstantDataArray>(global->getInitializer());
    if (!dataArray || !dataArray->isString() || dataArray->getNumElements() == 0)
        return false;

    return true;
}

void LeetObfuscator::StringEncryptionPass::EncryptGlobalAndPatchAllUses(StringGlobalInfo &stringInfo, const EmittedTemplate& templates)
{
    llvm::GlobalVariable* globalVar = stringInfo.globalVar;
    uint32_t key = stringInfo.key;

    // Try to convert all uses into patchable instructions
    llvm::convertUsersOfConstantsToInstructions({globalVar});
    std::vector<llvm::Use*> usesToPatch;
    for(auto& use : globalVar->uses())
    {
        if (!llvm::isa<llvm::Instruction>(use.getUser()))
        {
            // Some uses were still not patched there's nothing we can do for this global
            llvm::errs() << "StringPass: skipping global '" << globalVar->getName()
                         << "' - has a non-instruction use ("
                         << *use.getUser() << ") that can't be patched\n";
            return;
        }

        // Check the function attirbutes of the user instruction's function, if it has "leet.skip" then we can't patch this use
        llvm::Instruction* userInst = llvm::cast<llvm::Instruction>(use.getUser());
        llvm::Function* userFunction = userInst->getFunction();
        SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(
            *userFunction, SettingsParser::PassType::StringEncryptionPass, m_Arguments
        );
        if (SettingsParser::ShouldSkipFunction(userFunction, attributes))
        {
            llvm::errs() << "StringPass: skipping global '" << globalVar->getName()
                         << "' - has a use in function '" << userFunction->getName()
                         << "' that has 'leet.skip' attribute, can't patch this use\n";
            return;
        }
        usesToPatch.push_back(&use);
    }

    llvm::ConstantDataArray* dataArray = llvm::cast<llvm::ConstantDataArray>(globalVar->getInitializer());
    const size_t bytesCount = dataArray->getNumElements();
    std::vector<uint8_t> encryptedBytes;
    encryptedBytes.reserve(bytesCount);
    for(size_t i = 0; i < bytesCount; i++)
    {
        uint8_t key8 = uint8_t((key >> (8 * (i % 4))) & 0xFF);
        uint8_t byte = dataArray->getElementAsInteger(i);
        encryptedBytes.push_back(byte ^ key8);
    }
    llvm::Constant* newInitialzer = llvm::ConstantDataArray::get(globalVar->getContext(), encryptedBytes);
    globalVar->setInitializer(newInitialzer);

    // Replace all uses with decrypt function
    llvm::Function* decryptFunction = GetDecryptFunction(*globalVar->getParent(), key, templates);
    for (llvm::Use* use : usesToPatch)
    {
        llvm::Instruction* instruction = llvm::cast<llvm::Instruction>(use->getUser());
        llvm::Function* parentFunction = instruction->getFunction();

        llvm::IRBuilder<> builder(&parentFunction->getEntryBlock(), parentFunction->getEntryBlock().getFirstInsertionPt());
        llvm::AllocaInst* stackBuffer = builder.CreateAlloca(builder.getInt8Ty(), builder.getInt32(bytesCount));

        // Compiler will sometimes aligns strings into 16 bytes to lower memcpy into an xmm move, so this stack buffer also has to be aligned
        stackBuffer->setAlignment(globalVar->getAlign().valueOrOne());

        // If this use feeds a PHI node, the decrypt call must be inserted at the
        // end of the corresponding predecessor block (before its terminator),
        // not before the PHI itself. Fuck phi nodes.
        llvm::Instruction* insertBefore = instruction;
        if (auto* phi = llvm::dyn_cast<llvm::PHINode>(instruction))
        {
            llvm::BasicBlock* incomingBlock = phi->getIncomingBlock(*use);
            insertBefore = incomingBlock->getTerminator();
        }

        llvm::IRBuilder<> callBuilder(insertBefore);
        llvm::Value* decryptCall = callBuilder.CreateCall(decryptFunction, { globalVar, stackBuffer, builder.getInt32(bytesCount) });
        use->set(decryptCall);
    }
}

llvm::Function *LeetObfuscator::StringEncryptionPass::GetDecryptFunction(llvm::Module &module, uint32_t key, const EmittedTemplate& templates)
{
    llvm::Function* templateFunction = templates.decryptFunction;

    llvm::LLVMContext& context = module.getContext();
    llvm::Type* bytePtrType = llvm::PointerType::getUnqual(context);
    llvm::Type* i32Type = llvm::Type::getInt32Ty(context);

    // basically `ptr __leet_decrypt_string(ptr encryptedStr, ptr outStr, i64 lenStr)`
    llvm::FunctionType* functionType = llvm::FunctionType::get(bytePtrType, {bytePtrType, bytePtrType, i32Type}, false);

    llvm::ValueToValueMapTy vmap;
    llvm::SmallVector<llvm::ReturnInst*, 8> returns;
    llvm::Function* clone = llvm::Function::Create(functionType, llvm::GlobalValue::InternalLinkage, templateFunction->getName() + std::to_string(key), module);
    // For whatever reason you have to manually copy arguments...
    auto destArgIt = clone->arg_begin();
    for(auto& oldArg : templateFunction->args())
    {
        destArgIt->setName(oldArg.getName());
        vmap[&oldArg] = destArgIt;
        destArgIt++;
    }
    llvm::CloneFunctionInto(clone, templateFunction, vmap, llvm::CloneFunctionChangeType::DifferentModule, returns);

    if (clone->isDeclaration())
    {
        llvm::errs() << "ERROR: Clone of the template is only a declaration!\n";
        return nullptr;
    }

    // Force every CallInst that still points into the template module
    // to use the equivalent declaration in the target module.
    // This is required for llvm.assume (and any other intrinsics / external funcs).
    for (auto& basicBlock : *clone)
    {
        for (auto& instruction : basicBlock)
        {
            if (auto* CB = llvm::dyn_cast<llvm::CallBase>(&instruction))
            {
                if (llvm::Function* Callee = CB->getCalledFunction())
                {
                    if (Callee->getParent() != &module)
                    {
                        if (Callee->isIntrinsic())
                        {
                            llvm::Function* NewCallee = llvm::Intrinsic::getOrInsertDeclaration(&module, Callee->getIntrinsicID());
                            CB->setCalledFunction(NewCallee);
                        }
                        else
                        {
                            llvm::Function* NewCallee = llvm::cast<llvm::Function>(
                                module.getOrInsertFunction(Callee->getName(), Callee->getFunctionType()).getCallee());
                            CB->setCalledFunction(NewCallee);
                        }
                    }
                }
            }
        }
    }

    // Find all getKeys inside the clone and replace them with the actual key
    std::vector<llvm::CallInst*> callsToReplace;
    for (auto& basicBlock : *clone)
    {
        for (auto& instruction : basicBlock)
        {
            if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(&instruction))
            {
                llvm::Function* calledFunction = callInst->getCalledFunction();
                if (calledFunction && calledFunction->getName() == templates.getKeyFunction->getName())
                {
                    callsToReplace.push_back(callInst);
                }
            }
        }
    }
    for (auto* callInst : callsToReplace)
    {
        llvm::IRBuilder<> builder(callInst);
        callInst->replaceAllUsesWith(builder.getInt32(key));
        callInst->eraseFromParent();
    }

    clone->addFnAttr(llvm::Attribute::NoInline);
    clone->addFnAttr(llvm::Attribute::OptimizeNone); // It's already optimized bitcode, and the optimizer will try to do some dumb shit without this 
    return clone;
}

LeetObfuscator::StringEncryptionPass::EmittedTemplate LeetObfuscator::StringEncryptionPass::GetTemplateFunctions(llvm::Module &module)
{
    EmittedTemplate result;
    llvm::LLVMContext& context = module.getContext();

    llvm::StringRef data;
    bool is64 = module.getDataLayout().getPointerSizeInBits() == 64;
    if (is64)
    {
        data = llvm::StringRef(reinterpret_cast<const char*>(LeetRuntimeString_x64_bc), LeetRuntimeString_x64_bc_len);
    }
    else
    {
        data = llvm::StringRef(reinterpret_cast<const char*>(LeetRuntimeString_x86_bc), LeetRuntimeString_x86_bc_len);
    }

    // Create a dummy module with template functions
    std::unique_ptr<llvm::MemoryBuffer> buffer = llvm::MemoryBuffer::getMemBuffer(data, "leet_obf_runtime", false);
    auto modOrErr = llvm::parseBitcodeFile(buffer->getMemBufferRef(), context);
    if (!modOrErr)
    {
        llvm::errs() << modOrErr.takeError() << "\n";
        return result;
    }
    result.module = std::move(modOrErr.get());

    result.decryptFunction = result.module->getFunction("__leet_decrypt_string");
    result.getKeyFunction  = result.module->getFunction("__leet_get_key");

    return result;
}

