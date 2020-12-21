#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <variant>
#include "NamePool.h"
#include "TypeTable.h"
#include "NodeVal.h"
#include "llvm/IR/Instructions.h"

struct FuncValue {
    NamePool::Id name;
    bool variadic;
    std::vector<NamePool::Id> argNames;
    std::vector<TypeTable::Id> argTypes;
    std::optional<TypeTable::Id> retType;
    bool defined;

    llvm::Function *llvmFunc;
    std::optional<NodeVal> evalFunc;

    std::size_t argCnt() const { return argNames.size(); }
    bool hasRet() const { return retType.has_value(); }

    bool isEval() const { return evalFunc.has_value(); }
};

struct MacroValue {
    TypeTable::Id type;

    NamePool::Id name;
    std::vector<NamePool::Id> argNames;
    NodeVal body;

    std::size_t argCnt() const { return argNames.size(); }
};

class SymbolTable {
public:
    struct Block {
        std::optional<NamePool::Id> name;
        std::optional<TypeTable::Id> type;
        std::optional<NodeVal> val;
        llvm::BasicBlock *blockExit = nullptr, *blockLoop = nullptr;
        llvm::PHINode *phi = nullptr;

        bool isEval() const { return blockExit == nullptr && blockLoop == nullptr && phi == nullptr; }
    };

    struct CalleeValueInfo {
        bool isFunc, isEval;
        std::optional<TypeTable::Id> retType;

        explicit CalleeValueInfo(const FuncValue &func) {
            isFunc = true;
            isEval = func.isEval();
            retType = func.retType;
        }

        explicit CalleeValueInfo(const MacroValue &macro) {
            isFunc = false;
            isEval = true;
        }
    };

private:
    struct BlockInternal {
        Block block;
        // Guarantees pointer stability of values.
        std::unordered_map<NamePool::Id, NodeVal> vars;
    };

    friend class BlockControl;

    // Guarantees pointer stability of values.
    std::unordered_map<NamePool::Id, FuncValue> funcs;
    // Guarantees pointer stability of values.
    std::unordered_map<NamePool::Id, MacroValue> macros;

    // Do NOT guarantee pointer stability of blocks.
    std::vector<BlockInternal> globalBlockChain;
    std::vector<std::pair<CalleeValueInfo, std::vector<BlockInternal>>> localBlockChains;

    void newBlock(Block b);
    void newBlock(const FuncValue &f);
    void newBlock(const MacroValue &m);
    void endBlock();

    const BlockInternal* getLastBlockInternal() const;
    BlockInternal* getLastBlockInternal();

public:
    SymbolTable();

    void addVar(NamePool::Id name, NodeVal var);
    const NodeVal* getVar(NamePool::Id name) const;
    NodeVal* getVar(NamePool::Id name);

    FuncValue* registerFunc(const FuncValue &val);
    const FuncValue* getFunction(NamePool::Id name) const;

    MacroValue* registerMacro(const MacroValue &val);
    const MacroValue* getMacro(NamePool::Id name) const;

    bool inGlobalScope() const;
    const Block* getLastBlock() const;
    Block* getLastBlock();
    const Block* getBlock(NamePool::Id name) const;
    Block* getBlock(NamePool::Id name);

    std::optional<CalleeValueInfo> getCurrCallee() const;

    bool isVarName(NamePool::Id name) const { return getVar(name) != nullptr; }
    bool isFuncName(NamePool::Id name) const;

    bool nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable) const;
};

class BlockControl {
    SymbolTable *symTable;

public:
    explicit BlockControl(SymbolTable *symTable = nullptr) : symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(SymbolTable::Block());
    }
    BlockControl(SymbolTable *symTable, SymbolTable::Block bo) : symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(bo);
    }
    // ref cuz must not be null
    BlockControl(SymbolTable *symTable, const FuncValue &func) : symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(func);
    }
    // ref cuz must not be null
    BlockControl(SymbolTable *symTable, const MacroValue &macro) : symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(macro);
    }

    BlockControl(const BlockControl&) = delete;
    void operator=(const BlockControl&) = delete;

    BlockControl(const BlockControl&&) = delete;
    void operator=(const BlockControl&&) = delete;

    ~BlockControl() {
        if (symTable != nullptr) symTable->endBlock();
    }
};