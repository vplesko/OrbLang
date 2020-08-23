#pragma once

#include "Processor.h"

class Evaluator : public Processor {
    // TODO!
    NodeVal cast(const NodeVal &node, TypeTable::Id ty) { return NodeVal(); }
    NodeVal loadSymbol(NamePool::Id id) { return NodeVal(); }

public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);
};