#include "SettingsParser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include "llvm/IR/InstIterator.h"

namespace LeetObfuscator
{

void SettingsParser::SetArgument(PassArguments& arguments, llvm::StringRef key, std::vector<std::string> values)
{
    auto it = std::find_if(arguments.begin(), arguments.end(), [key](const auto& argument)
    {
        return argument.first == key;
    });

    if (it == arguments.end())
        arguments.emplace_back(key.str(), std::move(values));
    else
        it->second = std::move(values);
}

const std::vector<std::string>* SettingsParser::FindArgument(const PassArguments& arguments, llvm::StringRef key)
{
    auto it = std::find_if(arguments.begin(), arguments.end(), [key](const auto& argument)
    {
        return argument.first == key;
    });
    return it == arguments.end() ? nullptr : &it->second;
}
std::vector<std::string> SettingsParser::ParseValues(llvm::StringRef value)
{
    std::vector<std::string> values;
    while (true)
    {
        auto split = value.split('|');
        values.push_back(split.first.trim().str());
        if (split.second.empty())
            return values;
        value = split.second;
    }
}
std::vector<std::string> SettingsParser::GetFunctionOption(llvm::Function& function, llvm::StringRef key)
{
    if (!function.hasFnAttribute(key))
        return {};
    return ParseValues(function.getFnAttribute(key).getValueAsString());
}
void SettingsParser::ReportInvalidArgument(llvm::Function& function, llvm::StringRef key, llvm::StringRef reason)
{
    llvm::errs()
        << "LeetObfuscator: invalid '" << key << "' for function '"
        << function.getName() << "': " << reason << "; using the default value\n";
}
template <typename T>
bool SettingsParser::ParseUnsignedArgument(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef key, T& output, T maximum)
{
    if (!values)
        return true;
    if (values->size() != 1)
    {
        ReportInvalidArgument(function, key, "expected exactly one value");
        return false;
    }

    uint64_t parsed = 0;
    llvm::StringRef value(values->front());
    if (value.empty() || value.getAsInteger(10, parsed) || parsed > maximum)
    {
        ReportInvalidArgument(function, key, "expected an unsigned integer in range");
        return false;
    }
    output = (T)parsed;
    return true;
}
void SettingsParser::ParseStringList(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef key, std::vector<std::string>& output)
{
    if (!values)
        return;
    if (std::any_of(values->begin(), values->end(), [](const std::string& value) { return value.empty(); }))
    {
        ReportInvalidArgument(function, key, "empty list elements are not allowed");
        return;
    }
    output = *values;
}
template <typename T>
bool SettingsParser::ParseEnumArgument(
    llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef key,
    T& output, const std::vector<std::pair<llvm::StringRef, T>>& namedValues, llvm::StringRef expected)
{
    if (!values)
        return true;
    if (values->size() == 1)
    {
        for (const auto& namedValue : namedValues)
        {
            if (namedValue.first == values->front())
            {
                output = namedValue.second;
                return true;
            }
        }
    }
    ReportInvalidArgument(function, key, expected);
    return false;
}

// ---------------------------------------------------------------------------
// Option tables.
//
// Every setting ParseFunctionAttributes understands is declared once
// here as an Option: a name, and a function that parses a value for that name
// into FunctionAttributes. this way ParseFunctionAttributes never manually checks
// for any parameter. Adding a new setting is one line in one
// table, and that line is the only place its name is spelled out.
// I know this is probably overcomplicated as fuck but I really had no idea how
// to nicely abstract this
// ---------------------------------------------------------------------------

template <typename T>
SettingsParser::OptionApplier SettingsParser::UnsignedOption(T FunctionAttributes::* field, T maximum)
{
    return [field, maximum](llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
    {
        ParseUnsignedArgument<T>(function, values, name, result.*field, maximum);
    };
}
SettingsParser::OptionApplier SettingsParser::StringListOption(std::vector<std::string> FunctionAttributes::* field)
{
    return [field](llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
    {
        ParseStringList(function, values, name, result.*field);
    };
}
template <typename T>
SettingsParser::OptionApplier SettingsParser::EnumOption(T FunctionAttributes::* field, std::vector<std::pair<llvm::StringRef, T>> namedValues, llvm::StringRef expected)
{
    return [field, namedValues = std::move(namedValues), expected](llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
    {
        ParseEnumArgument<T>(function, values, name, result.*field, namedValues, expected);
    };
}

const std::vector<std::pair<llvm::StringRef, SettingsParser::GlobalParseMode>> SettingsParser::kParseModeValues = {
    {"all", SettingsParser::GlobalParseMode::All},
    {"none", SettingsParser::GlobalParseMode::None},
};

void SettingsParser::ApplyRuntimeSeed(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
{
    if (!values)
        return;
    if (values->size() != 1)
    {
        ReportInvalidArgument(function, name, "expected 'auto' or one unsigned integer");
        return;
    }
    ParseUnsignedArgument<uint64_t>(function, values, name, result.runtimeSeed, std::numeric_limits<uint64_t>::max());
}
void SettingsParser::ApplyDefaultParseMode(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
{
    if (!values)
        return;

    if (values->size() != 1)
    {
        ReportInvalidArgument(function, name, "expected all or none");
        return;
    }

    const std::string& value = values->front();
    if (value == "all")
        result.skip = false;
    else if (value == "none")
        result.skip = true;
    else
        ReportInvalidArgument(function, name, "expected all or none");
}
void SettingsParser::ApplySkip(llvm::Function&, const std::vector<std::string>* values, llvm::StringRef, FunctionAttributes& result)
{
    if (!values)
        return;

    result.skip = true;
}
void SettingsParser::ApplyForcePass(llvm::Function&, const std::vector<std::string>* values, llvm::StringRef, FunctionAttributes& result)
{
    if (!values)
        return;

    result.skip = false;
}

// Settings specific to one pass, namespaced as "leet.<PassName>.<name>".
const std::vector<SettingsParser::Option>& SettingsParser::GetPassOptions(SettingsParser::PassType passType)
{
    using FA = FunctionAttributes;

    static const std::vector<Option> stringEncryptionOptions = {
        {"defaultParseMode", ApplyDefaultParseMode},
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"probability", UnsignedOption(&FA::stringEncryptionProbability, 100u)},
    };
    static const std::vector<Option> mbaOptions = {
        {"defaultParseMode", ApplyDefaultParseMode},
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"minBlockSize", UnsignedOption(&FunctionAttributes::minBlockSize)},
        {"maxBlockSize", UnsignedOption(&FunctionAttributes::maxBlockSize)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"expansionCount", UnsignedOption(&FA::mbaExpansionCount)},
        {"instructionSet", StringListOption(&FA::mbaInstructionSet)},
        {"probability", UnsignedOption(&FA::mbaProbability, 100u)},
    };
    static const std::vector<Option> blockSplitterOptions = {
        {"defaultParseMode", ApplyDefaultParseMode},
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"probability", UnsignedOption(&FA::blockSplitterProbability, 100u)},
        {"blockSplitSize", UnsignedOption(&FA::blockSplitSize)},
    };
    static const std::vector<Option> dispatcherOptions = {
        {"defaultParseMode", ApplyDefaultParseMode},
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::dispatcherProbability, 100u)},
    };
    static const std::vector<Option> antiAnalysisOptions = {
        {"defaultParseMode", ApplyDefaultParseMode},
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"minBlockSize", UnsignedOption(&FunctionAttributes::minBlockSize)},
        {"maxBlockSize", UnsignedOption(&FunctionAttributes::maxBlockSize)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::antiAnalysisProbability, 100u)},
        {"bogusInsertPosition", EnumOption<SettingsParser::BogusInsertPosition>(&FA::antiAnalysisInsertPosition, {
            {"start", SettingsParser::BogusInsertPosition::Start},
            {"random", SettingsParser::BogusInsertPosition::Random},
        }, "expected start or random")},
        {"rdtscProbability", UnsignedOption(&FunctionAttributes::antiAnalysisRdtscProbability, 100u)},
        {"validBogusBlocksProbability", UnsignedOption(&FA::validBogusBlocksProbability, 100u)},
        {"invalidBogusBlocksProbability", UnsignedOption(&FA::invalidBogusBlocksProbability, 100u)},
    };
    static const std::vector<Option> aambaOptions = {
        {"defaultParseMode", ApplyDefaultParseMode},
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"minBlockSize", UnsignedOption(&FunctionAttributes::minBlockSize)},
        {"maxBlockSize", UnsignedOption(&FunctionAttributes::maxBlockSize)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::aambaProbability, 100u)},
        {"targetOps", StringListOption(&FA::aambaTargetOps)},
    };
    static const std::vector<Option> antiAliasingOptions = {
        {"defaultParseMode", ApplyDefaultParseMode},
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::antiAliasingProbability, 100u)},
    };
    static const std::vector<Option> nanomitesOptions = {
        {"defaultParseMode", ApplyDefaultParseMode},
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::NanomitesProbability, 100u)},
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
        default: return noOptions;
    }
}
void SettingsParser::OverlayFunctionAttributes(llvm::Function& function, llvm::StringRef attributePrefix, const std::vector<Option>& options, PassArguments& effective)
{
    for (const Option& option : options)
    {
        std::string attributeKey = attributePrefix.str() + "." + option.name.str();
        if (function.hasFnAttribute(attributeKey))
            SetArgument(effective, option.name, GetFunctionOption(function, attributeKey));
    }
}
void SettingsParser::ExtractOptions(llvm::Function& function, const std::vector<Option>& options, const PassArguments& effective, FunctionAttributes& result)
{
    for (const Option& option : options)
    {
        option.apply(function, FindArgument(effective, option.name), option.name, result);
    }
}

