#include "PermutationHelper.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Scalar/Reg2Mem.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <random>
#include "SettingsParser.h"
#include "llvm/IR/NoFolder.h"

#include "llvm/IR/IntrinsicsX86.h"

//
// -------------------------------- HELPER --------------------------------
//
// Emits IR equivalent to:
//
//   uint64_t __leet_split_mix_64(uint64_t *state) {
//       uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
//       z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
//       z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
//       return z ^ (z >> 31);
//   }
//
//   extern "C" void __leet_permutation(uint32_t* table, uint32_t size)
//   {
//       uint64_t state = __rdtsc();
//       for (uint32_t i = 0; i < size; i++)
//           table[i] = i;
//       for (int i = size - 1; i > 0; i--) {
//           uint64_t z = splitmix64_next(&state);
//           int j = z % (i + 1);
//           uint32_t tmp = table[i];
//           table[i] = table[j];
//           table[j] = tmp;
//       }
//   }

// Emits `uint64_t splitmix64_next(uint64_t *state)` into the module and returns it
static llvm::Function* EmitSplitmix64Next(llvm::Module &module)
{
    llvm::LLVMContext& context = module.getContext();
    llvm::IRBuilder<> builder(context);

    llvm::FunctionType* functionType = llvm::FunctionType::get(builder.getInt64Ty(), {builder.getPtrTy()}, false);
    llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, "__leet_split_mix_64", &module);
    function->arg_begin()->setName("state");
    llvm::Argument* statePtr = function->arg_begin();

    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(context, "entry", function);
    builder.SetInsertPoint(entryBlock);

    const uint64_t k1 = 0x9E3779B97F4A7C15ULL;
    const uint64_t k2 = 0xBF58476D1CE4E5B9ULL;
    const uint64_t k3 = 0x94D049BB133111EBULL;

    // z = (*state += k1);
    llvm::Value* oldState = builder.CreateLoad(builder.getInt64Ty(), statePtr, "old");
    llvm::Value* newState = builder.CreateAdd(oldState, builder.getInt64(k1), "z");
    builder.CreateStore(newState, statePtr);

    // z = (z ^ (z >> 30)) * k2;
    llvm::Value* shr1 = builder.CreateLShr(newState, builder.getInt64(30), "shr1");
    llvm::Value* xor1 = builder.CreateXor(newState, shr1, "xor1");
    llvm::Value* mul1 = builder.CreateMul(xor1, builder.getInt64(k2), "mul1");

    // z = (z ^ (z >> 27)) * k3;
    llvm::Value* shr2 = builder.CreateLShr(mul1, builder.getInt64(27), "shr2");
    llvm::Value* xor2 = builder.CreateXor(mul1, shr2, "xor2");
    llvm::Value* mul2 = builder.CreateMul(xor2, builder.getInt64(k3), "mul2");

    // return z ^ (z >> 31);
    llvm::Value* shr3 = builder.CreateLShr(mul2, builder.getInt64(31), "shr3");
    llvm::Value* retVal = builder.CreateXor(mul2, shr3, "xor3");

    builder.CreateRet(retVal);

    llvm::verifyFunction(*function);
    return function;
}

