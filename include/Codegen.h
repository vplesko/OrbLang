#pragma once

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

    llvm::Constant* getConstB(bool val);
    // generates a constant for a string literal
    llvm::Constant* getConstString(const std::string &str);

    llvm::Type* getLlvmType(TypeTable::Id typeId);

    NodeVal promoteKnownVal(const NodeVal &node);

    // TODO!    
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id) { return NodeVal(); }
    // TODO don't forget str to char arr
    NodeVal performCast(const NodeVal &node, TypeTable::Id ty);
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) { return NodeVal(); }
    bool performFunctionMake(const NodeVal &node, FuncValue &func) { return false; }
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