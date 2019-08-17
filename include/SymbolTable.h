#pragma once

#include <unordered_map>
#include <string>
#include "llvm/IR/Instructions.h"

class NamePool {
public:
    typedef unsigned Id;

private:
    Id next;

    std::unordered_map<Id, std::string> names;
    std::unordered_map<std::string, Id> ids;

public:
    NamePool();

    Id add(const std::string &name);
    const std::string& get(Id id) const { return names.at(id); }
};

struct FuncSignature {
    typedef std::size_t ArgSize;

    NamePool::Id name;
    ArgSize argCnt;

    bool operator==(const FuncSignature &other) const {
        return name == other.name && argCnt == other.argCnt;
    }

    struct Hasher {
        std::size_t operator()(const FuncSignature &k) const {
            // le nice hashe functione
            return (17*31+k.name)*31+k.argCnt;
        }
    };
};

struct FuncValue {
    llvm::Function *func;
    bool hasRetVal;
    bool defined;
};

class SymbolTable {
    struct Scope {
        std::unordered_map<NamePool::Id, llvm::Value*> vars;
        Scope *prev;
    };

    std::unordered_map<FuncSignature, FuncValue, FuncSignature::Hasher> funcs;

    Scope *last, *glob;

    friend class ScopeControl;
    void newScope();
    void endScope();

public:
    SymbolTable();

    void addVar(NamePool::Id name, llvm::Value *val);
    llvm::Value* getVar(NamePool::Id name) const;

    void addFunc(const FuncSignature &sig, const FuncValue &val);
    const FuncValue* getFunc(const FuncSignature &sig) const;
    
    bool inGlobalScope() const { return last == glob; }
    bool taken(NamePool::Id name) const;

    ~SymbolTable();
};

class ScopeControl {
    SymbolTable *symTable;

public:
    ScopeControl(SymbolTable *symTable = nullptr) : symTable(symTable) {
        if (symTable != nullptr) symTable->newScope();
    }

    ScopeControl(const ScopeControl&) = delete;
    void operator=(const ScopeControl&) = delete;

    ScopeControl(const ScopeControl&&) = delete;
    void operator=(const ScopeControl&&) = delete;

    ~ScopeControl() {
        if (symTable) symTable->endScope();
    }
};