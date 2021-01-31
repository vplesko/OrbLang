#pragma once

#include "SymbolTable.h"

class Processor;

class BlockControl {
    Processor *processor;
    SymbolTable *symbolTable;

public:
    explicit BlockControl(Processor *processor, SymbolTable *symbolTable = nullptr) : processor(processor), symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(SymbolTable::Block());
    }
    BlockControl(Processor *processor, SymbolTable *symbolTable, SymbolTable::Block bo) : processor(processor), symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(bo);
    }
    // ref cuz must not be null
    BlockControl(Processor *processor, SymbolTable *symbolTable, const SymbolTable::CalleeValueInfo &c) : processor(processor), symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(c);
    }

    BlockControl(const BlockControl&) = delete;
    void operator=(const BlockControl&) = delete;

    BlockControl(const BlockControl&&) = delete;
    void operator=(const BlockControl&&) = delete;

    ~BlockControl();
};