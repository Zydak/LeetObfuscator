#include "AnnotationPass.h"

#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"

llvm::PreservedAnalyses LeetObfuscator::AnnotationPass::run(llvm::Module &module, llvm::ModuleAnalysisManager&)
{
    llvm::GlobalVariable* globalVar = module.getGlobalVariable("llvm.global.annotations");
    if (!globalVar)
        return llvm::PreservedAnalyses::all();

    llvm::ConstantArray* annotations = llvm::dyn_cast<llvm::ConstantArray>(globalVar->getInitializer());
    if (!annotations)
        return llvm::PreservedAnalyses::all();

    for (size_t i = 0; i < annotations->getNumOperands(); i++)
    {
        llvm::ConstantStruct* entry = llvm::dyn_cast<llvm::ConstantStruct>(annotations->getOperand(i));
        if (!entry)
            continue;

        llvm::Function* function = llvm::dyn_cast<llvm::Function>(entry->getOperand(0)->stripPointerCasts());
        if (!function)
            continue;

        llvm::GlobalVariable* annotationGlobalVar = llvm::dyn_cast<llvm::GlobalVariable>(entry->getOperand(1)->stripPointerCasts());
        if (!annotationGlobalVar)
            continue;

        llvm::ConstantDataArray* annotationData = llvm::dyn_cast<llvm::ConstantDataArray>(annotationGlobalVar->getInitializer());
        if (!annotationData)
            continue;

        llvm::StringRef annotationStr = annotationData->getAsCString();
        if (!annotationStr.starts_with("leet."))
            continue;

        auto split = annotationStr.split('=');

        llvm::StringRef key = split.first;
        llvm::StringRef value = split.second;

        if (value.empty())
        {
            function->addFnAttr(key.str());
        }
        else
        {
            function->addFnAttr(key.str(), value);
        }
    }

    // TODO: something better, this will erase all annotations even if they are unrelated to leet
    globalVar->eraseFromParent();

    return llvm::PreservedAnalyses::none();
}