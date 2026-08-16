#include "VariableSplittingPass.h"
#include "Src/Passes/SettingsParser.h"
#include "llvm/IR/Analysis.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Verifier.h"

extern bool TryGetAllocaTotalSize(llvm::AllocaInst* allocaInst, const llvm::DataLayout& dataLayout, uint64_t& outSize);

llvm::PreservedAnalyses LeetObfuscator::VariableSplittingPass::run(llvm::Module& module, llvm::ModuleAnalysisManager&)
{
    llvm::errs() << "Running VariableSplittingPass\n";

    for (auto& function : module)
    {
        ObfuscateFunction(&function);
    }

    return llvm::PreservedAnalyses::none();
}

void LeetObfuscator::VariableSplittingPass::ObfuscateFunction(llvm::Function* function)
{
    SettingsParser::FunctionAttributes attributes = SettingsParser::ParseFunctionAttributes(*function, SettingsParser::PassType::VariableSplittingPass, m_Arguments);
    if (SettingsParser::ShouldSkipFunction(function, attributes))
        return;

    llvm::Module* module = function->getParent();
    std::vector<llvm::AllocaInst*> allocaInstructions;

    for (auto& block : *function)
    {
        for (auto& instruction : block)
        {
            llvm::AllocaInst* allocaInstruction = llvm::dyn_cast<llvm::AllocaInst>(&instruction);
            if (allocaInstruction)
                allocaInstructions.push_back(allocaInstruction);
        }
    }

    for (auto& allocaInstruction : allocaInstructions)
    {
        llvm::Type* allocatedType = allocaInstruction->getAllocatedType();
        llvm::IntegerType* intType = llvm::dyn_cast<llvm::IntegerType>(allocatedType);
        if (!intType || intType->getBitWidth() % 8 != 0)
            continue;

        uint64_t allocaSize = 0;
        if (!TryGetAllocaTotalSize(allocaInstruction, module->getDataLayout(), allocaSize))
        {
            llvm::errs() << "Can't get alloca size\n";
            continue;
        }
        if (allocaSize < 2 || allocaSize > 8)
        {
            llvm::errs() << "Alloca to big or small " << allocaSize << "\n";
            continue;
        }
        
        if (!IsAllocaSplittable(allocaInstruction))
        {
            llvm::errs() << "alloca is not splittable\n";
            continue;
        }

        SplitAlloca(allocaInstruction, intType, allocaSize);
    }

    // Verify the function at the end
    if (llvm::verifyFunction(*function, &llvm::errs()))
    {
        llvm::errs() << "[ERROR] VariableSplittingPass: Function '" << function->getName() << "' verification failed after transformation!\n";

        // Dump the function IR and terminate
        
        std::error_code ec;
        llvm::raw_fd_ostream logFile("error_log.txt", ec);
        if (!ec)
        {
            function->print(logFile);
            logFile.close();
            llvm::errs() << "VariableSplittingPass: Function IR dumped to error_log.txt\n";
        }
        else
        {
            llvm::errs() << "VariableSplittingPass: Failed to open error_log.txt for writing: " << ec.message() << "\n";
        }
        exit(1);
    }
}

bool LeetObfuscator::VariableSplittingPass::IsAllocaSplittable(llvm::AllocaInst* allocaInstruciton)
{
    for (llvm::User* user : allocaInstruciton->users())
    {
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(user))
        {
            if (load->isVolatile() || load->isAtomic())
                return false; // Don't touch anything that has to do with volatiles or atomics
            continue;
        }
        
        if (auto* store = llvm::dyn_cast<llvm::StoreInst>(user))
        {
            if (store->isVolatile() || store->isAtomic())
                return false; // Don't touch anything that has to do with volatiles or atomics
            if (store->getValueOperand() == allocaInstruciton)
                return false; // Something is using pointer to this alloca, leave it alone
            continue;
        }

        // used as GEP, call, bitcast, ptrtoint select or fucking whatever, just don't touch it if it's not a normal computation value
        // There's really no point in obfuscating it in that case, tho it could be handled
        return false;
    }
    return true;
}

void LeetObfuscator::VariableSplittingPass::SplitAlloca(llvm::AllocaInst* allocaInstruction, llvm::IntegerType* intType, uint64_t byteSize)
{
    llvm::IRBuilder<> entryBuilder(allocaInstruction);

    llvm::Type* i8Type = entryBuilder.getInt8Ty();

    llvm::ArrayType* splitArrayType = llvm::ArrayType::get(i8Type, byteSize);
    llvm::AllocaInst* splitArray = entryBuilder.CreateAlloca(splitArrayType, nullptr, allocaInstruction->getName() + ".split");
    splitArray->setAlignment(allocaInstruction->getAlign());

    std::vector<llvm::LoadInst*> loads;
    std::vector<llvm::StoreInst*> stores;

    for (auto* user : allocaInstruction->users())
    {
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(user))
            loads.push_back(load);
        if (auto* store = llvm::dyn_cast<llvm::StoreInst>(user))
            stores.push_back(store);
    }

    llvm::errs() << "Replacing " << loads.size() << " loads\n";
    llvm::errs() << "Replacing " << stores.size() << " stores\n";

    auto getBytePtr = [&](llvm::IRBuilder<>& builder, uint32_t byteIndex) -> llvm::Value*
    {
        llvm::Value* index = builder.getInt32(byteIndex);
        return builder.CreateInBoundsGEP(splitArrayType, splitArray, {builder.getInt32(0), index});
    };

    // Store individual bytes
    for (llvm::StoreInst* store : stores)
    {
        llvm::IRBuilder<> builder(store);
        llvm::Value* storedValue = store->getValueOperand();

        for (uint32_t part = 0; part < byteSize; part++)
        {
            llvm::Value* shifted = builder.CreateLShr(storedValue, llvm::ConstantInt::get(intType, part * 8));
            llvm::Value* byteValue = builder.CreateTrunc(shifted, builder.getInt8Ty());
            builder.CreateStore(byteValue, getBytePtr(builder, part));
        }
        store->eraseFromParent();
    }

    // Reconstruct bytes into a value
    for (llvm::LoadInst* load : loads)
    {
        llvm::IRBuilder<> builder(load);
        llvm::Value* accumulatedValue = llvm::ConstantInt::get(intType, 0);

        for (uint32_t part = 0; part < byteSize; part++)
        {
            llvm::Value* byteVal = builder.CreateLoad(builder.getInt8Ty(), getBytePtr(builder, part));
            llvm::Value* extended = builder.CreateZExt(byteVal, intType);
            llvm::Value* shifted = builder.CreateShl(extended, llvm::ConstantInt::get(intType, part * 8));
            accumulatedValue = builder.CreateOr(accumulatedValue, shifted);
        }
        load->replaceAllUsesWith(accumulatedValue);
        load->eraseFromParent();
    }

    allocaInstruction->eraseFromParent();
}