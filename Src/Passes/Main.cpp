#include "llvm/IR/PassManager.h"
#include "llvm/Plugins/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/Module.h"

#include "MBAPass.h"
#include "StringEncryptionPass.h"
#include "BlockSplitterPass.h"
#include "DispatcherPass.h"
#include "AnnotationPass.h"

#include "SettingsParser.h"

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
                    // Annotation pass is mandatory
                    passManager.addPass(LeetObfuscator::AnnotationPass());

                    LeetObfuscator::SettingsParser::GlobalAttributes globalSettings = LeetObfuscator::SettingsParser::ParseGlobalAttributes();
                    // add passes according to the config
                    for (auto& pass : globalSettings.passes)
                    {
                        switch (pass.type)
                        {
                            case LeetObfuscator::SettingsParser::PassType::StringEncryptionPass:
                                passManager.addPass(LeetObfuscator::StringEncryptionPass());
                                break;
                            case LeetObfuscator::SettingsParser::PassType::MBAPass:
                                passManager.addPass(LeetObfuscator::MBAPass(pass.expansionCount));
                                break;
                            case LeetObfuscator::SettingsParser::PassType::BlockSplitterPass:
                                passManager.addPass(LeetObfuscator::BlockSplitterPass());
                                break;
                            case LeetObfuscator::SettingsParser::PassType::DispatcherPass:
                                passManager.addPass(LeetObfuscator::DispatcherPass());
                                break;
                        }
                    }

                }
            );
        }
    };
}