#include "llvm/IR/PassManager.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Module.h"

#include "MBAPass.h"
#include "StringEncryptionPass.h"
#include "BlockSplitterPass.h"
#include "DispatcherPass.h"

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo llvmGetPassPluginInfo()
{
    return
    {
        LLVM_PLUGIN_API_VERSION,
        "LeetObfuscatorPass",
        "0.0.1",
        [](llvm::PassBuilder& passBuilder)
        {
            passBuilder.registerOptimizerLastEPCallback(
                [](llvm::ModulePassManager& passManager, llvm::OptimizationLevel, llvm::ThinOrFullLTOPhase)
                {
                    passManager.addPass(LeetObfuscator::StringEncryptionPass());
                    passManager.addPass(LeetObfuscator::MBAPass(2));
                    passManager.addPass(LeetObfuscator::BlockSplitterPass());
                    passManager.addPass(LeetObfuscator::DispatcherPass());
                    passManager.addPass(LeetObfuscator::MBAPass(1));

                }
            );
        }
    };
}