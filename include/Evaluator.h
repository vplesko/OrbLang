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

    void evaluateMac(AstNode *ast);

public:
    Evaluator(SymbolTable *symbolTable, AstStorage *astStorage, CompileMessages *msgs);

    void setCodegen(Codegen *c) { codegen = c; }

    bool evaluateNode(AstNode *ast);
};