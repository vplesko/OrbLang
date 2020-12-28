#include "SymbolTable.h"
#include <cassert>
#include "TypeTable.h"
#include "Reserved.h"
#include "utils.h"
using namespace std;

const TypeTable::Callable& FuncValue::getCallable(const FuncValue &func, const TypeTable *typeTable) {
    const TypeTable::Callable *call = typeTable->extractCallable(func.type);
    assert(call != nullptr);
    return *call;
}

optional<TypeTable::Id> FuncValue::getRetType(const FuncValue &func, const TypeTable *typeTable) {
    return getCallable(func, typeTable).retType;
}

const TypeTable::Callable& MacroValue::getCallable(const MacroValue &macro, const TypeTable *typeTable) {
    const TypeTable::Callable *call = typeTable->extractCallable(macro.type);
    assert(call != nullptr);
    return *call;
}

SymbolTable::CalleeValueInfo SymbolTable::CalleeValueInfo::make(const FuncValue &func, const TypeTable *typeTable) {
    CalleeValueInfo c;
    c.isFunc = true;
    c.isEval = func.isEval();
    const TypeTable::Callable *call = typeTable->extractCallable(func.type);
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

void SymbolTable::addVar(NamePool::Id name, NodeVal var) {
    getLastBlockInternal()->vars.insert(make_pair(name, move(var)));
}

const NodeVal* SymbolTable::getVar(NamePool::Id name) const {
    if (!localBlockChains.empty()) {
        for (auto it = localBlockChains.back().second.rbegin();
            it != localBlockChains.back().second.rend();
            ++it) {
            auto loc = it->vars.find(name);
            if (loc != it->vars.end()) return &loc->second;
        }
    }
    for (auto it = globalBlockChain.rbegin();
        it != globalBlockChain.rend();
        ++it) {
        auto loc = it->vars.find(name);
        if (loc != it->vars.end()) return &loc->second;
    }

    return nullptr;
}

NodeVal* SymbolTable::getVar(NamePool::Id name) {
    return const_cast<NodeVal*>(const_cast<const SymbolTable*>(this)->getVar(name));
}

FuncValue* SymbolTable::registerFunc(const FuncValue &val) {
    auto loc = funcs.find(val.name);

    if (loc == funcs.end()) {
        return &(funcs[val.name] = val);
    }

    if (loc->second.defined && val.defined) return nullptr;
    // TODO! only signature needs to match
    if (loc->second.type != val.type) return nullptr;

    if (val.defined) {
        return &(funcs[val.name] = val);
    } else {
        return &funcs[val.name];
    }
}

const FuncValue* SymbolTable::getFunction(NamePool::Id name) const {
    auto loc = funcs.find(name);
    if (loc == funcs.end()) return nullptr;
    return &loc->second;
}

MacroValue* SymbolTable::registerMacro(const MacroValue &val) {
    return &(macros[val.name] = val);
}

const MacroValue* SymbolTable::getMacro(NamePool::Id name) const {
    auto loc = macros.find(name);
    if (loc == macros.end()) return nullptr;
    return &loc->second;
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

const SymbolTable::Block* SymbolTable::getLastBlock() const {
    return &getLastBlockInternal()->block;
}

SymbolTable::Block* SymbolTable::getLastBlock() {
    return &getLastBlockInternal()->block;
}

const SymbolTable::Block* SymbolTable::getBlock(NamePool::Id name) const {
    if (!localBlockChains.empty()) {
        for (auto it = localBlockChains.back().second.rbegin();
            it != localBlockChains.back().second.rend();
            ++it) {
            if (it->block.name == name) return &it->block;
        }
    }
    for (auto it = globalBlockChain.rbegin();
        it != globalBlockChain.rend();
        ++it) {
        if (it->block.name == name) return &it->block;
    }

    return nullptr;
}

SymbolTable::Block* SymbolTable::getBlock(NamePool::Id name) {
    return const_cast<SymbolTable::Block*>(const_cast<const SymbolTable*>(this)->getBlock(name));
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