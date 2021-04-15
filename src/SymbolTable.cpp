#include "SymbolTable.h"
#include <cassert>
#include "Reserved.h"
#include "TypeTable.h"
#include "utils.h"
using namespace std;

void BaseCallableValue::setType(BaseCallableValue &callable, TypeTable::Id type, TypeTable *typeTable) {
    callable.type = type;
    callable.typeSig = typeTable->addCallableSig(getCallable(callable, typeTable));
}

TypeTable::Callable BaseCallableValue::getCallable(const BaseCallableValue &callable, const TypeTable *typeTable) {
    const TypeTable::Callable *call = typeTable->extractCallable(callable.type);
    assert(call != nullptr);
    return *call;
}

TypeTable::Callable BaseCallableValue::getCallableSig(const BaseCallableValue &callable, const TypeTable *typeTable) {
    const TypeTable::Callable *call = typeTable->extractCallable(callable.typeSig);
    assert(call != nullptr);
    return *call;
}

optional<TypeTable::Id> FuncValue::getRetType(const FuncValue &func, const TypeTable *typeTable) {
    return getCallable(func, typeTable).retType;
}

EscapeScore MacroValue::toEscapeScore(PreHandling h) {
    switch (h) {
    case MacroValue::PREPROC: return 0;
    case MacroValue::PLUS_ESC: return 2;
    default: return 1;
    }
}

SymbolTable::CalleeValueInfo SymbolTable::CalleeValueInfo::make(const FuncValue &func, const TypeTable *typeTable) {
    CalleeValueInfo c;
    c.isFunc = true;
    c.isLlvm = func.isLlvm();
    c.isEval = func.isEval();
    const TypeTable::Callable *call = typeTable->extractCallable(func.getType());
    assert(call != nullptr);
    c.retType = call->retType;
    return c;
}

SymbolTable::SymbolTable() {
    globalBlockChain.push_back(BlockInternal());
}

void SymbolTable::newBlock(Block b) {
    BlockInternal s;
    s.block = b;
    if (localBlockChains.empty()) globalBlockChain.push_back(s);
    else localBlockChains.back().second.push_back(s);
}

void SymbolTable::newBlock(const CalleeValueInfo &c) {
    localBlockChains.push_back(make_pair(c, vector<BlockInternal>(1, BlockInternal())));
}

void SymbolTable::endBlock() {
    if (localBlockChains.empty()) {
        globalBlockChain.pop_back();
    } else {
        localBlockChains.back().second.pop_back();
        if (localBlockChains.back().second.empty())
            localBlockChains.pop_back();
    }
}

VarId SymbolTable::addVar(VarEntry var, bool forGlobal) {
    BlockInternal &block = forGlobal ? getGlobalBlockInternal() : getLastBlockInternal();

    block.vars.push_back(move(var));

    VarId varId;
    if (!forGlobal && !localBlockChains.empty()) {
        varId.callable = localBlockChains.size()-1;
        varId.block = localBlockChains.back().second.size()-1;
        varId.index = localBlockChains.back().second.back().vars.size()-1;
    } else {
        varId.block = globalBlockChain.size()-1;
        varId.index = globalBlockChain.back().vars.size()-1;
    }
    return varId;
}

const SymbolTable::VarEntry& SymbolTable::getVar(VarId varId) const {
    if (varId.callable.has_value()) {
        return localBlockChains[varId.callable.value()].second[varId.block].vars[varId.index];
    } else {
        return globalBlockChain[varId.block].vars[varId.index];
    }
}

SymbolTable::VarEntry& SymbolTable::getVar(VarId varId) {
    if (varId.callable.has_value()) {
        return localBlockChains[varId.callable.value()].second[varId.block].vars[varId.index];
    } else {
        return globalBlockChain[varId.block].vars[varId.index];
    }
}

bool SymbolTable::isVarName(NamePool::Id name) const {
    return getVarId(name).has_value();
}

