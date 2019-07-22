#pragma once

#include <unordered_map>
#include <string>
#include "llvm/IR/Instructions.h"

class NamePool {
public:
    typedef unsigned Id;

private:
    Id next;
    // TODO: urgh, optimize plz
    std::unordered_map<Id, std::string> names;
    std::unordered_map<std::string, Id> ids;

public:
    NamePool();

    Id add(const std::string &name);
    const std::string& get(Id id) const { return names.at(id); }
};

class SymbolTable {
    std::unordered_map<NamePool::Id, llvm::AllocaInst*> symbols;

public:

    void add(NamePool::Id name, llvm::AllocaInst *val) { symbols.insert(std::make_pair(name, val)); }
    llvm::AllocaInst* get(NamePool::Id name);
};