bool SettingsParser::IsKnownOption(const std::vector<Option>& options, llvm::StringRef key)
{
    for (const auto& o : options)
        if (o.name == key)
            return true;

    // Check if the option exists for any pass type (allowing any valid parameter globally)
    const std::vector<SettingsParser::PassType> allPassTypes = {
        SettingsParser::PassType::StringEncryptionPass,
        SettingsParser::PassType::MBAPass,
        SettingsParser::PassType::BlockSplitterPass,
        SettingsParser::PassType::DispatcherPass,
        SettingsParser::PassType::AAMBAPass,
        SettingsParser::PassType::AntiAnalysisPass,
        SettingsParser::PassType::AntiAliasingPass,
        SettingsParser::PassType::NanomitesPass,
    };
    for (auto pt : allPassTypes)
    {
        const auto& opts = GetPassOptions(pt);
        for (const auto& o : opts)
            if (o.name == key)
                return true;
    }

    return false;
}
size_t SettingsParser::FindTopLevelSeparator(llvm::StringRef text, char separator, int& depth)
{
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '(') ++depth;
        else if (text[i] == ')' && depth > 0) --depth;
        else if (text[i] == separator && depth == 0) return i;
    }
    return llvm::StringRef::npos;
}
void SettingsParser::ParsePassList(SettingsParser::GlobalAttributes& settings, llvm::StringRef value)
{
    int depth = 0;
    while (true)
    {
        size_t comma = FindTopLevelSeparator(value, ',', depth);
        llvm::StringRef entry = comma == llvm::StringRef::npos ? value : value.take_front(comma);
        SettingsParser::Pass pass = SettingsParser::ParsePassString(entry.str());
        if (pass.type != SettingsParser::PassType::INVALID)
            settings.passes.push_back(std::move(pass));
        if (comma == llvm::StringRef::npos)
            return;
        value = value.drop_front(comma + 1);
    }
}

}