optional<VarId> SymbolTable::getVarId(NamePool::Id name) const {
    if (!localBlockChains.empty()) {
        for (size_t ind = 0; ind < localBlockChains.back().second.size(); ++ind) {
            size_t i = localBlockChains.back().second.size()-1-ind;
            const BlockInternal &blockInternal = localBlockChains.back().second[i];

            for (size_t ind = 0; ind < blockInternal.vars.size(); ++ind) {
                size_t j = blockInternal.vars.size()-1-ind;

                if (blockInternal.vars[j].name == name) {
                    VarId varId;
                    varId.callable = localBlockChains.size()-1;
                    varId.block = i;
                    varId.index = j;
                    return varId;
                }
            }
        }

        for (size_t ind = 0; ind < globalBlockChain[0].vars.size(); ++ind) {
            size_t i = globalBlockChain[0].vars.size()-1-ind;

            if (globalBlockChain[0].vars[i].name == name) {
                VarId varId;
                varId.block = 0;
                varId.index = i;
                return varId;
            }
        }
    } else {
        for (size_t ind = 0; ind < globalBlockChain.size(); ++ind) {
            size_t i = globalBlockChain.size()-1-ind;
            const BlockInternal &blockInternal = globalBlockChain[i];

            for (size_t ind = 0; ind < blockInternal.vars.size(); ++ind) {
                size_t j = blockInternal.vars.size()-1-ind;

                if (blockInternal.vars[j].name == name) {
                    VarId varId;
                    varId.block = i;
                    varId.index = j;
                    return varId;
                }
            }
        }
    }

    return nullopt;
}

SymbolTable::RegisterCallablePayload SymbolTable::registerFunc(FuncValue val) {
    // cannot combine funcs and macros in overloading
    if (isMacroName(val.name)) {
        RegisterCallablePayload ret;
        ret.kind = RegisterCallablePayload::Kind::kOtherCallableTypeSameName;
        ret.codeLocOther = getMacro(getMacros(val.name).front()).codeLoc;
        return ret;
    }

    // funcs with no name mangling cannot be overloaded (note: variadic funcs are always noNameMangle)
    for (const auto &it : funcs[val.name]) {
        if ((val.noNameMangle || it.noNameMangle) && it.getTypeSig() != val.getTypeSig()) {
            RegisterCallablePayload ret;
            ret.codeLocOther = it.codeLoc;
            ret.kind = RegisterCallablePayload::Kind::kNoNameMangleCollision;
            return ret;
        }
    }

    // find the other decl with the same sig, if exists
    optional<size_t> existing;
    for (size_t i = 0; i < funcs[val.name].size(); ++i) {
        const FuncValue &it = funcs[val.name][i];

        if (val.getTypeSig() == it.getTypeSig()) {
            if (val.getType() != it.getType() || val.noNameMangle != it.noNameMangle || (val.defined && it.defined)) {
                RegisterCallablePayload ret;
                ret.kind = RegisterCallablePayload::Kind::kCollision;
                ret.codeLocOther = it.codeLoc;
                return ret;
            }
            existing = i;
            break;
        }
    }

    FuncId funcId;
    funcId.name = val.name;
    if (!existing.has_value()) {
        // if no decls with same sig, simply add
        funcId.index = funcs[val.name].size();
        funcs[val.name].push_back(move(val));
    } else {
        // otherwise, replace with new one only if definition
        funcId.index = existing.value();
        if (val.defined) funcs[val.name][existing.value()] = move(val);
    }

    RegisterCallablePayload ret;
    ret.kind = RegisterCallablePayload::Kind::kSuccess;
    ret.funcId = funcId;
    return ret;
}

const FuncValue& SymbolTable::getFunc(FuncId funcId) const {
    return funcs.at(funcId.name)[funcId.index];
}

FuncValue& SymbolTable::getFunc(FuncId funcId) {
    return funcs.at(funcId.name)[funcId.index];
}

bool SymbolTable::isFuncName(NamePool::Id name) const {
    return funcs.find(name) != funcs.end();
}

vector<FuncId> SymbolTable::getFuncIds(NamePool::Id name) const {
    if (funcs.find(name) == funcs.end()) return vector<FuncId>();

    vector<FuncId> ret;
    ret.reserve(funcs.at(name).size());
    FuncId funcId;
    funcId.name = name;
    for (size_t i = 0; i < funcs.at(name).size(); ++i) {
        funcId.index = i;
        ret.push_back(funcId);
    }

    return ret;
}

