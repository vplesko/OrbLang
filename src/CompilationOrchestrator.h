#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "CompilationMessages.h"
#include "Compiler.h"
#include "Evaluator.h"
#include "ProgramArgs.h"
#include "SymbolTable.h"

class CompilationOrchestrator {
    std::unique_ptr<NamePool> namePool;
    std::unique_ptr<StringPool> stringPool;
    std::unique_ptr<TypeTable> typeTable;
    std::unique_ptr<SymbolTable> symbolTable;
    std::unique_ptr<CompilationMessages> msgs;
    std::unique_ptr<Compiler> compiler;
    std::unique_ptr<Evaluator> evaluator;

    void genReserved();
    void genPrimTypes();

public:
    CompilationOrchestrator(std::ostream &out);

    bool process(const ProgramArgs &args);
    void printout(const std::string &filename) const;
    bool compile(const ProgramArgs &args);

    bool isInternalError() const;
};