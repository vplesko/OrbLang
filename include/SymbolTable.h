#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include "llvm/IR/Instructions.h"
#include "NamePool.h"
#include "TypeTable.h"

/*
UntypedVal is needed to represent a literal value whose exact type is yet unknown.

Let's say we interpret an integer literal as i32.
Then the user couldn't do this: i8 i = 0;

Let's say we interpret it as the shortest type it can fit.
Then this would overflow: i32 i = 250 + 10;

Therefore, we need this intermediate value holder.

REM
Does not hold uint values. Users can overcome this by casting.
*/
struct UntypedVal {
    enum Type {
        T_NONE,
        T_SINT,
        T_FLOAT,
        T_CHAR,
        T_STRING,
        T_BOOL,
        T_NULL
    };

    Type type;
    union {
        int64_t val_si;
        double val_f;
        char val_c;
        bool val_b;
    };
    std::string val_str;

    static std::size_t getStringLen(const std::string &str) { return str.size()+1; }
    std::size_t getStringLen() const { return getStringLen(val_str); }
};

struct FuncCallSite {
    NamePool::Id name;
    std::vector<TypeTable::Id> argTypes;
    std::vector<UntypedVal> untypedVals;

    FuncCallSite() {}
    FuncCallSite(std::size_t sz) : argTypes(sz), untypedVals(sz) {}

    void set(std::size_t ind, TypeTable::Id t) {
        argTypes[ind] = t;
        untypedVals[ind] = {UntypedVal::T_NONE};
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
    std::vector<TypeTable::Id> argTypes;
    bool hasRet;
    TypeTable::Id retType;
    bool defined;
    llvm::Function *func;
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
    std::unordered_map<NamePool::Id, FuncValue> funcsNoNameMangle;

    Scope *last, *glob;

    bool inFunc;
    FuncValue currFunc;

    void setCurrFunc(const FuncValue &func) { inFunc = true; currFunc = func; }
    void clearCurrFunc() { inFunc = false; }

    void newScope();
    void endScope();

    FuncSignature makeFuncSignature(NamePool::Id name, const std::vector<TypeTable::Id> &argTypes) const;
    std::pair<FuncSignature, bool> makeFuncSignature(const FuncCallSite &call) const;

public:
    SymbolTable(TypeTable *typeTable);

    void addVar(NamePool::Id name, const VarPayload &var);
    std::pair<VarPayload, bool> getVar(NamePool::Id name) const;

    bool canRegisterFunc(const FuncValue &val) const;
    FuncValue registerFunc(const FuncValue &val);
    llvm::Function* getFunction(const FuncValue &val) const;
    std::pair<FuncValue, bool> getFuncForCall(const FuncCallSite &call);

    bool inGlobalScope() const { return last == glob; }

    std::pair<FuncValue, bool> getCurrFunc() const { return std::make_pair(currFunc, inFunc); }

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