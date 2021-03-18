#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <variant>
#include "CodeLoc.h"
#include "NamePool.h"
#include "TypeTable.h"
#include "NodeVal.h"
#include "llvm/IR/Instructions.h"

struct BaseCallableValue {
protected:
    TypeTable::Id type, typeSig;

public:
    CodeLoc codeLoc;
    NamePool::Id name;
    std::vector<NamePool::Id> argNames;

    std::size_t getArgCnt() const { return argNames.size(); }

    TypeTable::Id getType() const { return type; }
    TypeTable::Id getTypeSig() const { return typeSig; }

    virtual ~BaseCallableValue() {}

    static void setType(BaseCallableValue &callable, TypeTable::Id type, TypeTable *typeTable);
    static const TypeTable::Callable& getCallable(const BaseCallableValue &callable, const TypeTable *typeTable);
    static const TypeTable::Callable& getCallableSig(const BaseCallableValue &callable, const TypeTable *typeTable);
};

struct FuncValue : public BaseCallableValue {
    bool noNameMangle = false;
    bool defined = false;

    llvm::Function *llvmFunc = nullptr;
    std::optional<NodeVal> evalFunc;

    bool isLlvm() const { return llvmFunc != nullptr; }
    bool isEval() const { return evalFunc.has_value(); }

    static std::optional<TypeTable::Id> getRetType(const FuncValue &func, const TypeTable *typeTable);
};

struct MacroValue : public BaseCallableValue {
    enum PreHandling {
        REGULAR,
        PREPROC,
        PLUS_ESC
    };

    std::vector<PreHandling> argPreHandling;
    NodeVal body;
    
    static EscapeScore toEscapeScore(PreHandling h);
};

class SymbolTable {
public:
    struct Block {
        std::optional<NamePool::Id> name;
        std::optional<TypeTable::Id> type;
        llvm::BasicBlock *blockExit = nullptr, *blockLoop = nullptr;
        llvm::PHINode *phi = nullptr;

        bool isEval() const { return blockExit == nullptr && blockLoop == nullptr && phi == nullptr; }
    };

    struct VarEntry {
        NodeVal var;
    };

    struct MacroCallSite {
        NamePool::Id name;
        std::size_t argCnt;
    };

    struct CalleeValueInfo {
        bool isFunc;
        bool isLlvm, isEval;
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
    friend class BlockControl;

    // TODO optimize by keeping all vals (including elems in EvalVals) in a single vector (this makes for fewer allocations)
    struct BlockInternal {
        Block block;
        // Guarantees pointer stability of values.
        std::unordered_map<NamePool::Id, VarEntry> vars;
        std::vector<NamePool::Id> varsInOrder;
    };

    // Guarantees pointer stability of values.
    std::unordered_map<NamePool::Id, std::vector<std::unique_ptr<FuncValue>>> funcs;
    // Guarantees pointer stability of values.
    std::unordered_map<NamePool::Id, std::vector<std::unique_ptr<MacroValue>>> macros;

    // Do NOT guarantee pointer stability of blocks.
    std::vector<BlockInternal> globalBlockChain;
    std::vector<std::pair<CalleeValueInfo, std::vector<BlockInternal>>> localBlockChains;

    std::unordered_map<TypeTable::Id, NodeVal, TypeTable::Id::Hasher> dropFuncs;

    void newBlock(Block b);
    void newBlock(const CalleeValueInfo &c);
    void endBlock();

    const BlockInternal* getLastBlockInternal() const;
    BlockInternal* getLastBlockInternal();
    const BlockInternal* getGlobalBlockInternal() const;
    BlockInternal* getGlobalBlockInternal();

    static void collectVarsInRevOrder(BlockInternal *block, std::vector<VarEntry*> &v);

public:
    SymbolTable();

    void addVar(NamePool::Id name, NodeVal val, bool forGlobal = false);
    void addVar(NamePool::Id name, VarEntry var, bool forGlobal = false);
    const VarEntry* getVar(NamePool::Id name) const;
    VarEntry* getVar(NamePool::Id name);

    FuncValue* registerFunc(const FuncValue &val);
    bool isFuncName(NamePool::Id name) const;
    std::vector<const FuncValue*> getFuncs(NamePool::Id name) const;

    MacroValue* registerMacro(const MacroValue &val, const TypeTable *typeTable);
    bool isMacroName(NamePool::Id name) const;
    std::vector<const MacroValue*> getMacros(NamePool::Id name) const;
    const MacroValue* getMacro(MacroCallSite callSite, const TypeTable *typeTable) const;

    void registerDropFunc(TypeTable::Id ty, NodeVal func);
    const NodeVal* getDropFunc(TypeTable::Id ty);

    bool inGlobalScope() const;
    LifetimeInfo::NestLevel currNestLevel() const;
    Block getLastBlock() const;
    std::optional<Block> getBlock(NamePool::Id name) const;

    std::optional<CalleeValueInfo> getCurrCallee() const;

    std::vector<VarEntry*> getVarsInRevOrderCurrBlock();
    std::vector<VarEntry*> getVarsInRevOrderFromBlockToCurrBlock(NamePool::Id name);
    std::vector<VarEntry*> getVarsInRevOrderCurrCallable();

    bool isVarName(NamePool::Id name) const { return getVar(name) != nullptr; }

    bool nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable, bool forGlobal = false) const;
};