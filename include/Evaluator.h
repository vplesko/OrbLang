#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    // TODO!
    NodeVal loadSymbol(NamePool::Id id) { return NodeVal(); }
    NodeVal cast(const NodeVal &node, TypeTable::Id ty) { return NodeVal(); }
    NodeVal evaluateNode(const NodeVal &node) { return NodeVal(); }
    bool makeFunction(const NodeVal &node, FuncValue &func) { return false; }

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);
};