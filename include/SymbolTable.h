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

class TypeTable {
public:
    // TODO known TypeIds for primitive types
    // TODO at some point, have Type struct that can represent ptrs, arrs...
    typedef unsigned Id;

private:
    Id next;

    std::unordered_map<NamePool::Id, Id> typeIds;
    std::vector<llvm::Type*> types;
    TypeTable::Id i64Id;

public:
    TypeTable();

    TypeTable::Id addType(NamePool::Id name, llvm::Type *type);
    llvm::Type* getType(Id id);

    bool isType(NamePool::Id name) const;
    TypeTable::Id getTypeId(NamePool::Id name) const { return typeIds.at(name); }

    // TODO refactor, need other types too
    TypeTable::Id addI64Type(NamePool::Id name, llvm::Type *type);
    llvm::Type* getI64Type();
    TypeTable::Id getI64TypeId() const { return i64Id; }
};

struct FuncSignature {
    NamePool::Id name;
    std::vector<TypeTable::Id> argTypes;

    bool operator==(const FuncSignature &other) const;

    struct Hasher {
        std::size_t operator()(const FuncSignature &k) const;
    };
};

struct FuncValue {
    llvm::Function *func;
    bool hasRet;
    TypeTable::Id retType;
    bool defined;
};

class SymbolTable {
public:
    struct VarPayload {
        TypeTable::Id type;
        llvm::Value *val;
    };

private:
    struct Scope {
        std::unordered_map<NamePool::Id, VarPayload> vars;
        Scope *prev;
    };

    std::unordered_map<FuncSignature, FuncValue, FuncSignature::Hasher> funcs;

    Scope *last, *glob;

    TypeTable *typeTable;

    friend class ScopeControl;
    void newScope();
    void endScope();

public:
    SymbolTable(TypeTable *typeTable);

    void addVar(NamePool::Id name, const VarPayload &var);
    const VarPayload* getVar(NamePool::Id name) const;

    void addFunc(const FuncSignature &sig, const FuncValue &val);
    FuncValue* getFunc(const FuncSignature &sig);
    
    bool inGlobalScope() const { return last == glob; }

    bool varNameTaken(NamePool::Id name) const;
    bool funcNameTaken(NamePool::Id name) const;

    TypeTable* getTypeTable() { return typeTable; }

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