// Emits `void __leet_permutation(uint32_t* table, uint32_t size)` into the module, calling the provided splitmix64_next
llvm::Function* EmitLeetPermutation(llvm::Module &module, llvm::Function* splitmix64Next)
{
    llvm::LLVMContext& context = module.getContext();
    llvm::IRBuilder<> builder(context);

    llvm::FunctionType* functionType = llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy(), builder.getInt32Ty()}, false);
    llvm::Function* function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, "__leet_permutation", &module);

    auto argIt = function->arg_begin();
    llvm::Argument* tablePtr = &*argIt++;
    llvm::Argument* size = &*argIt++;
    tablePtr->setName("table");
    size->setName("size");

    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(context, "entry", function);
    llvm::BasicBlock* loop1Cond = llvm::BasicBlock::Create(context, "loop1.cond", function);
    llvm::BasicBlock* loop1Body = llvm::BasicBlock::Create(context, "loop1.body", function);
    llvm::BasicBlock* loop1End = llvm::BasicBlock::Create(context, "loop1.end", function);
    llvm::BasicBlock* loop2Cond = llvm::BasicBlock::Create(context, "loop2.cond", function);
    llvm::BasicBlock* loop2Body = llvm::BasicBlock::Create(context, "loop2.body", function);
    llvm::BasicBlock* loop2End = llvm::BasicBlock::Create(context, "loop2.end", function);

    // entry: state = __rdtsc();
    builder.SetInsertPoint(entryBlock);
    llvm::AllocaInst* stateAlloca = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "state");

    llvm::Function* rdtscIntr = llvm::Intrinsic::getOrInsertDeclaration(&module, llvm::Intrinsic::x86_rdtsc);
    llvm::Value* rdtsc = builder.CreateCall(rdtscIntr, {}, "rdtsc");

    builder.CreateStore(rdtsc, stateAlloca);
    builder.CreateBr(loop1Cond);

    // loop1.cond: for (i = 0; i < size; i++)
    builder.SetInsertPoint(loop1Cond);
    llvm::PHINode* i1 = builder.CreatePHI(builder.getInt32Ty(), 2, "i1");
    i1->addIncoming(builder.getInt32(0), entryBlock);
    llvm::Value* cmp1 = builder.CreateICmpULT(i1, size, "cmp1");
    builder.CreateCondBr(cmp1, loop1Body, loop1End);

    // loop1.body: table[i] = i;
    builder.SetInsertPoint(loop1Body);
    llvm::Value* gep1 = builder.CreateInBoundsGEP(builder.getInt32Ty(), tablePtr, i1, "gep1");
    builder.CreateStore(i1, gep1);
    llvm::Value* i1Next = builder.CreateAdd(i1, builder.getInt32(1), "i1.next");
    builder.CreateBr(loop1Cond);
    i1->addIncoming(i1Next, loop1Body);

    // loop1.end: int i = size - 1;
    builder.SetInsertPoint(loop1End);
    llvm::Value* init2 = builder.CreateSub(size, builder.getInt32(1), "init2");
    builder.CreateBr(loop2Cond);

    // loop2.cond: for (; i > 0; i--)
    builder.SetInsertPoint(loop2Cond);
    llvm::PHINode* i2 = builder.CreatePHI(builder.getInt32Ty(), 2, "i2");
    i2->addIncoming(init2, loop1End);
    llvm::Value* cmp2 = builder.CreateICmpSGT(i2, builder.getInt32(0), "cmp2");
    builder.CreateCondBr(cmp2, loop2Body, loop2End);

    // loop2.body: Fisher-Yates swap
    builder.SetInsertPoint(loop2Body);
    llvm::Value* z = builder.CreateCall(splitmix64Next, {stateAlloca}, "z");

    llvm::Value* iPlus1 = builder.CreateAdd(i2, builder.getInt32(1), "ip1");
    llvm::Value* iPlus164 = builder.CreateSExt(iPlus1, builder.getInt64Ty(), "ip1.64"); // i>0 so i+1 > 0
    llvm::Value* j64 = builder.CreateURem(z, iPlus164, "j64");
    llvm::Value* j = builder.CreateTrunc(j64, builder.getInt32Ty(), "j");

    llvm::Value* gepI = builder.CreateInBoundsGEP(builder.getInt32Ty(), tablePtr, i2, "gepi");
    llvm::Value* tmp = builder.CreateLoad(builder.getInt32Ty(), gepI, "tmp");
    llvm::Value* gepJ = builder.CreateInBoundsGEP(builder.getInt32Ty(), tablePtr, j, "gepj");
    llvm::Value* tableJ = builder.CreateLoad(builder.getInt32Ty(), gepJ, "tablej");

    builder.CreateStore(tableJ, gepI);
    builder.CreateStore(tmp, gepJ);

    llvm::Value* i2Next = builder.CreateSub(i2, builder.getInt32(1), "i2.next");
    builder.CreateBr(loop2Cond);
    i2->addIncoming(i2Next, loop2Body);

    // loop2.end: return;
    builder.SetInsertPoint(loop2End);
    builder.CreateRetVoid();

    llvm::verifyFunction(*function);
    return function;
}

// Convenience entry point: emits both functions and returns __leet_permutation's llvm::Function*
llvm::Function* LeetObfuscator::GetOrEmitLeetPermutationWithDeps(llvm::Module &module)
{
    llvm::Function* splitmix64NextFunction = module.getFunction("__leet_split_mix_64");
    if (!splitmix64NextFunction)
        splitmix64NextFunction = EmitSplitmix64Next(module);

    llvm::Function* permFunction = module.getFunction("__leet_permutation");
    if (!permFunction)
        permFunction = EmitLeetPermutation(module, splitmix64NextFunction);

    return permFunction;
}