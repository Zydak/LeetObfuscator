#include "SettingsParser.h"

#include <fstream>
#include <iostream>
#include <filesystem>

LeetObfuscator::SettingsParser::FunctionAttributes LeetObfuscator::SettingsParser::ParseFunctionAttributes(llvm::Function& function)
{
    FunctionAttributes functionSettings;
    GlobalAttributes globalSettings = ParseGlobalAttributes();

    functionSettings.skip = globalSettings.parseMode == GlobalParseMode::None; // Default to skipping if global parse mode is None
    functionSettings.maxBlockSize = globalSettings.maxBlockSize; // Default to global max block
    functionSettings.mbaExpansionCount = -1; // This will default to whatever was in the global settings for given MBA pass

    if (function.hasFnAttribute("leet.skip"))
    {
        functionSettings.skip = true;
    }
    else if (function.hasFnAttribute("leet.parse"))
    {
        functionSettings.skip = false;
    }

    if (function.hasFnAttribute("leet.maxBlockSize"))
    {
        auto attribute = function.getFnAttribute("leet.maxBlockSize");
        attribute.getValueAsString().getAsInteger(10, functionSettings.maxBlockSize);
    }

    if (function.hasFnAttribute("leet.MBAexpansionCount"))
    {
        auto attribute = function.getFnAttribute("leet.MBAexpansionCount");
        attribute.getValueAsString().getAsInteger(10, functionSettings.mbaExpansionCount);
    }

    return functionSettings;
}

LeetObfuscator::SettingsParser::Pass LeetObfuscator::SettingsParser::ParsePassString(const std::string &passStr)
{
    if (passStr.find("MBAPass") != std::string::npos)
    {
        size_t openParen = passStr.find('(');
        size_t closeParen = passStr.find(')');
        if (openParen != std::string::npos && closeParen != std::string::npos)
        {
            uint32_t expansionCount = std::stoi(passStr.substr(openParen + 1, closeParen - openParen - 1));
            return {PassType::MBAPass, expansionCount};
        }
    }
    else if (passStr == "StringEncryptionPass")
    {
        return {PassType::StringEncryptionPass, 0};
    }
    else if (passStr == "BlockSplitterPass")
    {
        return {PassType::BlockSplitterPass, 0};
    }
    else if (passStr == "DispatcherPass")
    {
        return {PassType::DispatcherPass, 0};
    }

    // Default return value if no match is found
    return {PassType::StringEncryptionPass, 0};
}

LeetObfuscator::SettingsParser::GlobalAttributes LeetObfuscator::SettingsParser::ParseGlobalAttributes()
{
    if (m_GlobalSettings)
    {
        return *m_GlobalSettings;
    }

    m_GlobalSettings = std::make_unique<GlobalAttributes>();
    GlobalAttributes& settings = *m_GlobalSettings;
    
    // Get all default attributes
    if (std::filesystem::exists("Leet.conf") == false)
    {
        llvm::errs() << "The settings file doesn't exist, Would you like to create one? (y/n): ";
        char response;
        std::cin >> response;
        if (response == 'y' || response == 'Y')
        {
            std::ofstream newSettingsFile("Leet.conf");

            // Write default settings to the file
            newSettingsFile << "# Leet Obfuscator Settings\n";
            newSettingsFile << "# This file contains default settings for the Leet Obfuscator.\n";
            newSettingsFile << "# You can modify these settings to customize the obfuscation process.\n\n";
            newSettingsFile << "# Default maximum block size for splitting, everything above this value will get split into smaller blocks\n";
            newSettingsFile << "maxBlockSize=20\n";
            newSettingsFile << "# Which functions to parse by default (all/none)\n";
            newSettingsFile << "# If (all) is selected, everything except for functions with leet.skip annotation will be parsed\n";
            newSettingsFile << "# If (none) is selected, only functions with leet.parse annotation will be parsed\n";
            newSettingsFile << "parseFunctions=all\n\n";
            newSettingsFile << "# Passes to run\n";
            newSettingsFile << "# The passes will be run in the order they are listed here\n";
            newSettingsFile << "# Some passes have additional options like MBAPass\n";
            newSettingsFile << "# Available passes:\n";
            newSettingsFile << "# - StringEncryptionPass\n";
            newSettingsFile << "# - MBAPass(x) - x being the amount of expansions that will happen for each operation\n";
            newSettingsFile << "# - BlockSplitterPass\n";
            newSettingsFile << "# - DispatcherPass\n";
            newSettingsFile << "passes=StringEncryptionPass,MBAPass(2),BlockSplitterPass,DispatcherPass,MBAPass(1)\n";
            newSettingsFile.close();
        }
        else
        {
            // Can't continue without default settings
            llvm::errs() << "No default settings file, exiting.\n";
            exit(1);
        }
    }
    std::ifstream settingsFile("Leet.conf");

    // Parse the settings file
    std::string line;
    while (std::getline(settingsFile, line))
    {
        // Ignore comments and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        auto delimiterPos = line.find('=');
        if (delimiterPos == std::string::npos)
            continue;

        // Also handle '#' in the middle of the line
        auto commentPos = line.find('#', delimiterPos);
        if (commentPos != std::string::npos)
        {
            line = line.substr(0, commentPos);
        }

        std::string key = line.substr(0, delimiterPos);
        std::string value = line.substr(delimiterPos + 1);

        if (key == "maxBlockSize")
        {
            settings.maxBlockSize = std::stoi(value);
        }
        else if (key == "parseFunctions")
        {
            if (value == "all")
                settings.parseMode = GlobalParseMode::All;
            else if (value == "none")
                settings.parseMode = GlobalParseMode::None;
        }
        else if (key == "passes")
        {
            size_t start = 0;
            size_t end = value.find(',');
            while (end != std::string::npos)
            {
                std::string passStr = value.substr(start, end - start);
                settings.passes.push_back(ParsePassString(passStr));
                start = end + 1;
                end = value.find(',', start);
            }
            std::string passStr = value.substr(start);
            settings.passes.push_back(ParsePassString(passStr));
        }
    }

    return settings;
}
