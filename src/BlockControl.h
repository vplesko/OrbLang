#pragma once

#include "SymbolTable.h"

class BlockControl {
    SymbolTable *symbolTable;

public:
    explicit BlockControl(SymbolTable *symbolTable = nullptr) : symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(SymbolTable::Block());
    }
    BlockControl(SymbolTable *symbolTable, SymbolTable::Block bo) : symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(bo);
    }
    // ref cuz must not be null
    BlockControl(SymbolTable *symbolTable, const SymbolTable::CalleeValueInfo &c) : symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(c);
    }

    BlockControl(const BlockControl&) = delete;
    void operator=(const BlockControl&) = delete;

    BlockControl(const BlockControl&&) = delete;
    void operator=(const BlockControl&&) = delete;

    ~BlockControl() {
        if (symbolTable != nullptr) symbolTable->endBlock();
    }
};