LeetObfuscator::SettingsParser::PassType LeetObfuscator::SettingsParser::ParsePassTypeName(llvm::StringRef passName)
{
    passName = passName.trim();
    if (passName == "StringEncryptionPass") return PassType::StringEncryptionPass;
    if (passName == "MBAPass") return PassType::MBAPass;
    if (passName == "BlockSplitterPass") return PassType::BlockSplitterPass;
    if (passName == "DispatcherPass") return PassType::DispatcherPass;
    if (passName == "AAMBAPass") return PassType::AAMBAPass;
    if (passName == "AntiAnalysisPass") return PassType::AntiAnalysisPass;
    if (passName == "AntiAliasingPass") return PassType::AntiAliasingPass;
    if (passName == "NanomitesPass") return PassType::NanomitesPass;
    return PassType::INVALID;
}

llvm::StringRef LeetObfuscator::SettingsParser::GetPassTypeName(PassType passType)
{
    switch (passType)
    {
        case PassType::StringEncryptionPass: return "StringEncryptionPass";
        case PassType::MBAPass: return "MBAPass";
        case PassType::BlockSplitterPass: return "BlockSplitterPass";
        case PassType::DispatcherPass: return "DispatcherPass";
        case PassType::AAMBAPass: return "AAMBAPass";
        case PassType::AntiAnalysisPass: return "AntiAnalysisPass";
        case PassType::AntiAliasingPass: return "AntiAliasingPass";
        case PassType::NanomitesPass: return "NanomitesPass";
        default:
            std::cout << "WRONG PASS NAME WTF?" << std::endl;
            exit(1);
            break;
    }
}

