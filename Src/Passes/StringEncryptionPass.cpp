#include "StringEncryptionPass.h"

#include "llvm/IR/ReplaceConstant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Module.h"
#include "SettingsParser.h"
#include "RandomNumberGenerator.h"

llvm::PreservedAnalyses LeetObfuscator::StringEncryptionPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running StringEncryptionPass\n";
    m_Logger.LogModule(module, "Starting pass", 0);

    srand(time(nullptr));
    std::vector<StringGlobalInfo> stringGlobals;
    for (auto& global : module.globals())
    {
        if (IsEncryptableStringGlobal(&global))
        {
            SettingsParser::GlobalAttributes globalAttributes = SettingsParser::ParseGlobalAttributes();
            std::shared_ptr<RandomNumberGenerator> generator = RandomNumberGenerator::GetGlobalRandomNumberGenerator();

            if (generator->DrawRange(0u, 100u) > globalAttributes.stringEncryptionProbability)
                continue;

            uint32_t key = generator->DrawRange(0u, std::numeric_limits<uint32_t>::max());
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
        EncryptGlobalAndPatchAllUses(stringGlobal);
    }

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

void LeetObfuscator::StringEncryptionPass::EncryptGlobalAndPatchAllUses(StringGlobalInfo &stringInfo)
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
    llvm::Function* decryptFunction = GetDecryptFunction(*globalVar->getParent(), key);
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

llvm::Function *LeetObfuscator::StringEncryptionPass::GetDecryptFunction(llvm::Module &module, uint32_t key)
{
    llvm::LLVMContext &context = module.getContext();
    llvm::IRBuilder<> builder(context);

    // Types
    llvm::Type *typeInt8 = builder.getInt8Ty();
    llvm::Type *typeInt32 = builder.getInt32Ty();
    llvm::Type *typePtr = builder.getPtrTy();

    // Function type: ptr (ptr, ptr, i32)
    llvm::FunctionType *functionType = llvm::FunctionType::get(typePtr, {typePtr, typePtr, typeInt32}, false);

    llvm::Function *function = llvm::Function::Create(
        functionType,
        llvm::GlobalValue::ExternalLinkage,
        "__leet_decrypt_string",
        &module
    );
    function->setCallingConv(llvm::CallingConv::C);

    // Name the arguments
    auto argumentIterator = function->arg_begin();
    llvm::Value *encrypted = argumentIterator++;
    encrypted->setName("enc");
    llvm::Value *output = argumentIterator++;
    output->setName("out");
    llvm::Value *length = argumentIterator++;
    length->setName("len");

    // Basic blocks
    llvm::BasicBlock *entryBlock = llvm::BasicBlock::Create(context, "entry", function);
    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(context, "loop", function);
    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(context, "body", function);
    llvm::BasicBlock *exitBlock = llvm::BasicBlock::Create(context, "exit", function);

    // entry
    builder.SetInsertPoint(entryBlock);
    builder.CreateBr(loopBlock);

    // loop header (phi + condition)
    builder.SetInsertPoint(loopBlock);
    llvm::PHINode *index = builder.CreatePHI(typeInt32, 2, "i");
    index->addIncoming(builder.getInt32(0), entryBlock);

    llvm::Value *comparison = builder.CreateICmpULT(index, length, "cmp");
    builder.CreateCondBr(comparison, bodyBlock, exitBlock);

    // loop body
    builder.SetInsertPoint(bodyBlock);

    // keyByte = (key >> (8 * (i % 4))) & 0xFF
    llvm::Value *modulo = builder.CreateURem(index, builder.getInt32(4), "mod");
    llvm::Value *shiftAmount = builder.CreateMul(modulo, builder.getInt32(8), "shift");
    llvm::Value *keyConstant = builder.getInt32(key);
    llvm::Value *shifted = builder.CreateLShr(keyConstant, shiftAmount, "shr");
    llvm::Value *keyByte32 = builder.CreateAnd(shifted, builder.getInt32(0xFF), "keybyte32");
    llvm::Value *keyByte = builder.CreateTrunc(keyByte32, typeInt8, "keybyte");

    // enc[i]
    llvm::Value *encryptedGep = builder.CreateGEP(typeInt8, encrypted, index, "enc.gep");
    llvm::Value *encryptedByte = builder.CreateLoad(typeInt8, encryptedGep, "enc.byte");

    // out[i] = enc[i] ^ keyByte
    llvm::Value *xored = builder.CreateXor(encryptedByte, keyByte, "xored");
    llvm::Value *outputGep = builder.CreateGEP(typeInt8, output, index, "out.gep");
    builder.CreateStore(xored, outputGep);

    // i = i + 1
    llvm::Value *nextIndex = builder.CreateAdd(index, builder.getInt32(1), "next");
    index->addIncoming(nextIndex, bodyBlock);
    builder.CreateBr(loopBlock);

    // exit
    builder.SetInsertPoint(exitBlock);
    builder.CreateRet(output);

    return function;
}