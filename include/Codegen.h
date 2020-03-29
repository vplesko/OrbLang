#pragma once

#include <stack>
#include <optional>
#include "SymbolTable.h"
#include "AST.h"
#include "CompileMessages.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class Codegen {
    NamePool *namePool;
    SymbolTable *symbolTable;
    CompileMessages *msgs;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;

    std::stack<llvm::BasicBlock*> breakStack, continueStack;

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
    llvm::GlobalValue* defineGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name);
    llvm::GlobalValue* declareGlobal(llvm::Type *type, bool isConstant, const std::string &name);
    llvm::Constant* createString(const std::string &str);
    
    std::optional<NamePool::Id> mangleName(const FuncValue &f);

    llvm::Type* getType(TypeTable::Id typeId);
    bool createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId);
    bool createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    bool createCast(ExprGenPayload &e, TypeTable::Id t);

    bool isGlobalScope() const;
    bool isBlockTerminated() const;

    // checks if both val and unty val are invalid
    bool valueBroken(const ExprGenPayload &e);
    // checks if val is invalid
    bool valBroken(const ExprGenPayload &e);
    // checks if ref is invalid
    bool refBroken(const ExprGenPayload &e);

    bool promoteUntyped(ExprGenPayload &e, TypeTable::Id t);

    ExprGenPayload codegen(const UntypedExprAst *ast);
    ExprGenPayload codegen(const VarExprAst *ast);
    ExprGenPayload codegen(const IndExprAst *ast);
    ExprGenPayload codegen(const UnExprAst *ast);
    ExprGenPayload codegenUntypedUn(CodeLoc codeLoc, Token::Oper op, UntypedVal unty);
    ExprGenPayload codegen(const BinExprAst *ast);
    // helper function for short-circuit evaluation of boolean AND and OR
    ExprGenPayload codegenLogicAndOr(const BinExprAst *ast);
    ExprGenPayload codegenLogicAndOrGlobalScope(const BinExprAst *ast);
    ExprGenPayload codegenUntypedBin(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR);
    ExprGenPayload codegen(const TernCondExprAst *ast);
    ExprGenPayload codegenGlobalScope(const TernCondExprAst *ast);
    ExprGenPayload codegen(const CallExprAst *ast);
    ExprGenPayload codegen(const CastExprAst *ast);
    ExprGenPayload codegen(const ArrayExprAst *ast);
    void codegen(const DeclAst *ast, bool scanning = false);
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
    Codegen(NamePool *namePool, SymbolTable *symbolTable, CompileMessages *msgs);

    llvm::Type* genPrimTypeBool();
    llvm::Type* genPrimTypeI(unsigned bits);
    llvm::Type* genPrimTypeU(unsigned bits);
    llvm::Type* genPrimTypeC(unsigned bits);
    llvm::Type* genPrimTypeF16();
    llvm::Type* genPrimTypeF32();
    llvm::Type* genPrimTypeF64();
    llvm::Type* genPrimTypePtr();

    void codegenNode(const BaseAst *ast, bool blockMakeScope = true);
    void scanNode(const BaseAst *ast);

    void printout() const;

    bool binary(const std::string &filename);
};