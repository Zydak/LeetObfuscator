#pragma once

#include "llvm/IR/Function.h"

#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace LeetObfuscator
{
    class SettingsParser
    {
    public:
        // vector<pair<key, vector<values>>>
        using PassArguments = std::vector<std::pair<std::string, std::vector<std::string>>>;

        enum class GlobalParseMode { All, None };
        enum class BogusBlockCount { Max, Half, Random, Constant };

        enum class PassType
        {
            INVALID,
            StringEncryptionPass,
            MBAPass,
            BlockSplitterPass,
            DispatcherPass,
            AAMBAPass,
            AntiAnalysisPass,
            AntiAliasingPass
        };

        struct Pass
        {
            PassType type;
            PassArguments parameters;
        };

        struct GlobalAttributes
        {
            GlobalParseMode defaultParseMode = GlobalParseMode::All;
            uint64_t defaultRuntimeSeed = 0;
            uint32_t stringEncryptionProbability;
            PassArguments parameters;
            std::vector<Pass> passes;
        };

        // All settings after merging global defaults, pass defaults, and
        // function annotations. ParseFunctionAttributes validates every raw
        // string argument before returning this structure.
        struct FunctionAttributes
        {
            bool skip = false;
            uint64_t runtimeSeed = 0;
            uint64_t minFunctionSize = 0;
            uint64_t maxFunctionSize = 0;

            uint32_t stringEncryptionProbability = 100;

            uint32_t mbaExpansionCount = 2;
            std::vector<std::string> mbaInstructionSet; // Unused for now
            uint32_t mbaProbability = 100;

            uint32_t maxBlockSize = 50;
            uint32_t minBlockSize = 1;
            uint32_t blockSplitterProbability = 100;

            uint32_t dispatcherProbability = 100;

            BogusBlockCount bogusBlockCount = BogusBlockCount::Max;
            uint32_t validBogusBlocksProbability = 0;
            uint32_t invalidBogusBlocksProbability = 100;

            uint32_t aambaProbability = 100;
            std::vector<std::string> aambaTargetOps;
        };

        static FunctionAttributes ParseFunctionAttributes(
            llvm::Function& function, PassType passType, const PassArguments& passArguments);
        static GlobalAttributes ParseGlobalAttributes();
        static Pass ParsePassString(const std::string& passStr);

    private:
        static inline std::unique_ptr<GlobalAttributes> m_GlobalSettings = nullptr;
        static PassType ParsePassTypeName(llvm::StringRef passName);
        static llvm::StringRef GetPassTypeName(PassType passType);
    };
}