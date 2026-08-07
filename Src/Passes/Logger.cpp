#include "Logger.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace LeetObfuscator
{
    Logger::Logger(std::string passName)
        : m_PassName(std::move(passName)), m_IsInitialized(false)
    {
    }

    void Logger::InitializeFilePath(const llvm::Module* module)
    {
        if (m_IsInitialized)
            return;

        std::filesystem::path logDir = std::filesystem::path(GetLogDirectory());
        std::error_code ec;
        std::filesystem::create_directories(logDir, ec);

        std::string moduleName = module ? module->getName().str() : "unknown";
        std::string fileName = SanitizeName(m_PassName) + "_" + SanitizeName(moduleName) + ".log";
        m_FilePath = (logDir / fileName).string();

        std::ofstream stream(m_FilePath, std::ios::trunc);
        if (stream.is_open())
        {
            stream.close();
        }

        m_IsInitialized = true;
    }

    void Logger::AppendLine(const std::string& line)
    {
        std::ofstream stream(m_FilePath, std::ios::app);
        if (!stream.is_open())
            return;

        stream << line << '\n';
        stream.flush();
    }

    void Logger::Log(const std::string& message, uint32_t indent)
    {
        AppendLine(std::string(indent, '\t') + message);
    }

    void Logger::LogModule(const llvm::Module& module, const std::string& message, uint32_t indent)
    {
        InitializeFilePath(&module);

        std::ostringstream stream;
        stream << message << " [module: " << module.getName().str() << "]";
        Log(stream.str(), indent);
    }

    void Logger::LogFunction(const llvm::Function& function, const std::string& message, uint32_t indent)
    {
        std::ostringstream stream;
        stream << message << " [function: " << function.getName().str() << "]";
        Log(stream.str(), indent);
    }

    void Logger::LogInstruction(const llvm::Instruction& instruction, const std::string& message, uint32_t indent)
    {
        std::ostringstream stream;
        stream << message << " [instruction: " << instruction.getOpcodeName() << "]";
        Log(stream.str(), indent);
    }

    void Logger::LogValue(const llvm::Value& value, const std::string& message, uint32_t indent)
    {
        std::ostringstream stream;
        stream << message << " [value: " << value.getName().str() << "]";
        Log(stream.str(), indent);
    }

    std::string Logger::GetLogDirectory()
    {
        return "logs";
    }

    std::string Logger::SanitizeName(const std::string& input)
    {
        std::string sanitized;
        sanitized.reserve(input.size());

        for (char ch : input)
        {
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
            {
                sanitized.push_back(ch);
            }
        }

        return sanitized;
    }
}
