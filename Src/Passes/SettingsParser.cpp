#include "SettingsParser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include "llvm/IR/InstIterator.h"

template <typename T>
LeetObfuscator::SettingsParser::OptionApplier LeetObfuscator::SettingsParser::UnsignedOption(T FunctionAttributes::* field, T maximum)
{
    return [field, maximum](const std::string* values, const std::string& name, FunctionAttributes& result)
    {
        ParseUnsignedArgument<T>(values, name, result.*field, maximum);
    };
}

template <typename T>
bool LeetObfuscator::SettingsParser::ParseUnsignedArgument(const std::string* value, const std::string& key, T& output, T maximum)
{
    if (!value)
        return true;

    if (value->empty() || std::stoull(*value, nullptr, 10) > maximum)
    {
        ReportInvalidArgument(key, "expected an unsigned integer in range");
        return false;
    }
    uint64_t parsed = std::stoull(*value, nullptr, 10);
    output = (T)parsed;
    return true;
}

bool LeetObfuscator::SettingsParser::ParseBoolArgument(const std::string* value, const std::string& key, bool& output)
{
    if (!value)
        return true;

    if (*value == "true" || *value == "True" || *value == "1")
    {
        output = true;
        return true;
    }
    if (*value == "false" || *value == "False" || *value == "0")
    {
        output = false;
        return true;
    }

    ReportInvalidArgument(key, "expected true or false");
    return false;
}

template <typename T>
bool LeetObfuscator::SettingsParser::ParseEnumArgument(const std::string* value, const std::string& key, T& output, const std::vector<std::pair<std::string, T>>& namedValues, std::string expected)
{
    if (!value)
        return true;

    for (const auto& entry : namedValues)
    {
        if (entry.first == *value)
        {
            output = entry.second;
            return true;
        }
    }

    ReportInvalidArgument(key, expected);
    return false;
}

void LeetObfuscator::SettingsParser::ReportInvalidArgument(const std::string& key, const std::string& reason)
{
    llvm::errs() << "LeetObfuscator: invalid '" << key << "': " << reason << "; using the default value\n";
}

template <typename T>
LeetObfuscator::SettingsParser::OptionApplier LeetObfuscator::SettingsParser::BoolOption(T FunctionAttributes::* field)
{
    return [field](const std::string* value, const std::string& key, FunctionAttributes& result)
    {
        ParseBoolArgument(value, key, result.*field);
    };
}

LeetObfuscator::SettingsParser::OptionApplier LeetObfuscator::SettingsParser::StringOption(std::string FunctionAttributes::* field)
{
    return [field](const std::string* value, const std::string& key, FunctionAttributes& result)
    {
        if (value)
            result.*field = *value;
    };
}

template <typename T>
LeetObfuscator::SettingsParser::OptionApplier LeetObfuscator::SettingsParser::EnumOption(T FunctionAttributes::* field, std::vector<std::pair<std::string, T>> namedValues, std::string expected)
{
    return [field, namedValues = std::move(namedValues), expected](const std::string* value, const std::string& key, FunctionAttributes& result)
    {
        ParseEnumArgument(value, key, result.*field, namedValues, expected);
    };
}

