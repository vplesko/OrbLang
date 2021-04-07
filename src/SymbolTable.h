#pragma once

// TODO sort the order of all includes
#include <unordered_map>
#include <vector>
#include "CodeLoc.h"
#include "NamePool.h"
#include "NodeVal.h"
#include "SymbolTableIds.h"
#include "TypeTable.h"
#include "llvm/IR/Instructions.h"

// TODO put these in a separate header
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
        NamePool::Id name;
        NodeVal var;
        bool skipDrop = false;
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

    struct RegisterCallablePayload {
        enum class Kind {
            kSuccess,
            kOtherCallableTypeSameName,
            kNoNameMangleCollision,
            kVariadicCollision,
            kCollision
        };

        Kind kind;

        union {
            CodeLoc codeLocOther;
            FuncId funcId;
            MacroId macroId;
        };
    };

private:
    friend class BlockControl;

    // TODO optimize by keeping all vals (including elems in EvalVals) in a single long arena-like vector
    struct BlockInternal {
        Block block;
        std::vector<VarEntry> vars;
    };

    std::unordered_map<NamePool::Id, std::vector<FuncValue>, NamePool::Id::Hasher> funcs;
    std::unordered_map<NamePool::Id, std::vector<MacroValue>, NamePool::Id::Hasher> macros;

    std::vector<BlockInternal> globalBlockChain;
    std::vector<std::pair<CalleeValueInfo, std::vector<BlockInternal>>> localBlockChains;

    std::unordered_map<TypeTable::Id, NodeVal, TypeTable::Id::Hasher> dropFuncs;

    void newBlock(Block b);
    void newBlock(const CalleeValueInfo &c);
    void endBlock();

    const BlockInternal& getLastBlockInternal() const;
    BlockInternal& getLastBlockInternal();
    const BlockInternal& getGlobalBlockInternal() const;
    BlockInternal& getGlobalBlockInternal();

    void collectVarsInRevOrder(std::optional<std::size_t> callable, std::size_t block, std::vector<VarId> &v);

public:
    SymbolTable();

    VarId addVar(VarEntry var, bool forGlobal = false);
    const VarEntry& getVar(VarId varId) const;
    VarEntry& getVar(VarId varId);
    bool isVarName(NamePool::Id name) const;
    std::optional<VarId> getVarId(NamePool::Id name) const;

    RegisterCallablePayload registerFunc(const FuncValue &val);
    const FuncValue& getFunc(FuncId funcId) const;
    FuncValue& getFunc(FuncId funcId);
    bool isFuncName(NamePool::Id name) const;
    std::vector<FuncId> getFuncIds(NamePool::Id name) const;

    RegisterCallablePayload registerMacro(const MacroValue &val, const TypeTable *typeTable);
    const MacroValue& getMacro(MacroId macroId) const;
    MacroValue& getMacro(MacroId macroId);
    bool isMacroName(NamePool::Id name) const;
    std::vector<MacroId> getMacros(NamePool::Id name) const;
    std::optional<MacroId> getMacroId(MacroCallSite callSite, const TypeTable *typeTable) const;

    void registerDropFunc(TypeTable::Id ty, NodeVal func);
    const NodeVal* getDropFunc(TypeTable::Id ty);

    bool inGlobalScope() const;
    LifetimeInfo::NestLevel currNestLevel() const;
    Block getLastBlock() const;
    std::optional<Block> getBlock(NamePool::Id name) const;

    std::optional<CalleeValueInfo> getCurrCallee() const;

    std::vector<VarId> getVarsInRevOrderCurrBlock();
    std::vector<VarId> getVarsInRevOrderFromBlockToCurrBlock(NamePool::Id name);
    std::vector<VarId> getVarsInRevOrderCurrCallable();

    bool nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable, bool forGlobal = false, bool checkAllScopes = false) const;
};