LeetObfuscator::SettingsParser::FunctionAttributes
LeetObfuscator::SettingsParser::ParseFunctionAttributes(llvm::Function& function, PassType passType, const PassArguments& passArguments)
{
    FunctionAttributes result;
    static GlobalAttributes global = ParseGlobalAttributes();
    
    // Check global defaultParseMode setting
    const std::vector<std::string>* defaultParseMode = FindArgument(global.parameters, "defaultParseMode");
    if (defaultParseMode && !defaultParseMode->empty())
    {
        if (defaultParseMode->front() == "none")
            result.skip = true;
        else if (defaultParseMode->front() == "all")
            result.skip = false;
    }

    llvm::StringRef passName = GetPassTypeName(passType);
    const std::vector<Option>& passOptions = GetPassOptions(passType);

    // Merge global defaults with this pass's own arguments, pass arguments take priority
    PassArguments effective = global.parameters;
    for (const auto& argument : passArguments)
        SetArgument(effective, argument.first, argument.second);

    // Let function attributes override the arguments
    OverlayFunctionAttributes(function, "leet." + passName.str(), passOptions, effective);

    // Report any unknown options provided for this pass (from global or pass args)
    for (const auto& arg : effective)
    {
        if (!IsKnownOption(passOptions, arg.first))
            llvm::errs() << "LeetObfuscator: unknown option '" << arg.first << "' for pass '" << passName << "'; ignoring\n";
    }

    // Everything above just produced the final argument values so now just place them into the struct
    ExtractOptions(function, passOptions, effective, result);

    if (result.maxFunctionSize != 0 && result.minFunctionSize > result.maxFunctionSize)
    {
        ReportInvalidArgument(function, "minFunctionSize/maxFunctionSize", "minimum cannot exceed maximum");
        result.minFunctionSize = 0;
        result.maxFunctionSize = 0;
    }
    if (result.maxFunctionSize != 0 && result.minBlockSize > result.maxBlockSize)
    {
        ReportInvalidArgument(function, "minBlockSize/maxBlockSize", "minimum cannot exceed maximum");
        result.maxBlockSize = 0;
        result.minBlockSize = 0;
    }

    if (function.getName().find("llvm.") != std::string::npos)
        result.skip = true;

    return result;
}

LeetObfuscator::SettingsParser::Pass LeetObfuscator::SettingsParser::ParsePassString(const std::string& passStr)
{
    llvm::StringRef text(passStr);
    text = text.trim();
    size_t open = text.find('(');
    llvm::StringRef passName = open == llvm::StringRef::npos ? text : text.take_front(open).trim();
    Pass pass{ParsePassTypeName(passName), {}};
    if (pass.type == PassType::INVALID || open == llvm::StringRef::npos)
        return pass;

    size_t close = text.rfind(')');
    if (close == llvm::StringRef::npos || close < open)
        return pass;

    llvm::StringRef parameterText = text.slice(open + 1, close);
    while (!parameterText.empty())
    {
        auto parameter = parameterText.split(',');
        llvm::StringRef item = parameter.first.trim();
        auto assignment = item.split('=');
        if (!assignment.second.empty() && !assignment.first.trim().empty())
            SetArgument(pass.parameters, assignment.first.trim(), ParseValues(assignment.second));
        parameterText = parameter.second;
    }
    return pass;
}

