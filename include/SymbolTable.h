#pragma once

#include <unordered_map>
#include <string>
#include "llvm/IR/Instructions.h"
#include "utils.h"

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
    // TODO at some point, have Type struct that can represent ptrs, arrs...
    typedef unsigned Id;

    enum PrimIds {
        P_BOOL,
        P_I8,
        P_I16,
        P_I32,
        P_I64,
        P_U8,
        P_U16,
        P_U32,
        P_U64,
        P_F16,
        P_F32,
        P_F64,
        P_ENUM_END // length marker, do not reference
    };

    static bool isImplicitCastable(Id from, Id into) {
        // TODO allow unsigned to int if lossless
        PrimIds s = (PrimIds) from, d = (PrimIds) into;
        return (between(s, P_I8, P_I64) && between(d, s, P_I64)) ||
            (between(s, P_U8, P_U64) && between(d, s, P_U64)) ||
            (between(s, P_F16, P_F64) && between(d, s, P_F64));
    }

    static bool isTypeI(Id t) {
        return between((PrimIds) t, P_I8, P_I64);
    }

    static bool isTypeU(Id t) {
        return between((PrimIds) t, P_U8, P_U64);
    }

    static bool isTypeF(Id t) {
        return between((PrimIds) t, P_F16, P_F64);
    }

private:
    Id next;

    std::unordered_map<NamePool::Id, Id> typeIds;
    std::vector<llvm::Type*> types;

public:
    TypeTable();

    TypeTable::Id addType(NamePool::Id name, llvm::Type *type);
    void addPrimType(NamePool::Id name, PrimIds id, llvm::Type *type);
    llvm::Type* getType(Id id);

    bool isType(NamePool::Id name) const;
    TypeTable::Id getTypeId(NamePool::Id name) const { return typeIds.at(name); }
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
    friend class ScopeControl;

    struct Scope {
        std::unordered_map<NamePool::Id, VarPayload> vars;
        Scope *prev;
    };

    TypeTable *typeTable;

    std::unordered_map<FuncSignature, FuncValue, FuncSignature::Hasher> funcs;

    Scope *last, *glob;

    // this for now; someday might contain loop info for break/continue (maybe lambda info)
    FuncValue *currFunc;

    void setCurrFunc(FuncValue *func) { currFunc = func; }

    void newScope();
    void endScope();

public:
    SymbolTable(TypeTable *typeTable);

    void addVar(NamePool::Id name, const VarPayload &var);
    const VarPayload* getVar(NamePool::Id name) const;

    void addFunc(const FuncSignature &sig, const FuncValue &val);
    FuncValue* getFunc(const FuncSignature &sig);
    std::pair<const FuncSignature*, FuncValue*> getFuncImplicitCastsAllowed(const FuncSignature &sig);
    
    bool inGlobalScope() const { return last == glob; }

    const FuncValue* getCurrFunc() const { return currFunc; }

    bool varNameTaken(NamePool::Id name) const;
    bool funcNameTaken(NamePool::Id name) const;

    TypeTable* getTypeTable() { return typeTable; }

    ~SymbolTable();
};

class ScopeControl {
    SymbolTable *symTable;
    bool funcOpen;

public:
    ScopeControl(SymbolTable *symTable = nullptr) : symTable(symTable), funcOpen(false) {
        if (symTable != nullptr) symTable->newScope();
    }
    // ref cuz must not be null
    ScopeControl(SymbolTable &symTable, FuncValue &func) : symTable(&symTable), funcOpen(true) {
        this->symTable->setCurrFunc(&func);
        this->symTable->newScope();
    }

    ScopeControl(const ScopeControl&) = delete;
    void operator=(const ScopeControl&) = delete;

    ScopeControl(const ScopeControl&&) = delete;
    void operator=(const ScopeControl&&) = delete;

    ~ScopeControl() {
        if (symTable) symTable->endScope();
        if (funcOpen) symTable->setCurrFunc(nullptr);
    }
};