// Settings specific to one pass, namespaced as "leet.<PassName>.<name>".
const std::vector<LeetObfuscator::SettingsParser::Option>& LeetObfuscator::SettingsParser::GetPassOptions(SettingsParser::PassType passType)
{
    using FA = FunctionAttributes;

    static const std::vector<Option> stringEncryptionOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"probability", UnsignedOption(&FA::stringEncryptionProbability, 100u)},
        {"inlineProbability", UnsignedOption(&FA::stringDecryptInlineProbability, 100u)},
    };
    static const std::vector<Option> mbaOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"minBlockSize", UnsignedOption(&FunctionAttributes::minBlockSize)},
        {"maxBlockSize", UnsignedOption(&FunctionAttributes::maxBlockSize)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"expansionCount", UnsignedOption(&FA::mbaExpansionCount)},
        {"probability", UnsignedOption(&FA::mbaProbability, 100u)},
    };
    static const std::vector<Option> blockSplitterOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"probability", UnsignedOption(&FA::blockSplitterProbability, 100u)},
        {"blockSplitSize", UnsignedOption(&FA::blockSplitSize)},
    };
    static const std::vector<Option> dispatcherOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::dispatcherProbability, 100u)},
        {"jumpTableSlotsPerBlock", UnsignedOption(&FA::dispatcherJumpTableSlotsPerBlock)},
        {"jumpTableDispatcherSlots", UnsignedOption(&FA::dispatcherJumpTableDispatcherSlots)},
        {"jumpTableMaxBlocksForMultiSlot", UnsignedOption(&FA::dispatcherJumpTableMaxBlocksForMultiSlot)},
        {"jumpTableMinSize", UnsignedOption(&FA::dispatcherJumpTableMinSize)},
        {"jumpTableMaxSize", UnsignedOption(&FA::dispatcherJumpTableMaxSize)},
        {"jumpTablePadding", UnsignedOption(&FA::dispatcherJumpTablePadding)},
        {"MBAProbability", UnsignedOption(&FA::dispatcherMBAProbability, 100u)},
        {"stateHardening", BoolOption(&FA::dispatcherStateHardening)},
    };
    static const std::vector<Option> antiAnalysisOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"minBlockSize", UnsignedOption(&FunctionAttributes::minBlockSize)},
        {"maxBlockSize", UnsignedOption(&FunctionAttributes::maxBlockSize)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::antiAnalysisProbability, 100u)},
        {"bogusInsertPosition", EnumOption<SettingsParser::BogusInsertPosition>(&FA::antiAnalysisInsertPosition, {
            {"start", SettingsParser::BogusInsertPosition::Start},
            {"random", SettingsParser::BogusInsertPosition::Random},
        }, "expected start or random")},
        {"rdtscRatio", UnsignedOption(&FunctionAttributes::antiAnalysisRdtscRatio)},
        {"opaqueRatio", UnsignedOption(&FunctionAttributes::antiAnalysisOpaqueRatio)},
        {"pidRatio", UnsignedOption(&FunctionAttributes::antiAnalysisPIDRatio)},
        {"blackListRatio", UnsignedOption(&FunctionAttributes::antiAnalysisBlackListRatio)},
        {"onlyEntryBlock", BoolOption(&FunctionAttributes::antiAnalysisOnlyEntryBlock)},
        {"delayedPoisoning", BoolOption(&FunctionAttributes::antiAnalysisDelayedPoisoning)},
        {"poisonProbability", UnsignedOption(&FunctionAttributes::antiAnalysisPoisonProbability, 100u)},
        {"localPoisonProbability", UnsignedOption(&FunctionAttributes::antiAnalysisLocalPoisonProbability, 100u)},
    };
    static const std::vector<Option> aambaOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"minBlockSize", UnsignedOption(&FunctionAttributes::minBlockSize)},
        {"maxBlockSize", UnsignedOption(&FunctionAttributes::maxBlockSize)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::aambaProbability, 100u)},
    };
    static const std::vector<Option> antiAliasingOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::antiAliasingProbability, 100u)},
        {"opaqueProbability", UnsignedOption(&FA::antiAliasingOpaqueProbability, 100u)},
        {"reuseProbability", UnsignedOption(&FA::antiAliasingReuseProbability, 100u)},
        {"MBAProbability", UnsignedOption(&FA::antiAliasingMBAProbability, 100u)},
        {"maxCandidates", UnsignedOption(&FA::antiAliasingMaxCandidates)},
        {"slotsPerCandidate", UnsignedOption(&FA::antiAliasingSlotsPerCandidate)},
        {"maxTableSize", UnsignedOption(&FA::antiAliasingMaxTableSize)},
        {"minCandidateSize", UnsignedOption(&FA::antiAliasingMinCandidateSize)},
        {"maxCandidateSize", UnsignedOption(&FA::antiAliasingMaxCandidateSize)},
        {"minCandidates", UnsignedOption(&FA::antiAliasingMinCandidates)},
    };
    static const std::vector<Option> nanomitesOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::nanomitesProbability, 100u)},
        {"trampolineProbability", UnsignedOption(&FA::nanomitesTrampolineProbability, 100u)},
    };
    static const std::vector<Option> variableSplittingOptions = {
        {"defaultParseMode", EnumOption<bool>(&FA::skip, {{"all", false}, {"none", true}}, "expected all or none")},
        {"skip", BoolOption(&FA::skip)},
        {"forcePass", BoolOption(&FA::force)},
        {"runtimeSeed", UnsignedOption(&FA::runtimeSeed)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::variableSplittingProbability, 100u)},
        {"splitCount", UnsignedOption(&FA::variableSplittingCount, 100u)},
        {"MBAProbability", UnsignedOption(&FA::variableSplittingMBAProbability, 100u)},
    };
    static const std::vector<Option> noOptions;

    switch (passType)
    {
        case SettingsParser::PassType::StringEncryptionPass: return stringEncryptionOptions;
        case SettingsParser::PassType::MBAPass: return mbaOptions;
        case SettingsParser::PassType::BlockSplitterPass: return blockSplitterOptions;
        case SettingsParser::PassType::DispatcherPass: return dispatcherOptions;
        case SettingsParser::PassType::AntiAnalysisPass: return antiAnalysisOptions;
        case SettingsParser::PassType::AAMBAPass: return aambaOptions;
        case SettingsParser::PassType::AntiAliasingPass: return antiAliasingOptions;
        case SettingsParser::PassType::NanomitesPass: return nanomitesOptions;
        case SettingsParser::PassType::VariableSplittingPass: return variableSplittingOptions;
        default: return noOptions;
    }
}

