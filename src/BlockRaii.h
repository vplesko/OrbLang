#pragma once

#include "SymbolTable.h"

class BlockRaii {
    SymbolTable *symbolTable = nullptr;

public:
    explicit BlockRaii(SymbolTable *symbolTable = nullptr) : symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(SymbolTable::Block());
    }
    BlockRaii(SymbolTable *symbolTable, SymbolTable::Block bo) : symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(bo);
    }
    // ref cuz must not be null
    BlockRaii(SymbolTable *symbolTable, const SymbolTable::CalleeValueInfo &c) : symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->newBlock(c);
    }

    BlockRaii(const BlockRaii&) = delete;
    void operator=(const BlockRaii&) = delete;

    BlockRaii(const BlockRaii&&) = delete;
    void operator=(const BlockRaii&&) = delete;

    ~BlockRaii() {
        if (symbolTable != nullptr) symbolTable->endBlock();
    }
};

class BlockTmpValRaii {
    SymbolTable *symbolTable = nullptr;

public:
    BlockTmpValRaii() {}
    explicit BlockTmpValRaii(SymbolTable *symbolTable, NodeVal val) : symbolTable(symbolTable) {
        if (symbolTable != nullptr) symbolTable->getLastBlockInternal().tmps.push_back(std::move(val));
    }

    BlockTmpValRaii(const BlockTmpValRaii&) = delete;
    void operator=(const BlockTmpValRaii&) = delete;

    BlockTmpValRaii(BlockTmpValRaii &&other) {
        symbolTable = other.symbolTable;
        other.symbolTable = nullptr;
    }
    BlockTmpValRaii& operator=(BlockTmpValRaii &&other) {
        if (this != &other) {
            symbolTable = other.symbolTable;
            other.symbolTable = nullptr;
        }
        return *this;
    }

    ~BlockTmpValRaii() {
        if (symbolTable != nullptr) symbolTable->getLastBlockInternal().tmps.pop_back();
    }
};