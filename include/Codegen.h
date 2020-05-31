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
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

class Evaluator;

class Codegen {
    // TODO evaluator may call some utility methods from Codegen, factor them out of this class and break this friendship
    friend class Evaluator;

    NamePool *namePool;
    StringPool *stringPool;
    SymbolTable *symbolTable;
    Evaluator *evaluator;
    CompileMessages *msgs;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder, llvmBuilderAlloca;
    std::unique_ptr<llvm::Module> llvmModule;
    std::unique_ptr<llvm::PassManagerBuilder> llvmPMB;
    std::unique_ptr<llvm::legacy::FunctionPassManager> llvmFPM;

    bool isBool(const NodeVal &e) const;

    bool promoteUntyped(NodeVal &e, TypeTable::Id t);

    TypeTable* getTypeTable() { return symbolTable->getTypeTable(); }
    const TypeTable* getTypeTable() const { return symbolTable->getTypeTable(); }

    llvm::Value* getConstB(bool val);

    llvm::AllocaInst* createAlloca(llvm::Type *type, const std::string &name);
    llvm::GlobalValue* createGlobal(llvm::Type *type, llvm::Constant *init, bool isConstant, const std::string &name);
    llvm::Constant* createString(const std::string &str);
    
    bool mangleName(std::stringstream &ss, TypeTable::Id ty);
    std::optional<NamePool::Id> mangleName(const FuncValue &f);

    llvm::Type* getLlvmType(TypeTable::Id typeId);
    TypeTable::Id getPrimTypeId(TypeTable::PrimIds primId) const { return getTypeTable()->getPrimTypeId(primId); }
    bool createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId);
    bool createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId);
    bool createCast(NodeVal &e, TypeTable::Id t);

    bool isGlobalScope() const;
    bool isLlvmBlockTerminated() const;

    typedef std::pair<NamePool::Id, TypeTable::Id> NameTypePair;

    bool checkEmptyTerminal(const AstNode *ast, bool orError);
    bool checkEllipsis(const AstNode *ast, bool orError);
    bool checkTerminal(const AstNode *ast, bool orError);
    bool checkNotTerminal(const AstNode *ast, bool orError);
    bool checkBlock(const AstNode *ast, bool orError);
    bool checkExactlyChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkAtLeastChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkAtMostChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkBetweenChildren(const AstNode *ast, std::size_t nLo, std::size_t nHi, bool orError);
    bool checkValueUnbroken(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsId(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsKeyword(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsOper(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsType(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsUntyped(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsAttribute(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkGlobalScope(CodeLoc codeLoc, bool orError);

    std::optional<NamePool::Id> getId(const AstNode *ast, bool orError);
    std::optional<TypeTable::Id> getType(const AstNode *ast, bool orError);
    std::optional<NameTypePair> getIdTypePair(const AstNode *ast, bool orError);
    std::optional<Token::Type> getKeyword(const AstNode *ast, bool orError);
    std::optional<Token::Oper> getOper(const AstNode *ast, bool orError);
    std::optional<UntypedVal> getUntypedVal(const AstNode *ast, bool orError);
    std::optional<Token::Attr> getAttr(const AstNode *ast, bool orError);

    NodeVal codegenUntypedVal(const AstNode *ast);
    NodeVal codegenVar(const AstNode *ast);
    NodeVal codegenOperInd(CodeLoc codeLoc, const NodeVal &base, const NodeVal &ind);
    NodeVal codegenOperInd(const AstNode *ast);
    NodeVal codegenOperDot(CodeLoc codeLoc, const NodeVal &base, const NodeVal &memb);
    NodeVal codegenOperDot(const AstNode *ast);
    NodeVal codegenOperUnary(const AstNode *ast, const NodeVal &first);
    NodeVal codegenOperUnaryUntyped(CodeLoc codeLoc, Token::Oper op, UntypedVal unty);
    NodeVal codegenOper(CodeLoc codeLoc, Token::Oper op, const NodeVal &lhs, const NodeVal &rhs);
    NodeVal codegenOper(const AstNode *ast, const NodeVal &first);
    // helper function for short-circuit evaluation of boolean AND and OR
    NodeVal codegenOperLogicAndOr(const AstNode *ast, const NodeVal &first);
    NodeVal codegenOperLogicAndOrGlobalScope(const AstNode *ast, const NodeVal &first);
    NodeVal codegenOperBinaryUntyped(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR);
    NodeVal codegenTuple(const AstNode *ast, const NodeVal &first);
    NodeVal codegenCall(const AstNode *ast, const NodeVal &first);
    NodeVal codegenCast(const AstNode *ast);
    NodeVal codegenArr(const AstNode *ast);
    NodeVal codegenImport(const AstNode *ast);
    NodeVal codegenLet(const AstNode *ast);
    NodeVal codegenIf(const AstNode *ast);
    NodeVal codegenFor(const AstNode *ast);
    NodeVal codegenWhile(const AstNode *ast);
    NodeVal codegenDo(const AstNode *ast);
    NodeVal codegenExit(const AstNode *ast);
    NodeVal codegenLoop(const AstNode *ast);
    NodeVal codegenRet(const AstNode *ast);
    NodeVal codegenBlock(const AstNode *ast);
    NodeVal codegenFunc(const AstNode *ast);

    std::optional<NodeVal> codegenTypeDescr(const AstNode *ast, const NodeVal &first);
    NodeVal codegenType(const AstNode *ast, const NodeVal &first);

    NodeVal codegenExpr(const AstNode *ast, const NodeVal &first);

    NodeVal codegenTerminal(const AstNode *ast);
    NodeVal codegenAll(const AstNode *ast);

    std::optional<FuncValue> codegenFuncProto(const AstNode *ast, bool definition);

public:
    Codegen(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs);

    void setEvaluator(Evaluator *e) { evaluator = e; }

    llvm::Type* genPrimTypeBool();
    llvm::Type* genPrimTypeI(unsigned bits);
    llvm::Type* genPrimTypeU(unsigned bits);
    llvm::Type* genPrimTypeC(unsigned bits);
    llvm::Type* genPrimTypeF16();
    llvm::Type* genPrimTypeF32();
    llvm::Type* genPrimTypeF64();
    llvm::Type* genPrimTypePtr();

    NodeVal codegenNode(const AstNode *ast);

    void printout() const;

    bool binary(const std::string &filename);
};