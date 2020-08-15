#pragma once

#include "CodeProcessor.h"

class Evaluator : public CodeProcessor {
    AstStorage *astStorage;

    bool loopIssued, exitIssued;
    std::optional<NamePool::Id> blockGoto;
    std::optional<NodeVal> blockPassVal;

    bool isGotoIssued() const {
        return loopIssued || exitIssued || blockPassVal.has_value();
    }

    void resetGotoIssuing() {
        loopIssued = false;
        exitIssued = false;
        blockGoto.reset();
        blockPassVal.reset();
    }

public:
    NodeVal calculateOperUnary(CodeLoc codeLoc, Token::Oper op, KnownVal known);
    NodeVal calculateOper(CodeLoc codeLoc, Token::Oper op, KnownVal knownL, KnownVal knownR);
    NodeVal calculateCast(CodeLoc codeLoc, KnownVal known, TypeTable::Id type);

protected:
    bool isSkippingProcessing() const { return isGotoIssued(); }

    NodeVal processVar(const AstNode *ast) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    NodeVal processOperUnary(const AstNode *ast, const NodeVal &first);
    NodeVal processOperInd(const AstNode *ast) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    NodeVal processOperDot(const AstNode *ast) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    NodeVal handleOper(CodeLoc codeLoc, Token::Oper op, const NodeVal &lhs, const NodeVal &rhs);
    NodeVal processTuple(const AstNode *ast, const NodeVal &first) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    NodeVal processCall(const AstNode *ast, const NodeVal &first) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    NodeVal processCast(const AstNode *ast);
    NodeVal processArr(const AstNode *ast) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    NodeVal processSym(const AstNode *ast) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    void handleExit(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond);
    void handleLoop(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond);
    void handlePass(const SymbolTable::Block *targetBlock, CodeLoc valCodeLoc, NodeVal val, TypeTable::Id expectedType);
    NodeVal handleBlock(std::optional<NamePool::Id> name, std::optional<TypeTable::Id> type, const AstNode *nodeBody);
    NodeVal processRet(const AstNode *ast) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    NodeVal processFunc(const AstNode *ast) { msgs->errorEvaluationNotSupported(ast->codeLoc); return NodeVal(); }
    NodeVal processEvalNode(const AstNode *ast) { return processNode(ast); }

public:
    NodeVal processKnownVal(const AstNode *ast);
    NodeVal processType(const AstNode *ast, const NodeVal &first);
    NodeVal processMac(AstNode *ast);
    std::unique_ptr<AstNode> processInvoke(NamePool::Id macroName, const AstNode *ast);

private:
    std::optional<NodeVal> evaluateTypeDescr(const AstNode *ast, const NodeVal &first);

    bool cast(KnownVal &val, TypeTable::Id t) const;
    void substitute(std::unique_ptr<AstNode> &body, const std::vector<NamePool::Id> &names, const std::vector<const AstNode*> &values);

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, CompileMessages *msgs, AstStorage *astStorage);
};