SymbolTable::RegisterCallablePayload SymbolTable::registerMacro(MacroValue val, const TypeTable *typeTable) {
    // cannot combine funcs and macros in overloading
    if (isFuncName(val.name)) {
        RegisterCallablePayload ret;
        ret.kind = RegisterCallablePayload::Kind::kOtherCallableTypeSameName;
        ret.codeLocOther = getFunc(getFuncIds(val.name).front()).codeLoc;
        return ret;
    }

    // cannot have more than one macro with the same sig (macros cannot be declared)
    for (const auto &it : macros[val.name]) {
        if (val.getTypeSig() == it.getTypeSig()) {
            RegisterCallablePayload ret;
            ret.kind = RegisterCallablePayload::Kind::kCollision;
            ret.codeLocOther = it.codeLoc;
            return ret;
        }
    }

    // don't allow ambiguity in variadic args
    TypeTable::Callable call = MacroValue::getCallable(val, typeTable);
    for (const auto &it : macros.at(val.name)) {
        TypeTable::Callable otherCall = MacroValue::getCallable(it, typeTable);

        if ((call.variadic && otherCall.variadic) ||
            (call.variadic && otherCall.getArgCnt() >= call.getArgCnt()-1) ||
            (otherCall.variadic && call.getArgCnt() >= otherCall.getArgCnt()-1)) {
            RegisterCallablePayload ret;
            ret.kind = RegisterCallablePayload::Kind::kVariadicCollision;
            ret.codeLocOther = it.codeLoc;
            return ret;
        }
    }

    MacroId macroId;
    macroId.name = val.name;
    macroId.index = macros[val.name].size();

    macros[val.name].push_back(move(val));

    RegisterCallablePayload ret;
    ret.kind = RegisterCallablePayload::Kind::kSuccess;
    ret.macroId = macroId;
    return ret;
}

const MacroValue& SymbolTable::getMacro(MacroId macroId) const {
    return macros.at(macroId.name)[macroId.index];
}

MacroValue& SymbolTable::getMacro(MacroId macroId) {
    return macros.at(macroId.name)[macroId.index];
}

bool SymbolTable::isMacroName(NamePool::Id name) const {
    return macros.find(name) != macros.end();
}

vector<MacroId> SymbolTable::getMacros(NamePool::Id name) const {
    if (macros.find(name) == macros.end()) return vector<MacroId>();

    vector<MacroId> ret;
    ret.reserve(macros.at(name).size());
    MacroId macroId;
    macroId.name = name;
    for (size_t i = 0; i < macros.at(name).size(); ++i) {
        macroId.index = i;
        ret.push_back(macroId);
    }

    return ret;
}

optional<MacroId> SymbolTable::getMacroId(MacroCallSite callSite, const TypeTable *typeTable) const {
    if (macros.find(callSite.name) == macros.end()) return nullopt;

    for (size_t i = 0; i < macros.at(callSite.name).size(); ++i) {
        const MacroValue &it = macros.at(callSite.name)[i];

        TypeTable::Callable callable = MacroValue::getCallable(it, typeTable);

        if (it.getArgCnt() == callSite.argCnt || (callable.variadic && it.getArgCnt()-1 <= callSite.argCnt)) {
            MacroId macroId;
            macroId.name = callSite.name;
            macroId.index = i;
            return macroId;
        }
    }

    return nullopt;
}

void SymbolTable::registerDataAttrs(TypeTable::Id ty, AttrMap attrs) {
    dataAttrs.insert({ty, move(attrs)});
}

const AttrMap* SymbolTable::getDataAttrs(TypeTable::Id ty) {
    if (dataAttrs.find(ty) == dataAttrs.end()) return nullptr;
    return &dataAttrs.at(ty);
}

void SymbolTable::registerDropFunc(TypeTable::Id ty, NodeVal func) {
    dropFuncs.insert({ty, move(func)});
}

const NodeVal* SymbolTable::getDropFunc(TypeTable::Id ty) {
    if (dropFuncs.find(ty) == dropFuncs.end()) return nullptr;
    return &dropFuncs.at(ty);
}

bool SymbolTable::inGlobalScope() const {
    return localBlockChains.empty() && globalBlockChain.size() == 1;
}

LifetimeInfo::NestLevel SymbolTable::currNestLevel() const {
    LifetimeInfo::NestLevel nestLevel;
    nestLevel.callable = localBlockChains.size();
    if (!localBlockChains.empty()) nestLevel.local = localBlockChains.back().second.size();
    else nestLevel.local = globalBlockChain.size();
    return nestLevel;
}

SymbolTable::Block SymbolTable::getLastBlock() const {
    return getLastBlockInternal().block;
}