bool LeetObfuscator::SettingsParser::ShouldSkipFunction(llvm::Function *function, const FunctionAttributes& attributes)
{
    size_t instructionCount = std::distance(llvm::inst_begin(function), llvm::inst_end(function));
    if ((attributes.maxFunctionSize != 0 && instructionCount > attributes.maxFunctionSize) ||
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

LeetObfuscator::SettingsParser::GlobalAttributes LeetObfuscator::SettingsParser::ParseGlobalAttributes()
{
    if (m_GlobalSettings)
        return *m_GlobalSettings;

    m_GlobalSettings = std::make_unique<GlobalAttributes>();
    GlobalAttributes& settings = *m_GlobalSettings;

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
# PASS-SPECIFIC SETTINGS
# Can be set on any pass, MBAPass(probability=50)
#
# Available: defaultParseMode, skip, forcePass, runtimeSeed, probability,
# minFunctionSize, maxFunctionSize, minBlockSize, maxBlockSize
# probability is 0-100.
#
# Some passes have additional specific attributes:
# - MBAPass: expansionCount, instructionSet
# - BlockSplitterPass: blockSplitSize
# - AntiAnalysisPass: bogusInsertPosition, rdtscProbability, validBogusBlocksProbability, invalidBogusBlocksProbability
# - AAMBAPass: targetOps
#
# THE PASSES
#
# StringEncryptionPass:
#   Encrypts string literals at compile time and inserts runtime decryption at every use.
#   This completely disables the ability to search for any strings in the binary.
#   Every string has its own unique key hardcoded in the decrypt function which
#   makes dumping and decrypting them a lot harder.
#
#   Performance impact: Very small
#   Just a decryption call per string use. You won't notice it unless you have a ton of strings.
#
#   Attributes: defaultParseMode, skip, forcePass, probability
#
# MBAPass (Mixed Boolean Arithmetic):
#   Replaces simple arithmetic operations with their MBA equivalents.
#   It's basically impossible to see what the original operation did unless you run
#   it through an MBA deobfuscator first. Of course this obfuscation is kinda weak
#   because MBA is a trick older than the world so there are many tools to deal
#   with that, for example CoBRA. That's why AAMBA pass exists.
#
#   Performance impact: Mild to high (depends on expansionCount)
#
#   Attributes:
#   expansionCount (int): How many times to expand each operation. 1-3 is the usual range,
#                          2 is a solid default. I wouldn't go higher than that since it grows exponentially.
#   instructionSet (string): List of instruction types to target (pipe-separated, currently unused)
#   probability (0-100): Chance to apply the transform to an operation.
#
# BlockSplitterPass:
#   Breaks large basic blocks into smaller ones. Increases control flow complexity,
#   useless on its own, very useful together with DispatcherPass. The dispatcher
#   pass works best when there are many small blocks to route through.
#
#   Performance impact: Mild
#
#   Attributes:
#   blockSplitSize (int): Target size for the split blocks. Default is 50 instructions.
#   probability (0-100): Chance to split a block.
#
# DispatcherPass:
#   Definitely the strongest and most useful pass. It collects all the blocks inside
#   a function and makes one giant state machine out of them. It creates a jump table
#   at the beginning of the function and places all the block pointers inside it.
#   Then instead of normal jump at the end of each block everything gets routed through
#   the dispatcher which uses indirect jumps. These are almost impossible to resolve
#   statically without any execution.
#
#   Performance impact: High (scales with block count, since you'll have an indirect jump for every block in the function)
#
#   Attributes:
#   probability (0-100): Chance to apply control flow flattening to a function.
#
# AntiAnalysisPass:
#   It creates a bunch of bogus blocks containing invalid assembly. This throws
#   disassemblers immensely because if the disassembler encounters a technically
#   invalid byte that never gets executed, it will still try to make sense of it.
#   So if the byte is incomplete, it will create an instruction from whatever bytes
#   happen to be after it, that creates a desynch essentially destroying every
#   instruction after that. On Windows it's able to somewhat get through this,
#   in rare cases it will be able generate a graph and decompile what it can
#   (tho it will be broken and incomplete), while on Linux it completely breaks
#   the graph view and disables decompilation.
#
#   If the pass sees any instruction starting with 0xFF, it inserts a single 0xEB
#   byte before it. This will create JMP RIP+1, so control flow is unchanged
#   (RIP simply advances one byte into the original instruction), but disassemblers
#   become desynchronized. Instructions beginning with 0xFF are mostly INC/DEC and
#   indirect JMP/CALL. The technique is especially useful around with the dispatcher
#   pass since everything there uses indirect jumps.
#
#   Performance impact: Very small, the bogus blocks are never executed.
#
#   Attributes:
#   bogusInsertPosition (start|random): Where to insert bogus blocks in the function.
#
# AntiAliasingPass:
#   Throws every stack local in a function into one big shared stack buffer to which
#   indices are computed at runtime. This way decompilers can't alias variables and
#   even access to the same variable multiple times will show up as possibly accessing
#   different values.
#
#   Performance impact: low to mild
#
#   Attributes:
#   probability (0-100): Chance to apply anti aliasing to a function.
#
# AAMBAPass (Architectural Hardening MBA):
#   Replaces operands of binary operations with ADC(X, 255) - 255 - CF and
#   SBB(X, 255) + 255 + CF. Of course it always evaluates to X, but it makes the
#   expression dependent on the carry flag. Unless a decompiler tracks the state
#   of CF (which sometimes is impossible) it will get very confused and won't be
#   unable to fold these expressions. It pairs very nicely with the previous MBA
#   pass obfuscating the arithmetic even further. The decompiler creates additional
#   stack variables and uses a lot of __PAIR64__ and __CFADD__ calls, so it becomes
#   a lot harder to paste that into tools like CoBRA. IDA's goomba plugin is also
#   no help in simplifying this. IDA's decompiler does track the carry flag to some
#   degree, but combining this with control flow obfuscation makes tracking impossible
#   without execution.
#
#   Performance impact: Mild
#
#   Attributes:
#   probability (0-100): Chance to apply AAMBA to an operation.
#
# NanomitesPass:
#   I think the second most useful pass after the dispatcher. It obfuscates control
#   flow through exceptions. It replaces all calls with int3 traps. When the trap
#   is triggered the control flow goes to the exception handler which adjusts RIP
#   to the actual call. It also inserts invalid bytes right after the trap to
#   desynchronize the disassembler.
#
#   Performance impact: Very high, it will fire an exception leaving the program and
#   going to kernel, and interrupts are just slow. Use very sparingly. It's best to
#   set defaultParsingMode on this to none and manually mark which functions or even
#   which calls you want to obfuscate with this explicitly. Obfuscating every call
#   inside a binary is pointless and costs a lot.
#
#   Attributes:
#   probability (0-100): Chance to apply nanomites to a call.
#
# =====================================
# DEFAULT PRESET, works well on small binaries
# =====================================
# This preset is designed to provide good obfuscation for small to medium
# sized binaries while keeping performance reasonable. Nanomites are disabled
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
    StringEncryptionPass(),
    MBAPass(expansionCount=2, probability=50),
    BlockSplitterPass(blockSplitSize=50),
    AntiAnalysisPass(bogusInsertPosition=random,probability=25),
    DispatcherPass(),
    MBAPass(expansionCount=1),
    AAMBAPass(probability=35),
    AntiAliasingPass(),
    AntiAnalysisPass(bogusInsertPosition=start,probability=25),
    NanomitesPass(defaultParseMode=none); # This is very expensive, I set the default to none, change it if you need to

)";
    }

    std::ifstream file("Leet.conf");
    std::string line;
    bool readingPassList = false;
    std::string passList;
    int passListDepth = 0;
    while (std::getline(file, line))
    {
        llvm::StringRef text(line);
        size_t comment = text.find('#');
        if (comment != llvm::StringRef::npos)
            text = text.take_front(comment);
        text = text.trim();
        if (text.empty()) continue;

        if (readingPassList)
        {
            size_t terminator = FindTopLevelSeparator(text, ';', passListDepth);
            if (terminator == llvm::StringRef::npos)
            {
                passList += text.str();
                continue;
            }

            passList += text.take_front(terminator).str();

            ParsePassList(settings, passList);
            passList.clear();
            passListDepth = 0;
            readingPassList = false;
            continue;
        }

        auto assignment = text.split('=');
        if (assignment.second.empty() && assignment.first != "passes") continue;
        llvm::StringRef key = assignment.first.trim();
        llvm::StringRef value = assignment.second.trim();

        if (key == "passes")
        {
            if (value.empty())
            {
                readingPassList = true;
                passList.clear();
                passListDepth = 0;
                continue;
            }
            ParsePassList(settings, value);
        }
        else
        {
            // Accept any parameter as a global setting
            SetArgument(settings.parameters, key, ParseValues(value));
        }
    }
    if (readingPassList)
        llvm::errs() << "LeetObfuscator: unterminated multi-line passes list; ignoring it\n";
    return settings;
}