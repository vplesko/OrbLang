#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <variant>
#include "NamePool.h"
#include "TypeTable.h"
#include "NodeVal.h"
#include "llvm/IR/Instructions.h"

struct BaseCallableValue {
    TypeTable::Id type, typeSig;

    NamePool::Id name;
    std::vector<NamePool::Id> argNames;

    std::size_t getArgCnt() const { return argNames.size(); }

    static void setType(BaseCallableValue &callable, TypeTable::Id type, TypeTable *typeTable);
    static const TypeTable::Callable& getCallable(const BaseCallableValue &callable, const TypeTable *typeTable);
};

struct FuncValue : BaseCallableValue {
    bool noNameMangle = false;
    bool defined = false;

    llvm::Function *llvmFunc;
    std::optional<NodeVal> evalFunc;

    bool isEval() const { return evalFunc.has_value(); }

    static std::optional<TypeTable::Id> getRetType(const FuncValue &func, const TypeTable *typeTable);
};

struct MacroValue : BaseCallableValue {
    std::vector<bool> argPreproc;
    NodeVal body;
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

        static CalleeValueInfo make(const FuncValue &func, const TypeTable *typeTable);

        static CalleeValueInfo make(const MacroValue &macro) {
            CalleeValueInfo c;
            c.isFunc = false;
            c.isEval = true;
            return c;
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
    void newBlock(const CalleeValueInfo &c);
    void endBlock();

    const BlockInternal* getLastBlockInternal() const;
    BlockInternal* getLastBlockInternal();

public:
    SymbolTable();

    void addVar(NamePool::Id name, NodeVal var);
    const NodeVal* getVar(NamePool::Id name) const;
    NodeVal* getVar(NamePool::Id name);

    FuncValue* registerFunc(const FuncValue &val);
    bool isFuncName(NamePool::Id name) const;
    const FuncValue* getFunc(NamePool::Id name) const;

    MacroValue* registerMacro(const MacroValue &val);
    bool isMacroName(NamePool::Id name) const;
    const MacroValue* getMacro(NamePool::Id name) const;

    bool inGlobalScope() const;
    const Block* getLastBlock() const;
    Block* getLastBlock();
    const Block* getBlock(NamePool::Id name) const;
    Block* getBlock(NamePool::Id name);

    std::optional<CalleeValueInfo> getCurrCallee() const;

    bool isVarName(NamePool::Id name) const { return getVar(name) != nullptr; }

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
    BlockControl(SymbolTable *symTable, const SymbolTable::CalleeValueInfo &c) : symTable(symTable) {
        if (symTable != nullptr) symTable->newBlock(c);
    }

    BlockControl(const BlockControl&) = delete;
    void operator=(const BlockControl&) = delete;

    BlockControl(const BlockControl&&) = delete;
    void operator=(const BlockControl&&) = delete;

    ~BlockControl() {
        if (symTable != nullptr) symTable->endBlock();
    }
};