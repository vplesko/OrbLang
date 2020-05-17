#pragma once

#include <stack>
#include <optional>
#include "SymbolTable.h"
#include "AST.h"
#include "Values.h"
#include "CompileMessages.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class Codegen {
    NamePool *namePool;
    StringPool *stringPool;
    SymbolTable *symbolTable;
    CompileMessages *msgs;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;

    std::stack<llvm::BasicBlock*> breakStack, continueStack;

    bool isBool(const ExprGenPayload &e) {
        return e.isUntyVal() ? e.untyVal.kind == UntypedVal::Kind::kBool : getTypeTable()->worksAsTypeB(e.type);
    }

    bool promoteUntyped(ExprGenPayload &e, TypeTable::Id t);

    TypeTable* getTypeTable() { return symbolTable->getTypeTable(); }
    const TypeTable* getTypeTable() const { return symbolTable->getTypeTable(); }

    llvm::Value* getConstB(bool val);

    llvm::AllocaInst* createAlloca(llvm::Type *type, const std::string &name);
    llvm::GlobalValue* createGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name);
    llvm::Constant* createString(const std::string &str);
    
    std::optional<NamePool::Id> mangleName(const FuncValue &f);

    llvm::Type* getType(TypeTable::Id typeId);
    TypeTable::Id getPrimTypeId(TypeTable::PrimIds primId) const { return getTypeTable()->getPrimTypeId(primId); }
    bool createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId);
    bool createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    bool createCast(ExprGenPayload &e, TypeTable::Id t);

    bool isGlobalScope() const;
    bool isBlockTerminated() const;

    bool checkDefinedTypeOrError(TypeTable::Id type, CodeLoc codeLoc);

    typedef std::pair<NamePool::Id, TypeTable::Id> NameTypePair;

    std::optional<Token::Type> getStartingKeyword(const AstNode *ast) const;
    bool checkStartingKeyword(const AstNode *ast, Token::Type t, bool orError);
    bool checkTerminal(const AstNode *ast, bool orError);
    bool checkEmptyTerminal(const AstNode *ast, bool orError);
    bool checkEllipsis(const AstNode *ast, bool orError);
    bool checkNotTerminal(const AstNode *ast, bool orError);
    bool checkBlock(const AstNode *ast, bool orError);
    bool checkExactlyChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkAtLeastChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkAtMostChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkBetweenChildren(const AstNode *ast, std::size_t nLo, std::size_t nHi, bool orError);
    std::optional<NamePool::Id> getId(const AstNode *ast, bool orError);
    std::optional<NameTypePair> getIdTypePair(const AstNode *ast, bool orError);
    std::optional<Token::Type> getKeyword(const AstNode *ast, bool orError);
    std::optional<Token::Oper> getOper(const AstNode *ast, bool orError);
    std::optional<UntypedVal> getUntypedVal(const AstNode *ast, bool orError);
    std::optional<Token::Attr> getAttr(const AstNode *ast, bool orError);

    ExprGenPayload codegenUntypedVal(const AstNode *ast);
    ExprGenPayload codegenVar(const AstNode *ast);
    ExprGenPayload codegenInd(const AstNode *ast);
    ExprGenPayload codegenDot(const AstNode *ast);
    ExprGenPayload codegenUn(const AstNode *ast);
    ExprGenPayload codegenUntypedUn(CodeLoc codeLoc, Token::Oper op, UntypedVal unty);
    ExprGenPayload codegenBin(const AstNode *ast);
    // helper function for short-circuit evaluation of boolean AND and OR
    ExprGenPayload codegenLogicAndOr(const AstNode *ast);
    ExprGenPayload codegenLogicAndOrGlobalScope(const AstNode *ast);
    ExprGenPayload codegenUntypedBin(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR);
    ExprGenPayload codegenCall(const AstNode *ast);
    ExprGenPayload codegenCast(const AstNode *ast);
    ExprGenPayload codegenArr(const AstNode *ast);
    std::optional<StringPool::Id> codegenImport(const AstNode *ast);
    void codegenLet(const AstNode *ast);
    void codegenIf(const AstNode *ast);
    void codegenFor(const AstNode *ast);
    void codegenWhile(const AstNode *ast);
    void codegenDo(const AstNode *ast);
    void codegenBreak(const AstNode *ast);
    void codegenContinue(const AstNode *ast);
    void codegenRet(const AstNode *ast);
    void codegenData(const AstNode *ast);
    void codegenBlock(const AstNode *ast);
    void codegenAll(const AstNode *ast, bool makeScope);
    std::optional<FuncValue> codegenFuncProto(const AstNode *ast, bool definition);
    void codegenFunc(const AstNode *ast);

    std::optional<TypeTable::Id> codegenType(const AstNode *ast);
    ExprGenPayload codegenExpr(const AstNode *ast);

public:
    Codegen(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs);

    llvm::Type* genPrimTypeBool();
    llvm::Type* genPrimTypeI(unsigned bits);
    llvm::Type* genPrimTypeU(unsigned bits);
    llvm::Type* genPrimTypeC(unsigned bits);
    llvm::Type* genPrimTypeF16();
    llvm::Type* genPrimTypeF32();
    llvm::Type* genPrimTypeF64();
    llvm::Type* genPrimTypePtr();

    CompilerAction codegenNode(const AstNode *ast);

    void printout() const;

    bool binary(const std::string &filename);
};