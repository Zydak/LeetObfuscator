#include "llvm/IR/PassManager.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Module.h"

class HelloPass : public llvm::PassInfoMixin<HelloPass>
{
public:
    llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager&)
    {
        for(auto& function : module)
        {
            llvm::errs() << function.getName() << "\n";
        }

        return llvm::PreservedAnalyses::all();
    }
};

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
                    passManager.addPass(HelloPass());
                }
            );
        }
    };
}