bool LeetObfuscator::SettingsParser::ShouldSkipFunction(llvm::Function *function, const FunctionAttributes& attributes)
{
    if (attributes.force)
        return false;
    
    size_t instructionCount = std::distance(llvm::inst_begin(function), llvm::inst_end(function));
    if (instructionCount == 0 ||
        (attributes.maxFunctionSize != 0 && instructionCount > attributes.maxFunctionSize) ||
        (attributes.minFunctionSize != 0 && instructionCount < attributes.minFunctionSize) ||
        attributes.skip ||
        function->getName().find(".llvm") != std::string::npos
    )
    {
        return true;
    }
    return false;
}

bool LeetObfuscator::SettingsParser::ShouldSkipBlock(llvm::BasicBlock *block, const FunctionAttributes& attributes)
{
    if ((attributes.maxBlockSize != 0 && block->size() > attributes.maxBlockSize) ||
        (attributes.minBlockSize != 0 && block->size() < attributes.minBlockSize)
    )
    {
        return true;
    }
    return false;
}

std::shared_ptr<LeetObfuscator::RandomNumberGenerator> LeetObfuscator::SettingsParser::GetGenerator(const FunctionAttributes &attributes)
{
    std::shared_ptr<RandomNumberGenerator> generator = RandomNumberGenerator::GetGlobalRandomNumberGenerator();
    if (generator->GetSeed() != attributes.runtimeSeed)
    {
        generator = std::make_shared<RandomNumberGenerator>(attributes.runtimeSeed); // This function has unique seed
    }

    return generator;
}

// Todo get rid of this for nanomites machine func
std::shared_ptr<LeetObfuscator::RandomNumberGenerator> LeetObfuscator::SettingsParser::GetGenerator()
{
    std::shared_ptr<RandomNumberGenerator> generator = RandomNumberGenerator::GetGlobalRandomNumberGenerator();
    return generator;
}

