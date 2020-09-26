#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include "NamePool.h"
#include "TypeTable.h"
#include "NodeVal.h"
#include "llvm/IR/Instructions.h"

// TODO consider renaming, may create confusion when first-class functions are introduces
struct FuncValue {
    NamePool::Id name;
    bool variadic;
    std::vector<NamePool::Id> argNames;
    std::vector<TypeTable::Id> argTypes;
    std::optional<TypeTable::Id> retType;
    bool defined;

    llvm::Function *func;

    std::size_t argCnt() const { return argNames.size(); }
    bool hasRet() const { return retType.has_value(); }

    static bool sameSignature(const FuncValue &fl, const FuncValue &fr);
};

// TODO consider renaming, may create confusion when first-class macros are introduces
struct MacroValue {
    NamePool::Id name;
    std::vector<NamePool::Id> argNames;
    NodeVal body;
};

class SymbolTable {
public:
    struct Block {
        std::optional<NamePool::Id> name;
        std::optional<TypeTable::Id> type;
        llvm::BasicBlock *blockExit = nullptr, *blockLoop = nullptr;
        llvm::PHINode *phi = nullptr;
    };

private:
    struct BlockInternal {
        Block block;
        // Guarantees pointer stability of values.
        std::unordered_map<NamePool::Id, NodeVal> vars;
        BlockInternal *prev;
    };

    friend class BlockControl;

    // Guarantees pointer stability of values.
    std::unordered_map<NamePool::Id, FuncValue> funcs;
    // Guarantees pointer stability of values.
    std::unordered_map<NamePool::Id, MacroValue> macros;

    BlockInternal *last, *glob;

    bool inFunc;
    FuncValue currFunc;

    void setCurrFunc(const FuncValue &func) { inFunc = true; currFunc = func; }
    void clearCurrFunc() { inFunc = false; }

    void newBlock(Block b);
    void endBlock();

public:
    SymbolTable();

    void addVar(NamePool::Id name, NodeVal var);
    const NodeVal* getVar(NamePool::Id name) const;
    NodeVal* getVar(NamePool::Id name);

    void registerFunc(const FuncValue &val);
    const FuncValue* getFunction(NamePool::Id name) const;

    void registerMacro(const MacroValue &val);
    const MacroValue* getMacro(NamePool::Id name) const;

    bool inGlobalScope() const { return last == glob; }
    const Block* getLastBlock() const { return &last->block; }
    Block* getLastBlock() { return &last->block; }
    const Block* getBlock(NamePool::Id name) const;
    Block* getBlock(NamePool::Id name);

    std::optional<FuncValue> getCurrFunc() const;

    bool isVarName(NamePool::Id name) const { return getVar(name) != nullptr; }
    bool isFuncName(NamePool::Id name) const;
    bool isMacroName(NamePool::Id name) const;
    
    bool nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable) const;

    ~SymbolTable();
};

class BlockControl {
    SymbolTable *symTable;
    bool funcOpen;

public:
    explicit BlockControl(SymbolTable *symTable = nullptr) : symTable(symTable), funcOpen(false) {
        if (symTable != nullptr) symTable->newBlock(SymbolTable::Block());
    }
    BlockControl(SymbolTable *symTable, SymbolTable::Block bo) : symTable(symTable), funcOpen(false) {
        this->symTable->newBlock(bo);
    }
    // ref cuz must not be null
    BlockControl(SymbolTable &symTable, const FuncValue &func) : symTable(&symTable), funcOpen(true) {
        this->symTable->setCurrFunc(func);
        this->symTable->newBlock(SymbolTable::Block());
    }

    BlockControl(const BlockControl&) = delete;
    void operator=(const BlockControl&) = delete;

    BlockControl(const BlockControl&&) = delete;
    void operator=(const BlockControl&&) = delete;

    ~BlockControl() {
        if (symTable) symTable->endBlock();
        if (funcOpen) symTable->clearCurrFunc();
    }
};