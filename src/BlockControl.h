#pragma once

#include "SymbolTable.h"

class BlockControl {
    SymbolTable *symTable;

public:
    explicit BlockControl(SymbolTable *symTable = nullptr) : symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(SymbolTable::Block());
    }
    BlockControl(SymbolTable *symTable, SymbolTable::Block bo) : symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(bo);
    }
    // ref cuz must not be null
    BlockControl(SymbolTable *symTable, const SymbolTable::CalleeValueInfo &c) : symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(c);
    }

    BlockControl(const BlockControl&) = delete;
    void operator=(const BlockControl&) = delete;

    BlockControl(const BlockControl&&) = delete;
    void operator=(const BlockControl&&) = delete;

    ~BlockControl() {
        if (symTable != nullptr) symTable->endBlock();
    }
};