std::shared_ptr<LeetObfuscator::SettingsParser::GlobalAttributes> LeetObfuscator::SettingsParser::ParseGlobalAttributes()
{
    if (m_GlobalSettings)
        return m_GlobalSettings;

    m_GlobalSettings = std::make_shared<GlobalAttributes>();

    if (!std::filesystem::exists("Leet.conf"))
    {
        llvm::errs() << "The settings file doesn't exist, Would you like to create one? (y/n): ";
        char response;
        std::cin >> response;
        if (response != 'y' && response != 'Y')
        {
            llvm::errs() << "No default settings file, exiting.\n";
            exit(1);
        }

        std::ofstream file("Leet.conf");

        std::cout << "Creating " << std::filesystem::current_path().c_str() << "/Leet.conf" << std::endl;

        file << R"(# Leet Obfuscator Config
# Priority order:
# 1. Global defaults
# 2. Pass specific arguments
# 3. Function attributes (forcePass / skip etc. set through provided macros)
#
# GLOBAL SETTINGS
#
# defaultParseMode:
#   'all'  = process every function unless it has a skip annotation
#   'none' = only process functions you explicitly mark with forcePass
#
# runtimeSeed:
#   <number> = uint64_t fixed seed for reproducible results
#
# minFunctionSize / maxFunctionSize:
#   Skip functions that are too small or too big. 0 = no limit.
#   These are measured in instruction count. Useful to skip tiny inline functions
#   or massive functions you don't really want to process
#
# minBlockSize / maxBlockSize:
#   Same idea but for basic blocks. 0 = no limit.
#   Passes that work on basic blocks (like MBA, AAMBA, AntiAnalysis) will skip
#   blocks outside this range.
#
# PASS SPECIFIC SETTINGS
# Can be set on almost any pass, MBAPass(probability=50)
#
# Available: defaultParseMode, skip, forcePass, runtimeSeed, probability,
# minFunctionSize, maxFunctionSize, minBlockSize, maxBlockSize
# probability is 0-100.
#
# Some passes have additional specific attributes
#
# THE PASSES
#
# StringEncryptionPass:
#   Performance impact: Very small
#   Just a decryption call per string use. You won't notice it unless you have a ton of strings.
#
#   Attributes: defaultParseMode, skip, forcePass, probability
#
# MBAPass (Mixed Boolean Arithmetic):
#   Performance impact: Mild to high (depends on expansionCount)
#
#   Attributes:
#   expansionCount (int): How many times to expand each operation. 1-3 is the usual range,
#                          2 is a solid default. I wouldn't go higher than that since it grows exponentially.
#   instructionSet (string): List of instruction types to target (pipe-separated, currently unused)
#   probability (0-100): Chance to apply the transform to an operation.
#
# BlockSplitterPass:
#   Performance impact: High when running with the dispatcher
#
#   Attributes:
#   blockSplitSize (int): Target size for the split blocks. Default is 50 instructions.
#   probability (0-100): Chance to split a block.
#
# DispatcherPass:
#   Performance impact: High (scales with block count, since you'll have an indirect jump for every block in the function)
#
#   Attributes:
#   probability (0-100): Chance to apply control flow flattening to a function.
#
# AntiAnalysisPass:
#   Performance impact: Very small, the bogus blocks are never executed, so it's only 1 opaque check / rdtsc.
#
#   Attributes:
#   bogusInsertPosition (start|random): Where to insert bogus blocks in the function.
#   rdtscProbability: Probability of inserting an anti debug RDTSC check
#
# VariableSplittingPass:
#   Performance impact: High, also bloats the binary size by a LOT
#
#   Attributes:
#   probability (0-100): Chance to split operands of an instruction
#   splitCount: how many times to split the variable, uint32_t on splitCount 1 becomes 2 uint16_t, on splitCount 2, 4 uint8_t and so on.
#
# AntiAliasingPass:
#   Performance impact: low to mild
#
#   Attributes:
#   probability (0 to 100): Chance to apply anti aliasing to a function.
#   MBAProbability (0 to 100): Chance to apply Mixed Boolean Arithmetic to address calculations.
#   opaqueProbability (0 to 100): Chance to emit opaque predicates with decoy addresses.
#   reuseProbability (0 to 100): Chance to reuse computed address within a basic block.
#   maxCandidates: Maximum candidate variables to alias per function (0 for unlimited).
#   slotsPerCandidate: Number of table slots allocated per candidate variable.
#   maxTableSize: Maximum permutation table size (0 for unlimited).
#   minCandidateSize: Minimum variable byte size to alias.
#   maxCandidateSize: Maximum variable byte size to alias (0 for unlimited).
#   minCandidates: Minimum candidate variables required to obfuscate a function.
#
# AAMBAPass (Architectural Hardening MBA):
#   Performance impact: Mild
#
#   Attributes:
#   probability (0-100): Chance to apply AAMBA to an operation.
#
# NanomitesPass:
#   Performance impact: Very high
#
#   Attributes:
#   probability (0-100): Chance to apply nanomites to a call.
#   trampolineProbability (0-100): Chance to make the call use trampoline vs forward function
#
# =====================================
# DEFAULT PRESET, works well on small binaries
# =====================================
# Made for small to medium sized binaries while keeping
# the performance reasonable. Nanomites are disabled
# by default because they have a huge performance impact.
#
# You can customize this by:
# 1. Changing global settings at the top
# 2. Adding/removing/modifying passes in the passes list
# 3. Adding parameters to individual passes, MBAPass(expansionCount=3, probability=75)
#
defaultParseMode=all
runtimeSeed=0
minFunctionSize=20
maxFunctionSize=0
minBlockSize=0
maxBlockSize=0

passes=
    StringEncryptionPass(inlineProbability=100)
    MBAPass(expansionCount=2, probability=50)
    AntiAnalysisPass(rdtscRatio=0,pidRatio=1,blackListRatio=1,opaqueRatio=0,bogusInsertPosition=start,probability=10,onlyEntryBlock=true)
    BlockSplitterPass(blockSplitSize=50)
    DispatcherPass()
    AntiAnalysisPass(rdtscRatio=1,pidRatio=0,blackListRatio=0,opaqueRatio=0,bogusInsertPosition=random,probability=25)
    MBAPass(expansionCount=1)
    AAMBAPass(probability=35)
    VariableSplittingPass(probability=100,splitCount=2)
    AntiAliasingPass()
    AntiAnalysisPass(rdtscRatio=0,pidRatio=0,blackListRatio=0,opaqueRatio=100,bogusInsertPosition=start,probability=100)
    NanomitesPass(defaultParseMode=none) # This is very expensive, I set the default to none, change it if you need to

)";
    }

    std::ifstream file("Leet.conf");
    std::string line;
    PassArguments arguments;
    bool readingPassList = false;
    while (std::getline(file, line))
    {
        llvm::StringRef text(line);
        size_t comment = text.find('#');
        if (comment != std::string::npos)
            text = text.take_front(comment);
        text = text.trim();
        if (text.empty()) continue;

        if (readingPassList)
        {
            if (line[0] != ' ')
            {
                readingPassList = false;
            }
        }

        if (readingPassList)
        {
            std::string passName = text.take_front(text.find('(')).trim().str();
            std::string passArgs = text.drop_front(text.find('(') + 1).drop_back(1).trim().str();
            PassType passType = GetPassTypeFromName(passName);
            if (passType == PassType::INVALID)
            {
                llvm::errs() << "LeetObfuscator: unknown pass '" << passName << "' in Leet.conf. ignoring\n";
                continue;
            }

            // Split arguments by comma
            std::vector<std::string> args;
            size_t start = 0;
            size_t end = passArgs.find(',');
            while (end != std::string::npos)
            {
                args.push_back(passArgs.substr(start, end - start));
                start = end + 1;
                end = passArgs.find(',', start);
            }
            args.push_back(passArgs.substr(start));

            PassArguments passArguments;
            for (const auto& arg : args)
            {
                if (arg.empty())
                    continue;

                size_t equal = arg.find('=');
                if (equal == std::string::npos)
                {
                    llvm::errs() << "LeetObfuscator: invalid argument '" << arg << "' for pass '" << passName << "' in Leet.conf. ignoring\n";
                    continue;
                }
                std::string passArgumentKey = arg.substr(0, equal);
                std::string passArgumentValue = arg.substr(equal + 1);

                passArguments.emplace_back(passArgumentKey, passArgumentValue);
            }

            m_GlobalSettings->passes.push_back({passType, std::move(passArguments)});
        }
        else
        {
            std::string value;
            std::string key;
            size_t equal = text.find('=');
            if (equal != std::string::npos)
            {
                key = text.take_front(equal).trim().str();
                value = text.drop_front(equal + 1).trim().str();
            }
            else
            {
                key = text.str();
                value = "";
            }

            if (key == "passes")
            {
                readingPassList = true;
                continue;
            }

            m_GlobalSettings->parameters.emplace_back(key, value);
        }
    }

    return m_GlobalSettings;
}

