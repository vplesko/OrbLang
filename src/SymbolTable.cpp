#include "SymbolTable.h"
#include <cassert>
#include "TypeTable.h"
#include "Reserved.h"
#include "utils.h"
using namespace std;

void BaseCallableValue::setType(BaseCallableValue &callable, TypeTable::Id type, TypeTable *typeTable) {
    callable.type = type;
    callable.typeSig = typeTable->addCallableSig(getCallable(callable, typeTable));
}

const TypeTable::Callable& BaseCallableValue::getCallable(const BaseCallableValue &callable, const TypeTable *typeTable) {
    const TypeTable::Callable *call = typeTable->extractCallable(callable.type);
    assert(call != nullptr);
    return *call;
}

const TypeTable::Callable& BaseCallableValue::getCallableSig(const BaseCallableValue &callable, const TypeTable *typeTable) {
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

bool SymbolTable::addVar(NamePool::Id name, NodeVal val) {
    VarEntry varEntry;
    varEntry.var = move(val);
    return addVar(name, move(varEntry));
}

bool SymbolTable::addVar(NamePool::Id name, VarEntry var) {
    BlockInternal *lastBlock = getLastBlockInternal();

    if (lastBlock->dropsOngoing) return false;

    auto loc = lastBlock->vars.insert(make_pair(name, move(var)));
    lastBlock->varsInOrder.push_back(&loc.first->second);

    return true;
}

const SymbolTable::VarEntry* SymbolTable::getVar(NamePool::Id name) const {
    if (!localBlockChains.empty()) {
        for (auto it = localBlockChains.back().second.rbegin();
            it != localBlockChains.back().second.rend();
            ++it) {
            auto loc = it->vars.find(name);
            if (loc != it->vars.end() && !loc->second.dropped) return &loc->second;
        }
    }
    for (auto it = globalBlockChain.rbegin();
        it != globalBlockChain.rend();
        ++it) {
        auto loc = it->vars.find(name);
        if (loc != it->vars.end() && !loc->second.dropped) return &loc->second;
    }

    return nullptr;
}

SymbolTable::VarEntry* SymbolTable::getVar(NamePool::Id name) {
    return const_cast<SymbolTable::VarEntry*>(const_cast<const SymbolTable*>(this)->getVar(name));
}

FuncValue* SymbolTable::registerFunc(const FuncValue &val) {
    // cannot combine funcs and macros in overloading
    if (isMacroName(val.name)) return nullptr;

    if (funcs.find(val.name) == funcs.end()) {
        funcs[val.name] = vector<unique_ptr<FuncValue>>();
    }

    // funcs with no name mangling cannot be overloaded (note: variadic funcs are always noNameMangle)
    for (const auto &it : funcs[val.name]) {
        if ((val.noNameMangle || it->noNameMangle) && it->getTypeSig() != val.getTypeSig()) return nullptr;
    }

    // find the other decl with the same sig, if exists
    auto loc = funcs[val.name].end();
    for (auto it = funcs[val.name].begin(); it != funcs[val.name].end(); ++it) {
        if (val.getTypeSig() == (*it)->getTypeSig()) {
            if (val.getType() != (*it)->getType() || val.noNameMangle != (*it)->noNameMangle || (val.defined && (*it)->defined)) {
                return nullptr;
            }
            loc = it;
            break;
        }
    }

    if (loc == funcs[val.name].end()) {
        // if no decls with same sig, simply add
        funcs[val.name].push_back(make_unique<FuncValue>(val));
        return funcs[val.name].back().get();
    } else {
        // otherwise, replace with new one only if definition
        if (val.defined) *loc = make_unique<FuncValue>(val);
        return (*loc).get();
    }
}

bool SymbolTable::isFuncName(NamePool::Id name) const {
    return funcs.find(name) != funcs.end();
}

vector<const FuncValue*> SymbolTable::getFuncs(NamePool::Id name) const {
    if (funcs.find(name) == funcs.end()) return vector<const FuncValue*>();

    vector<const FuncValue*> ret;
    ret.reserve(funcs.find(name)->second.size());
    for (const auto &it : funcs.find(name)->second) {
        ret.push_back(it.get());
    }

    return ret;
}

MacroValue* SymbolTable::registerMacro(const MacroValue &val, const TypeTable *typeTable) {
    // cannot combine funcs and macros in overloading
    if (isFuncName(val.name)) return nullptr;

    if (macros.find(val.name) == macros.end()) {
        macros[val.name] = vector<unique_ptr<MacroValue>>();
    }

    // cannot have more than one macro with the same sig (macros cannot be declared)
    for (const auto &it : macros[val.name]) {
        if (val.getTypeSig() == it->getTypeSig()) return nullptr;
    }

    // don't allow ambiguity in variadic args
    const TypeTable::Callable &call = MacroValue::getCallable(val, typeTable);
    for (const auto &it : macros[val.name]) {
        const TypeTable::Callable &otherCall = MacroValue::getCallable(*it, typeTable);

        if ((call.variadic && otherCall.variadic) ||
            (call.variadic && otherCall.getArgCnt() >= call.getArgCnt()-1) ||
            (otherCall.variadic && call.getArgCnt() >= otherCall.getArgCnt()-1)) {
            return nullptr;
        }
    }

    macros[val.name].push_back(make_unique<MacroValue>(val));
    return macros[val.name].back().get();
}

bool SymbolTable::isMacroName(NamePool::Id name) const {
    return macros.find(name) != macros.end();
}

vector<const MacroValue*> SymbolTable::getMacros(NamePool::Id name) const {
    if (macros.find(name) == macros.end()) return vector<const MacroValue*>();

    vector<const MacroValue*> ret;
    ret.reserve(macros.find(name)->second.size());
    for (const auto &it : macros.find(name)->second) {
        ret.push_back(it.get());
    }

    return ret;
}

const MacroValue* SymbolTable::getMacro(MacroCallSite callSite, const TypeTable *typeTable) const {
    for (const auto &it : macros.find(callSite.name)->second) {
        const TypeTable::Callable &callable = MacroValue::getCallable(*it, typeTable);

        if (it->getArgCnt() == callSite.argCnt || (callable.variadic && it->getArgCnt()-1 <= callSite.argCnt)) {
            return it.get();
        }
    }

    return nullptr;
}

void SymbolTable::registerDropMacro(TypeTable::Id ty, const MacroValue *mac) {
    dropMacros.insert({ty, mac});
}

const MacroValue* SymbolTable::getDropMacro(TypeTable::Id ty) {
    auto loc = dropMacros.find(ty);
    if (loc == dropMacros.end()) return nullptr;
    return loc->second;
}

bool SymbolTable::inGlobalScope() const {
    return localBlockChains.empty() && globalBlockChain.size() == 1;
}

const SymbolTable::BlockInternal* SymbolTable::getLastBlockInternal() const {
    if (localBlockChains.empty()) {
        return &globalBlockChain.back();
    } else {
        return &localBlockChains.back().second.back();
    }
}

SymbolTable::BlockInternal* SymbolTable::getLastBlockInternal() {
    if (localBlockChains.empty()) {
        return &globalBlockChain.back();
    } else {
        return &localBlockChains.back().second.back();
    }
}

SymbolTable::Block SymbolTable::getLastBlock() const {
    return getLastBlockInternal()->block;
}

optional<SymbolTable::Block> SymbolTable::getBlock(NamePool::Id name) const {
    if (!localBlockChains.empty()) {
        for (auto it = localBlockChains.back().second.rbegin();
            it != localBlockChains.back().second.rend();
            ++it) {
            if (it->block.name == name) return it->block;
        }
    }
    for (auto it = globalBlockChain.rbegin();
        it != globalBlockChain.rend();
        ++it) {
        if (it->block.name == name) return it->block;
    }

    return nullopt;
}

optional<SymbolTable::CalleeValueInfo> SymbolTable::getCurrCallee() const {
    if (localBlockChains.empty()) return nullopt;
    return localBlockChains.back().first;
}

bool SymbolTable::nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable) const {
    if (isReserved(name) || typeTable->isType(name)) return false;

    const BlockInternal *last = getLastBlockInternal();
    return last->vars.find(name) == last->vars.end();
}