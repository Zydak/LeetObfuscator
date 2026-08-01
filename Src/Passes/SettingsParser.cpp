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

using FunctionAttributes = SettingsParser::FunctionAttributes;
using PassArguments = SettingsParser::PassArguments;

static void SetArgument(PassArguments& arguments, llvm::StringRef key, std::vector<std::string> values)
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

static const std::vector<std::string>* FindArgument(const PassArguments& arguments, llvm::StringRef key)
{
    auto it = std::find_if(arguments.begin(), arguments.end(), [key](const auto& argument)
    {
        return argument.first == key;
    });
    return it == arguments.end() ? nullptr : &it->second;
}

static std::vector<std::string> ParseValues(llvm::StringRef value)
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

static std::vector<std::string> GetFunctionOption(llvm::Function& function, llvm::StringRef key)
{
    if (!function.hasFnAttribute(key))
        return {};
    return ParseValues(function.getFnAttribute(key).getValueAsString());
}

static void ReportInvalidArgument(llvm::Function& function, llvm::StringRef key, llvm::StringRef reason)
{
    llvm::errs()
        << "LeetObfuscator: invalid '" << key << "' for function '"
        << function.getName() << "': " << reason << "; using the default value\n";
}

template <typename T>
static bool ParseUnsignedArgument(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef key, T& output, T maximum)
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

static void ParseStringList(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef key, std::vector<std::string>& output)
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
static bool ParseEnumArgument(
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

static uint64_t GenerateRuntimeSeed()
{
    static std::random_device randomDevice;
    static std::mt19937_64 generator((uint64_t(randomDevice()) << 32) ^ randomDevice());
    static uint64_t seed = generator();
    return seed;
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

using OptionApplier = std::function<void(llvm::Function&, const std::vector<std::string>*, llvm::StringRef, FunctionAttributes&)>;

struct Option
{
    llvm::StringRef name;
    OptionApplier apply;
};

template <typename T>
static OptionApplier UnsignedOption(T FunctionAttributes::* field, T maximum = std::numeric_limits<T>::max())
{
    return [field, maximum](llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
    {
        ParseUnsignedArgument<T>(function, values, name, result.*field, maximum);
    };
}

static OptionApplier StringListOption(std::vector<std::string> FunctionAttributes::* field)
{
    return [field](llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
    {
        ParseStringList(function, values, name, result.*field);
    };
}

template <typename T>
static OptionApplier EnumOption(T FunctionAttributes::* field, std::vector<std::pair<llvm::StringRef, T>> namedValues, llvm::StringRef expected)
{
    return [field, namedValues = std::move(namedValues), expected](llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
    {
        ParseEnumArgument<T>(function, values, name, result.*field, namedValues, expected);
    };
}

static const std::vector<std::pair<llvm::StringRef, SettingsParser::GlobalParseMode>> kParseModeValues = {
    {"all", SettingsParser::GlobalParseMode::All},
    {"none", SettingsParser::GlobalParseMode::None},
};

static void ApplyRuntimeSeed(llvm::Function& function, const std::vector<std::string>* values, llvm::StringRef name, FunctionAttributes& result)
{
    if (!values)
        return;
    if (values->size() != 1)
    {
        ReportInvalidArgument(function, name, "expected 'auto' or one unsigned integer");
        return;
    }
    if (values->front() == "auto")
    {
        static uint64_t seed = GenerateRuntimeSeed();
        result.runtimeSeed = seed;
        return;
    }
    ParseUnsignedArgument<uint64_t>(function, values, name, result.runtimeSeed, std::numeric_limits<uint64_t>::max());
}

static void ApplySkip(llvm::Function&, const std::vector<std::string>* values, llvm::StringRef, FunctionAttributes& result)
{
    if (!values)
        return;

    result.skip = true;
}

static void ApplyForcePass(llvm::Function&, const std::vector<std::string>* values, llvm::StringRef, FunctionAttributes& result)
{
    if (!values)
        return;

    result.skip = false;
}

// Settings specific to one pass, namespaced as "leet.<PassName>.<name>".
static const std::vector<Option>& GetPassOptions(SettingsParser::PassType passType)
{
    using FA = FunctionAttributes;

    static const std::vector<Option> stringEncryptionOptions = {
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
    };
    static const std::vector<Option> mbaOptions = {
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
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"maxBlockSize", UnsignedOption(&FunctionAttributes::maxBlockSize)},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"maxBlockSize", UnsignedOption(&FA::maxBlockSize)},
        {"minBlockSize", UnsignedOption(&FA::minBlockSize)},
        {"probability", UnsignedOption(&FA::blockSplitterProbability, 100u)},
        {"blockSplitSize", UnsignedOption(&FA::blockSplitSize)},
    };
    static const std::vector<Option> dispatcherOptions = {
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::dispatcherProbability, 100u)},
    };
    static const std::vector<Option> antiAnalysisOptions = {
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
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"minFunctionSize", UnsignedOption(&FunctionAttributes::minFunctionSize)},
        {"maxFunctionSize", UnsignedOption(&FunctionAttributes::maxFunctionSize)},
        {"probability", UnsignedOption(&FA::antiAliasingProbability, 100u)},
    };
    static const std::vector<Option> nanomitesOptions = {
        {"skip", ApplySkip},
        {"forcePass", ApplyForcePass},
        {"runtimeSeed", ApplyRuntimeSeed},
        {"probability", UnsignedOption(&FA::antiAliasingProbability, 100u)},
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

// for each Option in `options`, let a
// "<attributePrefix>.<name>" function attribute, if present, override the
// value already in `effective`. Doesn't touch `result` - overriding and
// extracting are kept as separate steps.
static void OverlayFunctionAttributes(llvm::Function& function, llvm::StringRef attributePrefix, const std::vector<Option>& options, PassArguments& effective)
{
    for (const Option& option : options)
    {
        std::string attributeKey = attributePrefix.str() + "." + option.name.str();
        if (function.hasFnAttribute(attributeKey))
            SetArgument(effective, option.name, GetFunctionOption(function, attributeKey));
    }
}

// parses the fully-resolved `effective`
// arguments into `result`, one Option at a time.
static void ExtractOptions(llvm::Function& function, const std::vector<Option>& options, const PassArguments& effective, FunctionAttributes& result)
{
    for (const Option& option : options)
    {
        option.apply(function, FindArgument(effective, option.name), option.name, result);
    }
}

// A semicolon or comma inside (...) belongs to a pass's parameter list.
// Finds the next occurrence of `separator` at paren depth 0, tracking depth
// by reference so a value split across multiple config lines can resume
// scanning where the previous line left off.
static size_t FindTopLevelSeparator(llvm::StringRef text, char separator, int& depth)
{
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '(') ++depth;
        else if (text[i] == ')' && depth > 0) --depth;
        else if (text[i] == separator && depth == 0) return i;
    }
    return llvm::StringRef::npos;
}

