#pragma once

#include "SymbolTable.h"
#include "AST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class CodeGen {
    NamePool *namePool;
    SymbolTable *symbolTable;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;

    bool panic;

    llvm::AllocaInst* createAlloca(llvm::Type *type, const std::string &name);
    llvm::GlobalValue* createGlobal(llvm::Type *type, const std::string &name);

    bool isBlockTerminated() const;

    llvm::Value* codegen(const LiteralExprAST *ast);
    llvm::Value* codegen(const VarExprAST *ast);
    llvm::Value* codegen(const BinExprAST *ast);
    llvm::Value* codegen(const CallExprAST *ast);
    llvm::Type* codegen(const TypeAST *ast);
    void codegen(const DeclAST *ast);
    void codegen(const IfAST *ast);
    void codegen(const ForAST *ast);
    void codegen(const WhileAST *ast);
    void codegen(const DoWhileAST *ast);
    void codegen(const RetAST *ast);
    void codegen(const BlockAST *ast, bool makeScope);
    llvm::Function* codegen(const FuncProtoAST *ast, bool definition);
    llvm::Function* codegen(const FuncAST *ast);

    // TODO literals of other types
    TypeId i64Type;
    llvm::Type* getI64Type();

public:
    CodeGen(NamePool *namePool, SymbolTable *symbolTable);

    void genPrimitiveTypes();

    llvm::Value* codegenNode(const BaseAST *ast, bool blockMakeScope = true);

    bool isPanic() const { return panic; }

    void printout() const;
};