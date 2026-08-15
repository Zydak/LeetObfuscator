#pragma once

#include "llvm/IR/Function.h"

#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <limits>
#include "RandomNumberGenerator.h"

namespace LeetObfuscator
{
    class SettingsParser
    {
    public:
        // vector<pair<key, vector<values>>>
        using PassArguments = std::vector<std::pair<std::string, std::vector<std::string>>>;

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
            NanomitesPass
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

            uint32_t maxBlockSize = 0;
            uint32_t minBlockSize = 1;
            uint32_t blockSplitterProbability = 100;
            uint32_t blockSplitSize = 50;

            uint32_t dispatcherProbability = 100;

            uint32_t antiAnalysisProbability = 100;
            BogusInsertPosition antiAnalysisInsertPosition = BogusInsertPosition::Random;
            uint32_t antiAnalysisRdtscProbability = 0;
            uint32_t validBogusBlocksProbability = 0; // Unused for now
            uint32_t invalidBogusBlocksProbability = 100; // Unused for now

            uint32_t antiAliasingProbability = 100;

            uint32_t aambaProbability = 100;
            std::vector<std::string> aambaTargetOps;

            uint32_t nanomitesCallsProbability = 100;
            uint32_t nanomitesJumpsProbability = 0;
        };

        static FunctionAttributes ParseFunctionAttributes(
            llvm::Function& function, PassType passType, const PassArguments& passArguments);
        static GlobalAttributes ParseGlobalAttributes();
        static Pass ParsePassString(const std::string& passStr);

        static bool ShouldSkipFunction(llvm::Function* function, const FunctionAttributes& attributes);
        static bool ShouldSkipBlock(llvm::BasicBlock* block, const FunctionAttributes& attributes);
        static std::shared_ptr<RandomNumberGenerator> GetGenerator(const FunctionAttributes& attributes);
        static std::shared_ptr<RandomNumberGenerator> GetGenerator(); // TODO
        static const std::vector<std::string>* FindArgument(const PassArguments& arguments, llvm::StringRef key);

    private:
        using OptionApplier = std::function<void(llvm::Function&, const std::vector<std::string>*, llvm::StringRef, FunctionAttributes&)>;

        struct Option
        {
            llvm::StringRef name;
            OptionApplier apply;
        };

        static void SetArgument(PassArguments& arguments, llvm::StringRef key, std::vector<std::string> values);
        static std::vector<std::string> ParseValues(llvm::StringRef value);
        static std::vector<std::string> GetFunctionOption(llvm::Function& function, llvm::StringRef key);
        static void ReportInvalidArgument(llvm::Function& function, llvm::StringRef key, llvm::StringRef reason);

        template <typename T>
        static bool ParseUnsignedArgument(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef key, T& output, T maximum);

        static void ParseStringList(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef key, std::vector<std::string>& output);

        template <typename T>
        static bool ParseEnumArgument(
            llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef key,
            T& output, const std::vector<std::pair<llvm::StringRef, T>>& namedValues, llvm::StringRef expected
        );

        template <typename T>
        static OptionApplier UnsignedOption(T FunctionAttributes::* field, T maximum = std::numeric_limits<T>::max());

        static OptionApplier StringListOption(std::vector<std::string> FunctionAttributes::* field);

        template <typename T>
        static OptionApplier EnumOption(T FunctionAttributes::* field, std::vector<std::pair<llvm::StringRef, T>> namedValues, llvm::StringRef expected);

        static const std::vector<std::pair<llvm::StringRef, GlobalParseMode>> kParseModeValues;

        static void ApplyRuntimeSeed(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result);
        static void ApplyDefaultParseMode(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result);
        static void ApplySkip(llvm::Function&, const std::vector<std::string>* values, llvm::StringRef, FunctionAttributes& result);
        static void ApplyForcePass(llvm::Function&, const std::vector<std::string>* values, llvm::StringRef, FunctionAttributes& result);

        static const std::vector<Option>& GetPassOptions(PassType passType);
        static void OverlayFunctionAttributes(llvm::Function& function, llvm::StringRef attributePrefix, const std::vector<Option>& options, PassArguments& effective);
        static void ExtractOptions(llvm::Function& function, const std::vector<Option>& options, const PassArguments& effective, FunctionAttributes& result);
        static bool IsKnownOption(const std::vector<Option>& options, llvm::StringRef key);

        static size_t FindTopLevelSeparator(llvm::StringRef text, char separator, int& depth);
        static void ParsePassList(GlobalAttributes& settings, llvm::StringRef value);
        static inline std::unique_ptr<GlobalAttributes> m_GlobalSettings = nullptr;
        static PassType ParsePassTypeName(llvm::StringRef passName);
        static llvm::StringRef GetPassTypeName(PassType passType);
    };
}