LeetObfuscator::SettingsParser::PassType LeetObfuscator::SettingsParser::GetPassTypeFromName(const std::string& name)
{
    if (name == "StringEncryptionPass") return PassType::StringEncryptionPass;
    if (name == "MBAPass") return PassType::MBAPass;
    if (name == "BlockSplitterPass") return PassType::BlockSplitterPass;
    if (name == "DispatcherPass") return PassType::DispatcherPass;
    if (name == "AAMBAPass") return PassType::AAMBAPass;
    if (name == "AntiAnalysisPass") return PassType::AntiAnalysisPass;
    if (name == "AntiAliasingPass") return PassType::AntiAliasingPass;
    if (name == "NanomitesPass") return PassType::NanomitesPass;
    if (name == "VariableSplittingPass") return PassType::VariableSplittingPass;
    return PassType::INVALID;
}

std::string LeetObfuscator::SettingsParser::GetPassNameFromType(LeetObfuscator::SettingsParser::PassType type)
{
    switch (type)
    {
    case PassType::StringEncryptionPass:
        return "StringEncryptionPass";
    case PassType::MBAPass:
        return "MBAPass";
    case PassType::BlockSplitterPass:
        return "BlockSplitterPass";
    case PassType::DispatcherPass:
        return "DispatcherPass";
    case PassType::AAMBAPass:
        return "AAMBAPass";
    case PassType::AntiAnalysisPass:
        return "AntiAnalysisPass";
    case PassType::AntiAliasingPass:
        return "AntiAliasingPass";
    case PassType::NanomitesPass:
        return "NanomitesPass";
    case PassType::VariableSplittingPass:
        return "VariableSplittingPass";
    default:
        return "INVALID";
    }
}

