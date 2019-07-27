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
    struct Scope {
        std::unordered_map<NamePool::Id, llvm::Value*> vars;
        Scope *prev;
    };

    // TODO need to store func info (args, ret type) and if it's defined (or just declared)
    std::unordered_map<NamePool::Id, llvm::Function*> funcs;

    Scope *last, *glob;

public:
    SymbolTable();

    void newScope();
    void endScope();

    void addVar(NamePool::Id name, llvm::Value *val);
    llvm::Value* getVar(NamePool::Id name) const;

    void addFunc(NamePool::Id name, llvm::Function *val);
    llvm::Function* getFunc(NamePool::Id name) const;
    
    bool inGlobalScope() const { return last == glob; }
    bool taken(NamePool::Id name) const;

    ~SymbolTable();
};