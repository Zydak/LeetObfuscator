#pragma once

#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"

#include <fstream>
#include <string>

namespace LeetObfuscator
{
    class Logger
    {
    public:
        explicit Logger(std::string passName);

        void Log(const std::string& message, uint32_t indent = 0);
        void LogModule(const llvm::Module& module, const std::string& message, uint32_t indent = 0);
        void LogFunction(const llvm::Function& function, const std::string& message, uint32_t indent = 0);
        void LogInstruction(const llvm::Instruction& instruction, const std::string& message, uint32_t indent = 0);
        void LogValue(const llvm::Value& value, const std::string& message, uint32_t indent = 0);

    private:
        void AppendLine(const std::string& line);
        void InitializeFilePath(const llvm::Module* module = nullptr);

        std::string m_PassName;
        std::string m_FilePath;
        bool m_IsInitialized;

        static std::string GetLogDirectory();
        static std::string SanitizeName(const std::string& input);
    };
}
