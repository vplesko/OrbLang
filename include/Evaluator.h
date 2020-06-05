#pragma once

#include "SymbolTable.h"
#include "AST.h"
#include "Values.h"
#include "CompileMessages.h"

class Codegen;

// TODO find a way to not get into a lot of code duplication with Codegen before introducing compile-time evaluations
class Evaluator {
    SymbolTable *symbolTable;
    AstStorage *astStorage;
    CompileMessages *msgs;

    // TODO refactor out
    Codegen *codegen;

    void evaluateMac(AstNode *ast);

    // TODO unify with one from codegen
    std::optional<NamePool::Id> getId(const AstNode *ast, bool orError);

    void substitute(std::unique_ptr<AstNode> &body, const std::vector<NamePool::Id> &names, const std::vector<const AstNode*> &values);

public:
    Evaluator(SymbolTable *symbolTable, AstStorage *astStorage, CompileMessages *msgs);

    void setCodegen(Codegen *c) { codegen = c; }

    bool evaluateNode(AstNode *ast);

    std::unique_ptr<AstNode> evaluateInvoke(const AstNode *ast);
};