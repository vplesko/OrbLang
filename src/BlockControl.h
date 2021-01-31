#pragma once

#include "SymbolTable.h"

class Processor;

class BlockControl {
    Processor *processor;
    SymbolTable *symTable;

public:
    explicit BlockControl(Processor *processor, SymbolTable *symTable = nullptr) : processor(processor), symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(SymbolTable::Block());
    }
    BlockControl(Processor *processor, SymbolTable *symTable, SymbolTable::Block bo) : processor(processor), symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(bo);
    }
    // ref cuz must not be null
    BlockControl(Processor *processor, SymbolTable *symTable, const SymbolTable::CalleeValueInfo &c) : processor(processor), symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(c);
    }

    BlockControl(const BlockControl&) = delete;
    void operator=(const BlockControl&) = delete;

    BlockControl(const BlockControl&&) = delete;
    void operator=(const BlockControl&&) = delete;

    ~BlockControl();
};