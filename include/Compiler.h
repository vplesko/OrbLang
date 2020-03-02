#pragma once

#include <string>
#include <memory>
#include "SymbolTable.h"
#include "Codegen.h"

class Compiler {
    std::unique_ptr<NamePool> namePool;
    std::unique_ptr<TypeTable> typeTable;
    std::unique_ptr<SymbolTable> symbolTable;
    std::unique_ptr<Codegen> codegen;

    void genPrimTypes();

public:
    Compiler();

    bool parse(const std::string &filename);
    void printout() const;
    bool compile(const std::string &filename);

    ~Compiler();
};