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

    template<typename T> bool broken(const T &x);

    llvm::Value* getConstB(bool val);
    llvm::AllocaInst* createAlloca(llvm::Type *type, const std::string &name);
    llvm::GlobalValue* createGlobal(llvm::Type *type, const std::string &name);
    void createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId);
    void createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);

    bool isBlockTerminated() const;

    struct ExprGenPayload {
        TypeTable::Id type;
        llvm::Value *val = nullptr;
        llvm::Value *ref = nullptr;
    };

    ExprGenPayload codegen(const LiteralExprAST *ast);
    ExprGenPayload codegen(const VarExprAST *ast);
    ExprGenPayload codegen(const UnExprAST *ast);
    ExprGenPayload codegen(const BinExprAST *ast);
    ExprGenPayload codegen(const CallExprAST *ast);
    ExprGenPayload codegen(const CastExprAST *ast);
    void codegen(const DeclAST *ast);
    void codegen(const IfAST *ast);
    void codegen(const ForAST *ast);
    void codegen(const WhileAST *ast);
    void codegen(const DoWhileAST *ast);
    void codegen(const RetAST *ast);
    void codegen(const BlockAST *ast, bool makeScope);
    FuncValue* codegen(const FuncProtoAST *ast, bool definition);
    void codegen(const FuncAST *ast);

    llvm::Type* codegenType(const TypeAST *ast);
    ExprGenPayload codegenExpr(const ExprAST *ast);

public:
    CodeGen(NamePool *namePool, SymbolTable *symbolTable);

    llvm::Type* genPrimTypeBool();
    llvm::Type* genPrimTypeI(unsigned bits);
    llvm::Type* genPrimTypeU(unsigned bits);
    llvm::Type* genPrimTypeF16();
    llvm::Type* genPrimTypeF32();
    llvm::Type* genPrimTypeF64();

    void codegenNode(const BaseAST *ast, bool blockMakeScope = true);

    bool isPanic() const { return panic; }

    void printout() const;
};

// panics if pointer x is null; returns panic
template<typename T>
bool CodeGen::broken(const T &x) {
    if (x == nullptr) panic = true;
    return panic;
}