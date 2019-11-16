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

    typedef std::pair<TypeTable::Id, llvm::Value*> ExprGenPayload;

    ExprGenPayload codegen(const LiteralExprAST *ast);
    ExprGenPayload codegen(const VarExprAST *ast);
    ExprGenPayload codegen(const BinExprAST *ast);
    ExprGenPayload codegen(const CallExprAST *ast);
    void codegen(const DeclAST *ast);
    void codegen(const IfAST *ast);
    void codegen(const ForAST *ast);
    void codegen(const WhileAST *ast);
    void codegen(const DoWhileAST *ast);
    void codegen(const RetAST *ast);
    void codegen(const BlockAST *ast, bool makeScope);
    llvm::Function* codegen(const FuncProtoAST *ast, bool definition);
    void codegen(const FuncAST *ast);

    llvm::Type* codegenType(const TypeAST *ast);
    ExprGenPayload codegenExpr(const ExprAST *ast);

public:
    CodeGen(NamePool *namePool, SymbolTable *symbolTable);

    llvm::Type* genPrimTypeInt(unsigned bits);

    void codegenNode(const BaseAST *ast, bool blockMakeScope = true);

    bool isPanic() const { return panic; }

    void printout() const;
};