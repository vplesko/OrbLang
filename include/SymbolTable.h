#pragma once

#include <unordered_map>
#include "llvm/IR/Instructions.h"
#include "NamePool.h"
#include "TypeTable.h"

struct LiteralVal {
    enum Type {
        T_NONE,
        T_SINT,
        T_FLOAT,
        T_BOOL
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