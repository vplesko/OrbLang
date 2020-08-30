#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    // TODO!
    NodeVal performLoad(CodeLoc codeLoc, NamePool::Id id) { return NodeVal(); }
    // TODO don't forget str to char arr
    NodeVal performCast(const NodeVal &node, TypeTable::Id ty) { return NodeVal(); }
    NodeVal performCall(CodeLoc codeLoc, const FuncValue &func, const std::vector<NodeVal> &args) { return NodeVal(); }
    bool performFunctionMake(const NodeVal &node, FuncValue &func) { return false; }
    NodeVal performEvaluation(const NodeVal &node);

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);
};