optional<SymbolTable::Block> SymbolTable::getBlock(NamePool::Id name) const {
    if (!localBlockChains.empty()) {
        for (auto it = localBlockChains.back().second.rbegin();
            it != localBlockChains.back().second.rend();
            ++it) {
            if (it->block.name == name) return it->block;
        }
    } else {
        for (auto it = globalBlockChain.rbegin();
            it != globalBlockChain.rend();
            ++it) {
            if (it->block.name == name) return it->block;
        }
    }

    return nullopt;
}

optional<SymbolTable::CalleeValueInfo> SymbolTable::getCurrCallee() const {
    if (localBlockChains.empty()) return nullopt;
    return localBlockChains.back().first;
}

vector<variant<VarId, NodeVal>> SymbolTable::getValsForDropCurrBlock() {
    vector<variant<VarId, NodeVal>> ret;

    optional<size_t> callable;
    size_t block;
    if (localBlockChains.empty()) {
        block = globalBlockChain.size()-1;
    } else {
        callable = localBlockChains.size()-1;
        block = localBlockChains.back().second.size()-1;
    }

    collectVarsInRevOrder(callable, block, ret);
    return ret;
}

vector<variant<VarId, NodeVal>> SymbolTable::getValsForDropFromBlockToCurrBlock(NamePool::Id name) {
    vector<variant<VarId, NodeVal>> ret;

    if (!localBlockChains.empty()) {
        for (size_t ind = 0; ind < localBlockChains.back().second.size(); ++ind) {
            size_t i = localBlockChains.back().second.size()-1-ind;

            collectVarsInRevOrder(localBlockChains.size()-1, i, ret);
            if (localBlockChains.back().second[i].block.name == name) return ret;
        }
    } else {
        for (size_t ind = 0; ind < globalBlockChain.size(); ++ind) {
            size_t i = globalBlockChain.size()-1-ind;

            collectVarsInRevOrder(nullopt, i, ret);
            if (globalBlockChain[i].block.name == name) return ret;
        }
    }

    // assumed that block with given name exists
    assert(false);
    return ret;
}

vector<variant<VarId, NodeVal>> SymbolTable::getValsForDropCurrCallable() {
    assert(!localBlockChains.empty());

    vector<variant<VarId, NodeVal>> ret;

    for (size_t ind = 0; ind < localBlockChains.back().second.size(); ++ind) {
        size_t i = localBlockChains.back().second.size()-1-ind;

        collectVarsInRevOrder(localBlockChains.size()-1, i, ret);
    }

    return ret;
}

bool SymbolTable::nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable, bool forGlobal, bool checkAllScopes) const {
    if (isReserved(name) || typeTable->isType(name)) return false;

    if (checkAllScopes) {
        if (isVarName(name)) return false;
    } else {
        const BlockInternal &block = forGlobal ? getGlobalBlockInternal() : getLastBlockInternal();

        for (const auto &it : block.vars) {
            if (it.name == name) return false;
        }
    }

    return true;
}

const SymbolTable::BlockInternal& SymbolTable::getLastBlockInternal() const {
    if (localBlockChains.empty()) {
        return globalBlockChain.back();
    } else {
        return localBlockChains.back().second.back();
    }
}

SymbolTable::BlockInternal& SymbolTable::getLastBlockInternal() {
    if (localBlockChains.empty()) {
        return globalBlockChain.back();
    } else {
        return localBlockChains.back().second.back();
    }
}

const SymbolTable::BlockInternal& SymbolTable::getGlobalBlockInternal() const {
    return globalBlockChain.front();
}

SymbolTable::BlockInternal& SymbolTable::getGlobalBlockInternal() {
    return globalBlockChain.front();
}

// TODO this is all a bit roundabout - find a way to optimize, but keep the API nice
void SymbolTable::collectVarsInRevOrder(optional<std::size_t> callable, size_t block, vector<variant<VarId, NodeVal>> &v) {
    const BlockInternal &blockInternal = callable.has_value() ? localBlockChains[callable.value()].second[block] : globalBlockChain[block];

    v.reserve(v.size()+blockInternal.tmps.size()+blockInternal.vars.size());

    for (size_t ind = 0; ind < blockInternal.tmps.size(); ++ind) {
        size_t i = blockInternal.tmps.size()-1-ind;

        v.push_back(blockInternal.tmps[i]);
    }

    VarId varId;
    varId.callable = callable;
    varId.block = block;
    for (size_t ind = 0; ind < blockInternal.vars.size(); ++ind) {
        size_t i = blockInternal.vars.size()-1-ind;

        varId.index = i;
        v.push_back(varId);
    }
}