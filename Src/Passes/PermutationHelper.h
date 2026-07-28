#pragma once

#include "llvm/IR/Module.h"

namespace LeetObfuscator
{
    llvm::Function* GetOrEmitLeetPermutationWithDeps(llvm::Module &module);
}