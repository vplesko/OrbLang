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

// TODO scoping for vars
class SymbolTable {
    std::unordered_map<NamePool::Id, llvm::AllocaInst*> vars;

    // TODO need to store func info (args, ret type)
    std::unordered_map<NamePool::Id, llvm::Function*> funcs;

public:

    void addVar(NamePool::Id name, llvm::AllocaInst *val) { vars.insert(std::make_pair(name, val)); }
    llvm::AllocaInst* getVar(NamePool::Id name) const;

    void addFunc(NamePool::Id name, llvm::Function *val) { funcs.insert(std::make_pair(name, val)); }
    llvm::Function* getFunc(NamePool::Id name) const;
};