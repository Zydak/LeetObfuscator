#pragma once

#include "llvm/IR/Function.h"

#include <memory>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include "RandomNumberGenerator.h"

namespace LeetObfuscator
{
    class SettingsParser
    {
    public:
        // vector<pair<key, values>>
        using PassArguments = std::vector<std::pair<std::string, std::string>>;

        enum class GlobalParseMode { All, None };
        enum class BogusInsertPosition { Random, Start };

        enum class PassType
        {
            INVALID,
            StringEncryptionPass,
            MBAPass,
            BlockSplitterPass,
            DispatcherPass,
            AAMBAPass,
            AntiAnalysisPass,
            AntiAliasingPass,
            NanomitesPass,
            VariableSplittingPass
        };

        struct Pass
        {
            PassType type;
            PassArguments parameters;
        };

        struct GlobalAttributes
        {
            PassArguments parameters;
            std::vector<Pass> passes;
        };

        struct FunctionAttributes
        {
            bool skip = false;
            bool force = false;
            
            uint64_t runtimeSeed = 0;
            uint64_t minFunctionSize = 0;
            uint64_t maxFunctionSize = 0;

            uint32_t stringEncryptionProbability = 100;
            uint32_t stringDecryptInlineProbability = 50;

            uint32_t mbaExpansionCount = 2;
            std::vector<std::string> mbaInstructionSet; // Unused for now
            uint32_t mbaProbability = 100;

            uint32_t maxBlockSize = 0;
            uint32_t minBlockSize = 1;
            uint32_t blockSplitterProbability = 100;
            uint32_t blockSplitSize = 50;

            uint32_t dispatcherProbability = 100;

            uint32_t antiAnalysisProbability = 100;
            BogusInsertPosition antiAnalysisInsertPosition = BogusInsertPosition::Random;
            uint32_t antiAnalysisRdtscRatio = 25;
            uint32_t antiAnalysisOpaqueRatio = 100;
            uint32_t antiAnalysisPIDRatio = 1;
            uint32_t antiAnalysisBlackListRatio = 1;
            bool antiAnalysisOnlyEntryBlock = false;
            bool antiAnalysisDelayedPoisoning = true;
            uint32_t antiAnalysisPoisonProbability = 50;
            uint32_t antiAnalysisLocalPoisonProbability = 50;

            uint32_t antiAliasingProbability = 100;
            uint32_t antiAliasingOpaqueProbability = 50;
            uint32_t antiAliasingReuseProbability = 70;

            uint32_t aambaProbability = 100;
            std::vector<std::string> aambaTargetOps;

            uint32_t nanomitesProbability = 100;
            uint32_t nanomitesTrampolineProbability = 50;
            uint32_t variableSplittingProbability = 100;
            uint32_t variableSplittingCount = 2;
            uint32_t variableSplittingMBAProbability = 100;
        };

        static FunctionAttributes ParseFunctionAttributes(llvm::Function& function, PassType passType, const PassArguments& passArguments);
        static std::shared_ptr<GlobalAttributes> ParseGlobalAttributes();
        static const std::string* FindArgument(const PassArguments& arguments, std::string key);

        static bool ShouldSkipFunction(llvm::Function* function, const FunctionAttributes& attributes);
        static bool ShouldSkipBlock(llvm::BasicBlock* block, const FunctionAttributes& attributes);
        static std::shared_ptr<RandomNumberGenerator> GetGenerator(const FunctionAttributes& attributes);
        static std::shared_ptr<RandomNumberGenerator> GetGenerator(); // TODO

    private:
        using OptionApplier = std::function<void(const std::string*, const std::string&, FunctionAttributes&)>;

        struct Option
        {
            std::string name;
            OptionApplier applier;
        };
        static inline std::shared_ptr<GlobalAttributes> m_GlobalSettings = nullptr;
        static PassType GetPassTypeFromName(const std::string& name);
        static std::string GetPassNameFromType(PassType type);

        static const std::vector<Option>& GetPassOptions(PassType passType);
        static void ReportInvalidArgument(const std::string& key, const std::string& reason);

        template <typename T>
        static OptionApplier UnsignedOption(T FunctionAttributes::* field, T maximum = std::numeric_limits<T>::max());
        template <typename T>
        static bool ParseUnsignedArgument(const std::string* value, const std::string& key, T& output, T maximum);

        template <typename T>
        static OptionApplier BoolOption(T FunctionAttributes::* field);
        static bool ParseBoolArgument(const std::string* value, const std::string& key, bool& output);

        static OptionApplier StringOption(std::string FunctionAttributes::* field);

        template <typename T>
        static OptionApplier EnumOption(T FunctionAttributes::* field, std::vector<std::pair<std::string, T>> namedValues, std::string expected);
        template <typename T>
        static bool ParseEnumArgument(const std::string* value, const std::string& key, T& output, const std::vector<std::pair<std::string, T>>& namedValues, std::string expected);
    };
}