static void ParsePassList(SettingsParser::GlobalAttributes& settings, llvm::StringRef value)
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
        default: return "";
    }
}

LeetObfuscator::SettingsParser::FunctionAttributes
LeetObfuscator::SettingsParser::ParseFunctionAttributes(llvm::Function& function, PassType passType, const PassArguments& passArguments)
{
    FunctionAttributes result;
    static GlobalAttributes global = ParseGlobalAttributes();
    result.skip = global.defaultParseMode == GlobalParseMode::None;

    llvm::StringRef passName = GetPassTypeName(passType);
    const std::vector<Option>& passOptions = GetPassOptions(passType);

    // Merge global defaults with this pass's own arguments, pass arguments take priority
    PassArguments effective = global.parameters;
    for (const auto& argument : passArguments)
        SetArgument(effective, argument.first, argument.second);

    // Let function attributes override the arguments
    OverlayFunctionAttributes(function, "leet." + passName.str(), passOptions, effective);

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
        attributes.skip
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
        std::cout << "UNIQUE SEED" << std::endl;
    }

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
        file << "# Leet Obfuscator Settings\n"
            << "#(all/none)\n"
            << "# all will mark all functions for parsing automatically and omit only functions with skip annotation\n"
            << "# none will parse only functions that are explicitly marked with forcePass annotation and skip everything else\n"
            << "defaultParseMode=all\n"
            << "# seed for randomness, keep auto unless debugging\n"
            << "runtimeSeed=auto\n"
            << "# minimum instruction count of a function for it to qualify for obfuscation (0 = all qualify)\n"
            << "minFunctionSize=10\n"
            << "# maximum instruction count of a function for it to qualify for obfuscation (0 = all qualify)\n"
            << "maxFunctionSize=0\n"
            << "# minimum instruction count of a block for it to qualify for obfuscation (0 = all qualify)\n"
            << "minBlockSize=5\n"
            << "# maximum instruction count of a block for it to qualify for obfuscation (0 = all qualify)\n"
            << "maxBlockSize=0\n\n"
            << "# Passes available right now:\n"
            << "# \t- StringEncryptionPass\n"
            << "# \t- MBAPass\n"
            << "# \t- BlockSplitterPass\n"
            << "# \t- DispatcherPass\n"
            << "# \t- AntiAnalysisPass\n"
            << "# \t- AntiAliasingPass\n"
            << "# \t- NanomitesPass\n"
            << "# \t- AAMBAPass\n"
            << "# Each pass needs to be on a separate line. Separate pass parameters with ',' and multi-values with '|'.\n"
            << "passes=\n"
            << "    StringEncryptionPass(),\n"
            << "    MBAPass(expansionCount=2),\n"
            << "    BlockSplitterPass(maxBlockSize=20),\n"
            << "    AntiAnalysisPass(),\n"
            << "    DispatcherPass(),\n"
            << "    MBAPass(expansionCount=1),\n"
            << "    AAMBAPass(),\n"
            << "    AntiAliasingPass(),\n"
            << "    AntiAnalysisPass(),\n"
            << "    NanomitesPass();\n";
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

        if (key == "defaultParseMode" || key == "parseFunctions")
        {
            if (value == "all") settings.defaultParseMode = GlobalParseMode::All;
            else if (value == "none") settings.defaultParseMode = GlobalParseMode::None;
            SetArgument(settings.parameters, "defaultParseMode", {value.str()});
        }
        if (key == "runtimeSeed")
        {
            if (value == "auto")
                settings.defaultRuntimeSeed = GenerateRuntimeSeed();
            else
                settings.defaultRuntimeSeed = std::stoull(value.str());
            SetArgument(settings.parameters, key, {value.str()});
        }
        if (key == "stringEncryptionProbability")
        {
            settings.stringEncryptionProbability = (uint32_t)std::stoi(value.str());
            SetArgument(settings.parameters, key, {value.str()});
        }
        else if (key == "runtimeSeed" || key == "minFunctionSize" || key == "maxFunctionSize" || key == "maxBlockSize" || key == "minBlockSize")
        {
            SetArgument(settings.parameters, key, ParseValues(value));
        }
        else if (key == "passes")
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
    }
    if (readingPassList)
        llvm::errs() << "LeetObfuscator: unterminated multi-line passes list; ignoring it\n";
    return settings;
}