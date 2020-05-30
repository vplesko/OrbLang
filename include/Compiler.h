#pragma once

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include "SymbolTable.h"
#include "CompileMessages.h"
#include "Codegen.h"
#include "Evaluator.h"

class Compiler {
    std::unique_ptr<NamePool> namePool;
    std::unique_ptr<StringPool> stringPool;
    std::unique_ptr<TypeTable> typeTable;
    std::unique_ptr<SymbolTable> symbolTable;
    std::unique_ptr<AstStorage> astStorage;
    std::unique_ptr<CompileMessages> msgs;
    std::unique_ptr<Codegen> codegen;
    std::unique_ptr<Evaluator> evaluator;

    void genPrimTypes();

public:
    Compiler(std::ostream &out);

    bool parse(const std::vector<std::string> &inputs);
    void printout() const;
    bool compile(const std::string &output, bool exe);
};