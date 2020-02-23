#pragma once

#include "SymbolTable.h"
#include "AST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include <stack>

class CodeGen {
    NamePool *namePool;
    SymbolTable *symbolTable;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;

    std::stack<llvm::BasicBlock*> breakStack, continueStack;

    bool panic;

    template<typename T> bool broken(const T &x);

    struct ExprGenPayload {
        TypeTable::Id type;
        llvm::Value *val = nullptr;
        llvm::Value *ref = nullptr;
        LiteralVal litVal = { .type = LiteralVal::T_NONE };

        bool isLitVal() const { return litVal.type != LiteralVal::T_NONE; }
        void resetLitVal() { litVal.type = LiteralVal::T_NONE; }
        bool isBool() const { return isLitVal() ? litVal.type == LiteralVal::T_BOOL : TypeTable::isTypeB(type); }
    };

    llvm::Value* getConstB(bool val);

    llvm::AllocaInst* createAlloca(llvm::Type *type, const std::string &name);
    llvm::GlobalValue* createGlobal(llvm::Type *type, llvm::Constant *init, const std::string &name);

    llvm::Type* getType(TypeTable::Id typeId);
    void createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId);
    void createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    void createCast(ExprGenPayload &e, TypeTable::Id t);

    bool isGlobalScope() const;
    bool isBlockTerminated() const;

    bool valueBroken(const ExprGenPayload &e);
    bool valBroken(const ExprGenPayload &e);
    bool refBroken(const ExprGenPayload &e);

    bool promoteLiteral(ExprGenPayload &e, TypeTable::Id t);

    ExprGenPayload codegen(const LiteralExprAST *ast);
    ExprGenPayload codegen(const VarExprAST *ast);
    ExprGenPayload codegen(const UnExprAST *ast);
    ExprGenPayload codegenLiteralUn(Token::Oper op, LiteralVal lit);
    ExprGenPayload codegen(const BinExprAST *ast);
    // helper function for short-circuit evaluation of boolean AND and OR
    ExprGenPayload codegenLogicAndOr(const BinExprAST *ast);
    ExprGenPayload codegenLogicAndOrGlobalScope(const BinExprAST *ast);
    ExprGenPayload codegenLiteralBin(Token::Oper op, LiteralVal litL, LiteralVal litR);
    ExprGenPayload codegen(const TernCondExprAST *ast);
    ExprGenPayload codegenGlobalScope(const TernCondExprAST *ast);
    ExprGenPayload codegen(const CallExprAST *ast);
    ExprGenPayload codegen(const CastExprAST *ast);
    void codegen(const DeclAST *ast);
    void codegen(const IfAST *ast);
    void codegen(const ForAST *ast);
    void codegen(const WhileAST *ast);
    void codegen(const DoWhileAST *ast);
    void codegen(const BreakAST *ast);
    void codegen(const ContinueAST *ast);
    void codegen(const SwitchAST *ast);
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
    llvm::Type* genPrimTypePtr();

    void codegenNode(const BaseAST *ast, bool blockMakeScope = true);

    bool isPanic() const { return panic; }

    void printout() const;

    bool binary(const std::string &filename);
};

// panics if pointer x is null; returns panic
template<typename T>
bool CodeGen::broken(const T &x) {
    if (x == nullptr) panic = true;
    return panic;
}