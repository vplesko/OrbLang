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

    llvm::Value* codegen(const LiteralExprAST *ast);
    llvm::Value* codegen(const VarExprAST *ast);
    llvm::Value* codegen(const BinExprAST *ast);
    llvm::Value* codegen(const CallExprAST *ast);
    void codegen(const DeclAST *ast);
    void codegen(const IfAST *ast);
    void codegen(const RetAST *ast);
    void codegen(const BlockAST *ast, bool makeScope);
    llvm::Function* codegen(const FuncProtoAST *ast, bool definition);
    llvm::Function* codegen(const FuncAST *ast);

public:
    CodeGen(const NamePool *namePool, SymbolTable *symbolTable);

    llvm::Value* codegenNode(const BaseAST *ast);

    bool isPanic() const { return panic; }

    void printout() const;
};