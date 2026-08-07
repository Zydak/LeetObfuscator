#pragma once
#include "llvm/Passes/PassBuilder.h"
#include "SettingsParser.h"
#include "Logger.h"
#include <vector>

#include "llvm/CodeGen/MachineFunctionPass.h"

namespace LeetObfuscator
{
    class NanomitesPass : public llvm::PassInfoMixin<NanomitesPass>
    {
    public:
        explicit NanomitesPass(SettingsParser::PassArguments arguments)
            : m_Arguments(std::move(arguments)), m_Logger("NanomitesPass") {}

        llvm::PreservedAnalyses run(llvm::Module& module, llvm::ModuleAnalysisManager& mam);

    private:
        void ObfuscateFunction(llvm::Function* function, std::vector<llvm::Constant*>& nanomitesEntries);
        void CreateGlobalNanomitesTable(llvm::Module& module, std::vector<llvm::Constant*>& nanomitesEntries);

        uint32_t GenerateUniqueNanomiteId(RandomNumberGenerator& generator);
        llvm::Function* CreateForwardFunction(llvm::Module& module, llvm::Function* realFunc, uint32_t id);

        SettingsParser::PassArguments m_Arguments;
        Logger m_Logger;
    };

    class NanomitesMachineCodePass : public llvm::MachineFunctionPass
    {
    public:
        static char ID;

        NanomitesMachineCodePass() : llvm::MachineFunctionPass(ID) {}

        bool runOnMachineFunction(llvm::MachineFunction &MF) override;

        llvm::StringRef getPassName() const override {
            return "Leet Nanomites Machine Code Pass";
        }

    private:
        bool ParseLeetID(llvm::StringRef name, uint32_t& id);
        void InsertTrap(uint32_t id, llvm::MachineInstr& machineInstruction);
    };
}