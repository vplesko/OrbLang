#include "SymbolTable.h"
#include "Reserved.h"
#include "utils.h"
using namespace std;

SymbolTable::SymbolTable() {
    globalBlockChain.push_back(BlockInternal());
}

void SymbolTable::newBlock(Block b) {
    BlockInternal s;
    s.block = b;
    if (localBlockChains.empty()) globalBlockChain.push_back(s);
    else localBlockChains.back().second.push_back(s);
}

void SymbolTable::newBlock(const FuncValue &f) {
    localBlockChains.push_back(make_pair(CalleeValueInfo(f), vector<BlockInternal>(1, BlockInternal())));
}

void SymbolTable::newBlock(const MacroValue &m) {
    localBlockChains.push_back(make_pair(CalleeValueInfo(m), vector<BlockInternal>(1, BlockInternal())));
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

FuncValue* SymbolTable::registerFunc(const FuncValue &val) {
    return &(funcs[val.name] = val);
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

optional<SymbolTable::CalleeValueInfo> SymbolTable::getCurrCallee() const {
    if (localBlockChains.empty()) return nullopt;
    return localBlockChains.back().first;
}

bool SymbolTable::isFuncName(NamePool::Id name) const {
    return getFunction(name) != nullptr;
}

bool SymbolTable::isMacroName(NamePool::Id name) const {
    return getMacro(name) != nullptr;
}

bool SymbolTable::nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable) const {
    if (isReserved(name) || typeTable->isType(name)) return false;

    if (inGlobalScope()) {
        // you can have vars with same name as funcs, except in global
        if (isFuncName(name) || isMacroName(name)) return false;
    }

    const BlockInternal *last = getLastBlockInternal();
    return last->vars.find(name) == last->vars.end();
}