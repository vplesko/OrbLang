#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include "NamePool.h"
#include "StringPool.h"
#include "Values.h"
#include "TypeTable.h"
#include "llvm/IR/Instructions.h"

struct FuncCallSite {
    NamePool::Id name;
    std::vector<TypeTable::Id> argTypes;
    std::vector<UntypedVal> untypedVals;

    FuncCallSite() {}
    FuncCallSite(std::size_t sz) : argTypes(sz), untypedVals(sz) {}

    void set(std::size_t ind, TypeTable::Id t) {
        argTypes[ind] = t;
        untypedVals[ind] = {UntypedVal::Kind::kNone};
    }

    void set(std::size_t ind, UntypedVal l) {
        untypedVals[ind] = l;
    }
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
    NamePool::Id name;
    bool variadic;
    bool noNameMangle;
    std::vector<NamePool::Id> argNames;
    std::vector<TypeTable::Id> argTypes;
    std::optional<TypeTable::Id> retType;
    bool defined;
    llvm::Function *func;

    bool hasRet() const { return retType.has_value(); }
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

    StringPool *stringPool;
    TypeTable *typeTable;

    std::unordered_map<FuncSignature, FuncValue, FuncSignature::Hasher> funcs;
    std::unordered_map<NamePool::Id, FuncValue> funcsNoNameMangle;

    Scope *last, *glob;

    bool inFunc;
    FuncValue currFunc;

    void setCurrFunc(const FuncValue &func) { inFunc = true; currFunc = func; }
    void clearCurrFunc() { inFunc = false; }

    void newScope();
    void endScope();

    FuncSignature makeFuncSignature(NamePool::Id name, const std::vector<TypeTable::Id> &argTypes) const;
    std::optional<FuncSignature> makeFuncSignature(const FuncCallSite &call) const;
    bool isCallArgsOk(const FuncCallSite &call, const FuncValue &func) const;

public:
    SymbolTable(StringPool *stringPool, TypeTable *typeTable);

    void addVar(NamePool::Id name, const VarPayload &var);
    std::optional<VarPayload> getVar(NamePool::Id name) const;

    bool canRegisterFunc(const FuncValue &val) const;
    FuncValue registerFunc(const FuncValue &val);
    llvm::Function* getFunction(const FuncValue &val) const;
    std::optional<FuncValue> getFuncForCall(const FuncCallSite &call);

    bool inGlobalScope() const { return last == glob; }

    std::optional<FuncValue> getCurrFunc() const;

    bool isFuncName(NamePool::Id name) const;
    
    bool varMayTakeName(NamePool::Id name) const;
    // only checks for name collisions with global vars and datas, NOT with funcs of same sig!
    bool funcMayTakeName(NamePool::Id name) const;

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
    ScopeControl(SymbolTable &symTable, const FuncValue &func) : symTable(&symTable), funcOpen(true) {
        this->symTable->setCurrFunc(func);
        this->symTable->newScope();
    }

    ScopeControl(const ScopeControl&) = delete;
    void operator=(const ScopeControl&) = delete;

    ScopeControl(const ScopeControl&&) = delete;
    void operator=(const ScopeControl&&) = delete;

    ~ScopeControl() {
        if (symTable) symTable->endScope();
        if (funcOpen) symTable->clearCurrFunc();
    }
};