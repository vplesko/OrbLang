#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    // TODO!
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id) { msgs->errorInternal(codeLoc); return NodeVal(); }
    NodeVal performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty);
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) { msgs->errorInternal(codeLoc); return NodeVal(); }
    bool performFunctionDeclaration(CodeLoc codeLoc, FuncValue &func) { return false; }
    bool performFunctionDefinition(const NodeVal &args, const NodeVal &body, FuncValue &func) { return false; }
    bool performRet(CodeLoc codeLoc) { return false; }
    bool performRet(CodeLoc codeLoc, const NodeVal &node) { return false; }
    NodeVal performEvaluation(const NodeVal &node);

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);
};