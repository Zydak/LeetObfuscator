#include "StringEncryptionPass.h"

#include "llvm/IR/ReplaceConstant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Module.h"

#include "leet_string_obf_runtime_bc.inc" // template bitcode

llvm::PreservedAnalyses LeetObfuscator::StringEncryptionPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running StringEncryptionPass\n";

    EmittedTemplate templates = GetTemplateFunctions(module);

    if (!templates.decryptFunction || !templates.getKeyFunction || !templates.module)
    {
        llvm::errs() << "ERROR: Failed to import template functions for string obfuscation. Skipping the pass.\n";
        return llvm::PreservedAnalyses::all();
    }

    srand(time(nullptr));
    std::vector<StringGlobalInfo> stringGlobals;
    for (auto& global : module.globals())
    {
        if (IsEncryptableStringGlobal(&global))
        {
            uint32_t key = rand();
            stringGlobals.push_back({&global, key});
        }
    }

    if (stringGlobals.empty())
        return llvm::PreservedAnalyses::all();

    for(auto& stringGlobal : stringGlobals)
    {
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
    llvm::CloneFunctionInto(clone, templateFunction, vmap, llvm::CloneFunctionChangeType::LocalChangesOnly, returns);

    if (clone->isDeclaration())
    {
        llvm::errs() << "ERROR: Clone of the template is only a declaration!\n";
        return nullptr;
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

    return clone;
}

LeetObfuscator::StringEncryptionPass::EmittedTemplate LeetObfuscator::StringEncryptionPass::GetTemplateFunctions(llvm::Module &module)
{
    EmittedTemplate result;
    llvm::LLVMContext& context = module.getContext();

    // Create a dummy module with template functions
    llvm::StringRef data(reinterpret_cast<const char*>(leet_string_obf_runtime_bc), leet_string_obf_runtime_bc_len);
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
