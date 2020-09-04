#pragma once

#include <string>
#include "Processor.h"
#include "Evaluator.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

class Codegen : public Processor {
    Evaluator *evaluator;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;
    std::unique_ptr<llvm::PassManagerBuilder> llvmPmb;
    std::unique_ptr<llvm::legacy::FunctionPassManager> llvmFpm;

    bool isLlvmBlockTerminated() const;
    llvm::Constant* getLlvmConstB(bool val);
    // generates a constant for a string literal
    llvm::Constant* getLlvmConstString(const std::string &str);
    llvm::Type* getLlvmType(TypeTable::Id typeId);
    llvm::AllocaInst* makeLlvmAlloca(llvm::Type *type, const std::string &name);

    std::string getNameForLlvm(NamePool::Id name) const;

    NodeVal promoteKnownVal(const NodeVal &node);

    // TODO!    
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performCast(const NodeVal &node, TypeTable::Id ty);
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args);
    bool performFunctionDeclaration(FuncValue &func);
    bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func);
    NodeVal performEvaluation(const NodeVal &node);

public:
    Codegen(Evaluator *evaluator, NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);

    llvm::Type* genPrimTypeBool();
    llvm::Type* genPrimTypeI(unsigned bits);
    llvm::Type* genPrimTypeU(unsigned bits);
    llvm::Type* genPrimTypeC(unsigned bits);
    llvm::Type* genPrimTypeF32();
    llvm::Type* genPrimTypeF64();
    llvm::Type* genPrimTypePtr();

    void printout() const;
    bool binary(const std::string &filename);
};