const std::string* LeetObfuscator::SettingsParser::FindArgument(const PassArguments& arguments, std::string key)
{
    auto it = std::find_if(arguments.begin(), arguments.end(), [key](const auto& argument)
    {
        return argument.first == key;
    });
    return it == arguments.end() ? nullptr : &it->second;
}

LeetObfuscator::SettingsParser::FunctionAttributes LeetObfuscator::SettingsParser::ParseFunctionAttributes(llvm::Function& function, PassType passType, const PassArguments& passArguments)
{
    std::shared_ptr<LeetObfuscator::SettingsParser::GlobalAttributes> globalSettings = ParseGlobalAttributes();

    FunctionAttributes attributes;

    auto passOptions = GetPassOptions(passType);
    std::string passName = GetPassNameFromType(passType);

    // Set all global attiributes first
    for (const auto& argument : globalSettings->parameters)
    {
        auto it = std::find_if(passOptions.begin(), passOptions.end(), [&argument](const Option& option)
        {
            return option.name == argument.first;
        });
        if (it != passOptions.end())
        {
            it->applier(&argument.second, argument.first, attributes);
        }
    }

    // Set pass specific attributes
    for (const auto& argument : passArguments)
    {
        auto it = std::find_if(passOptions.begin(), passOptions.end(), [&argument](const Option& option)
        {
            return option.name == argument.first;
        });
        if (it != passOptions.end())
        {
            it->applier(&argument.second, argument.first, attributes);
        }
    }

    // Set function specific attributes
    for (const auto& option : passOptions)
    {
        std::string fullAttributeName = "leet." + passName + "." + option.name;
        if (function.hasFnAttribute(fullAttributeName))
        {
            std::string value = function.getFnAttribute(fullAttributeName).getValueAsString().str();
            option.applier(&value, option.name, attributes);
        }
    }

    return attributes;
}