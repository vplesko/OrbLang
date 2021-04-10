#pragma once

#include "SymbolTable.h"

class BlockControl {
    SymbolTable *symbolTable = nullptr;

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

class BlockTmpValControl {
    SymbolTable *symbolTable = nullptr;

public:
    BlockTmpValControl() {}
    // ref cuz must not be null
    explicit BlockTmpValControl(SymbolTable *symbolTable, NodeVal &val) : symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->getLastBlockInternal().tmps.push_back(&val);
    }

    BlockTmpValControl(const BlockTmpValControl&) = delete;
    void operator=(const BlockTmpValControl&) = delete;

    BlockTmpValControl(BlockTmpValControl &&other) {
        symbolTable = other.symbolTable;
        other.symbolTable = nullptr;
    }
    BlockTmpValControl& operator=(BlockTmpValControl &&other) {
        if (this != &other) {
            symbolTable = other.symbolTable;
            other.symbolTable = nullptr;
        }
        return *this;
    }

    ~BlockTmpValControl() {
        if (symbolTable != nullptr) symbolTable->getLastBlockInternal().tmps.pop_back();
    }
};