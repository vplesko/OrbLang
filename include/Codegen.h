#pragma once

#include <stack>
#include <optional>
#include "SymbolTable.h"
#include "AST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class Codegen {
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
        UntypedVal untyVal = { .type = UntypedVal::T_NONE };

        bool isUntyVal() const { return untyVal.type != UntypedVal::T_NONE; }
        void resetUntyVal() { untyVal.type = UntypedVal::T_NONE; }
    };

    bool isBool(const ExprGenPayload &e) {
        return e.isUntyVal() ? e.untyVal.type == UntypedVal::T_BOOL : getTypeTable()->isTypeB(e.type);
    }

    TypeTable* getTypeTable() { return symbolTable->getTypeTable(); }

    llvm::Value* getConstB(bool val);

    llvm::AllocaInst* createAlloca(llvm::Type *type, const std::string &name);
    llvm::GlobalValue* createGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name);
    llvm::Constant* createString(const std::string &str);
    
    std::optional<NamePool::Id> mangleName(const FuncValue &f);

    llvm::Type* getType(TypeTable::Id typeId);
    void createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId);
    void createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    void createCast(ExprGenPayload &e, TypeTable::Id t);

    bool isGlobalScope() const;
    bool isBlockTerminated() const;

    // panics if both val and unty val are invalid, returns panic
    bool valueBroken(const ExprGenPayload &e);
    // panics if val is invalid, returns panic
    bool valBroken(const ExprGenPayload &e);
    // panics if ref is invalid, returns panic
    bool refBroken(const ExprGenPayload &e);

    bool promoteUntyped(ExprGenPayload &e, TypeTable::Id t);

    ExprGenPayload codegen(const UntypedExprAst *ast);
    ExprGenPayload codegen(const VarExprAst *ast);
    ExprGenPayload codegen(const IndExprAst *ast);
    ExprGenPayload codegen(const UnExprAst *ast);
    ExprGenPayload codegenUntypedUn(Token::Oper op, UntypedVal unty);
    ExprGenPayload codegen(const BinExprAst *ast);
    // helper function for short-circuit evaluation of boolean AND and OR
    ExprGenPayload codegenLogicAndOr(const BinExprAst *ast);
    ExprGenPayload codegenLogicAndOrGlobalScope(const BinExprAst *ast);
    ExprGenPayload codegenUntypedBin(Token::Oper op, UntypedVal untyL, UntypedVal untyR);
    ExprGenPayload codegen(const TernCondExprAst *ast);
    ExprGenPayload codegenGlobalScope(const TernCondExprAst *ast);
    ExprGenPayload codegen(const CallExprAst *ast);
    ExprGenPayload codegen(const CastExprAst *ast);
    ExprGenPayload codegen(const ArrayExprAst *ast);
    void codegen(const DeclAst *ast);
    void codegen(const IfAst *ast);
    void codegen(const ForAst *ast);
    void codegen(const WhileAst *ast);
    void codegen(const DoWhileAst *ast);
    void codegen(const BreakAst *ast);
    void codegen(const ContinueAst *ast);
    void codegen(const SwitchAst *ast);
    void codegen(const RetAst *ast);
    void codegen(const BlockAst *ast, bool makeScope);
    std::pair<FuncValue, bool> codegen(const FuncProtoAst *ast, bool definition);
    void codegen(const FuncAst *ast);

    llvm::Type* codegenType(const TypeAst *ast);
    ExprGenPayload codegenExpr(const ExprAst *ast);

public:
    Codegen(NamePool *namePool, SymbolTable *symbolTable);

    llvm::Type* genPrimTypeBool();
    llvm::Type* genPrimTypeI(unsigned bits);
    llvm::Type* genPrimTypeU(unsigned bits);
    llvm::Type* genPrimTypeC(unsigned bits);
    llvm::Type* genPrimTypeF16();
    llvm::Type* genPrimTypeF32();
    llvm::Type* genPrimTypeF64();
    llvm::Type* genPrimTypePtr();

    void codegenNode(const BaseAst *ast, bool blockMakeScope = true);

    bool isPanic() const { return panic; }

    void printout() const;

    bool binary(const std::string &filename);
};

// panics if pointer x is null; returns panic
template<typename T>
bool Codegen::broken(const T &x) {
    if (x == nullptr) panic = true;
    return panic;
}