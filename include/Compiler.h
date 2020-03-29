#pragma once

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include "SymbolTable.h"
#include "CompileMessages.h"
#include "Codegen.h"

class Compiler {
    std::unique_ptr<NamePool> namePool;
    std::unique_ptr<TypeTable> typeTable;
    std::unique_ptr<SymbolTable> symbolTable;
    std::unique_ptr<CompileMessages> msgs;
    std::unique_ptr<Codegen> codegen;

    void genPrimTypes();

public:
    Compiler();

    bool parse(const std::vector<std::string> &inputs);
    void printout() const;
    void dumpMsgs(std::ostream &out);
    bool compile(const std::string &output, bool exe);
};