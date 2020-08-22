#pragma once

#include "Processor.h"

class Evaluator : public Processor {
public:
    Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs);
};