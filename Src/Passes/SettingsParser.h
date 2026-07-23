#pragma once

#include "llvm/IR/Function.h"
#include <vector>
#include <memory>

namespace LeetObfuscator
{
    class SettingsParser
    {
    public:
        enum class GlobalParseMode
        {
            All,
            None
        };

        enum class PassType
        {
            StringEncryptionPass,
            MBAPass,
            BlockSplitterPass,
            DispatcherPass
        };

        struct Pass
        {
            PassType type;
            uint32_t expansionCount; // Only used for MBAPass, ignored for other passes
        };

        struct GlobalAttributes
        {
            uint32_t maxBlockSize;
            GlobalParseMode parseMode;
            std::vector<Pass> passes;
        };

        struct FunctionAttributes
        {
            bool skip;
            
            uint32_t maxBlockSize;
            int mbaExpansionCount;
        };

        static FunctionAttributes ParseFunctionAttributes(llvm::Function& function);
        static GlobalAttributes ParseGlobalAttributes();
        static Pass ParsePassString(const std::string& passStr);
    private:
        static inline std::unique_ptr<GlobalAttributes> m_GlobalSettings = nullptr;
    };
}