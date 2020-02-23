#pragma once

#include <unordered_map>
#include "llvm/IR/Instructions.h"
#include "NamePool.h"
#include "TypeTable.h"

/*
LiteralVal is needed to represent a literal value who's exact type is yet unknown.

Let's say we interpret an integer literal as i32.
Then the user couldn't do this: i8 i = 0;

Let's say we interpret it as the shortest type it can fit.
Then this would underflow: i32 i = 250 + 10;

Therefore, we need this intermediate value holder.

REM
Does not hold uint values. Users can overcome this by casting.
*/
struct LiteralVal {
    enum Type {
        T_NONE,
        T_SINT,
        T_FLOAT,
        T_BOOL,
        T_NULL
    };

    Type type;
    union {
        std::int64_t val_si;
        double val_f;
        bool val_b;
    };
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
    std::pair<const FuncSignature*, FuncValue*> getFuncCastsAllowed(const FuncSignature &sig, const LiteralVal *litVals);
    
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