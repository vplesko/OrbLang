#pragma once

#include "SymbolTable.h"
#include "AST.h"
#include "Values.h"
#include "CompileMessages.h"

class Codegen;

class Evaluator {
    SymbolTable *symbolTable;
    AstStorage *astStorage;
    CompileMessages *msgs;

    // TODO refactor out
    Codegen *codegen;

    TypeTable* getTypeTable() { return symbolTable->getTypeTable(); }
    const TypeTable* getTypeTable() const { return symbolTable->getTypeTable(); }

    NodeVal evaluateMac(AstNode *ast);
    NodeVal evaluateImport(AstNode *ast);

    // TODO unify with one from codegen
    std::optional<NamePool::Id> getId(const AstNode *ast, bool orError);

    NodeVal evaluateUntypedVal(const AstNode *ast);
    NodeVal evaluateOperUnary(const AstNode *ast, const NodeVal &first);
    NodeVal evaluateOper(CodeLoc codeLoc, Token::Oper op, const NodeVal &lhs, const NodeVal &rhs);
    NodeVal evaluateOper(const AstNode *ast, const NodeVal &first);

    void substitute(std::unique_ptr<AstNode> &body, const std::vector<NamePool::Id> &names, const std::vector<const AstNode*> &values);

public:
    Evaluator(SymbolTable *symbolTable, AstStorage *astStorage, CompileMessages *msgs);

    void setCodegen(Codegen *c) { codegen = c; }

    // TODO unify with evaluateNode
    CompilerAction evaluateGlobalNode(AstNode *ast);

    NodeVal evaluateTerminal(const AstNode *ast);
    NodeVal evaluateNode(const AstNode *ast);

    std::unique_ptr<AstNode> evaluateInvoke(const AstNode *ast);

    // TODO! if no longer called by Codegen, make private
    NodeVal calculate(CodeLoc codeLoc, Token::Oper op, UntypedVal unty);
    NodeVal calculate(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR);
};