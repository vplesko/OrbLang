#pragma once

#include "SymbolTable.h"
#include "AST.h"
#include "Values.h"
#include "CompileMessages.h"

class CodeProcessor {
protected:
    NamePool *namePool;
    StringPool *stringPool;
    SymbolTable *symbolTable;
    CompileMessages *msgs;
    
    bool isGlobalScope() const { return symbolTable->inGlobalScope(); }
    SymbolTable::Block* getBlock(CodeLoc codeLoc, NamePool::Id name, bool orError);

    TypeTable* getTypeTable() { return symbolTable->getTypeTable(); }
    const TypeTable* getTypeTable() const { return symbolTable->getTypeTable(); }
    
    TypeTable::Id getPrimTypeId(TypeTable::PrimIds primId) const { return getTypeTable()->getPrimTypeId(primId); }

    bool checkIsEmptyTerminal(const AstNode *ast, bool orError);
    bool checkIsEllipsis(const AstNode *ast, bool orError);
    bool checkIsTerminal(const AstNode *ast, bool orError);
    bool checkIsNotTerminal(const AstNode *ast, bool orError);
    bool checkIsBlock(const AstNode *ast, bool orError);
    bool checkExactlyChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkAtLeastChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkAtMostChildren(const AstNode *ast, std::size_t n, bool orError);
    bool checkBetweenChildren(const AstNode *ast, std::size_t nLo, std::size_t nHi, bool orError);
    bool checkValueUnbroken(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsId(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsKeyword(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsOper(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsType(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsKnown(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkIsAttribute(CodeLoc codeLoc, const NodeVal &val, bool orError);
    bool checkGlobalScope(CodeLoc codeLoc, bool orError);
    bool checkIsBool(CodeLoc codeLoc, const NodeVal &val, bool orError);
    
    typedef std::pair<NamePool::Id, TypeTable::Id> NameTypePair;

    std::optional<NamePool::Id> getId(const AstNode *ast, bool orError);
    std::optional<KnownVal> getKnownVal(const AstNode *ast, bool orError);
    std::optional<TypeTable::Id> getType(const AstNode *ast, bool orError);
    std::optional<NameTypePair> getIdTypePair(const AstNode *ast, bool orError);
    std::optional<Token::Attr> getAttr(const AstNode *ast, bool orError);

    virtual bool isSkippingProcessing() const =0;

    virtual NodeVal processKnownVal(const AstNode *ast) =0;
    virtual NodeVal processType(const AstNode *ast, const NodeVal &first) =0;
    virtual NodeVal processVar(const AstNode *ast) =0;
    virtual NodeVal processOperUnary(const AstNode *ast, const NodeVal &first) =0;
    virtual NodeVal processOperInd(const AstNode *ast) =0;
    virtual NodeVal processOperDot(const AstNode *ast) =0;
    virtual NodeVal handleOper(CodeLoc codeLoc, Token::Oper op, const NodeVal &lhs, const NodeVal &rhs) =0;
    virtual NodeVal processTuple(const AstNode *ast, const NodeVal &first) =0;
    virtual NodeVal processCall(const AstNode *ast, const NodeVal &first) =0;
    virtual NodeVal processCast(const AstNode *ast) =0;
    virtual NodeVal processArr(const AstNode *ast) =0;
    virtual NodeVal processSym(const AstNode *ast) =0;
    virtual void handleExit(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond) =0;
    virtual void handleLoop(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond) =0;
    virtual void handlePass(const SymbolTable::Block *targetBlock, CodeLoc valCodeLoc, NodeVal val, TypeTable::Id expectedType) =0;
    virtual NodeVal handleBlock(std::optional<NamePool::Id> name, std::optional<TypeTable::Id> type, const AstNode *nodeBody) =0;
    virtual NodeVal processRet(const AstNode *ast) =0;
    virtual NodeVal processFunc(const AstNode *ast) =0;
    virtual NodeVal processMac(AstNode *ast) =0;
    virtual NodeVal processEvalNode(const AstNode *ast) =0;
    virtual std::unique_ptr<AstNode> processInvoke(NamePool::Id macroName, const AstNode *ast) =0;

    NodeVal processAll(const AstNode *ast);

private:
    bool verifyTypeAnnotationMatch(const NodeVal &val, const AstNode *typeAst, bool orError);

    NodeVal processTerminal(const AstNode *ast);
    NodeVal processOper(const AstNode *ast, const NodeVal &first);
    NodeVal processExpr(const AstNode *ast, const NodeVal &first);
    NodeVal processExit(const AstNode *ast);
    NodeVal processLoop(const AstNode *ast);
    NodeVal processPass(const AstNode *ast);
    NodeVal processBlock(const AstNode *ast);
    NodeVal processImport(const AstNode *ast);

public:
    CodeProcessor(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs);

    NodeVal processNode(const AstNode *ast);

    virtual ~CodeProcessor() {}
};