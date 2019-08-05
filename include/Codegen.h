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

    bool panic;

    llvm::AllocaInst* createAlloca(const std::string &name);
    llvm::GlobalValue* createGlobal(const std::string &name);

    // TODO some of these always ret null, but declare otherwise
    llvm::Value* codegen(const VarExprAST *ast);
    llvm::Value* codegen(const BinExprAST *ast);
    llvm::Value* codegen(const DeclAST *ast);
    llvm::Value* codegen(const RetAST *ast);
    llvm::Value* codegen(const BlockAST *ast, bool makeScope);
    llvm::Function* codegen(const FuncProtoAST *ast);
    llvm::Function* codegen(const FuncAST *ast);

public:
    CodeGen(const NamePool *namePool, SymbolTable *symbolTable);

    llvm::Value* codegen(const BaseAST *ast);

    bool isPanic() const { return panic; }

    void printout() const;
};