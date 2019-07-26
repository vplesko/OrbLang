#pragma once

#include "SymbolTable.h"
#include "AST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class CodeGen {
    const NamePool *namePool;
    SymbolTable *symbolTable;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;

    llvm::BasicBlock *funcBody;

    bool panic;

    llvm::Function *main; // TODO tmp, remove

public:
    CodeGen(const NamePool *namePool, SymbolTable *symbolTable);

    void codegenStart(); // TODO tmp, remove
    llvm::AllocaInst* createAlloca(const std::string &name);
    llvm::Value* codegen(const BaseAST *ast);
    llvm::Value* codegen(const VarExprAST *ast);
    llvm::Value* codegen(const BinExprAST *ast);
    llvm::Value* codegen(const DeclAST *ast);
    void codegenEnd(); // TODO tmp, remove

    bool isPanic() const { return panic; }

    void printout() const;
};