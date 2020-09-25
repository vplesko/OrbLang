#include "SymbolTable.h"
#include "Reserved.h"
#include "utils.h"
using namespace std;

bool FuncValue::sameSignature(const FuncValue &fl, const FuncValue &fr) {
    if (fl.name != fr.name) return false;
    if (fl.variadic != fr.variadic) return false;
    if (fl.argCnt() != fr.argCnt()) return false;
    for (size_t i = 0; i < fl.argCnt(); ++i) {
        if (fl.argTypes[i] != fr.argTypes[i]) return false;
    }
    if (fl.retType != fr.retType) return false;

    return true;
}

SymbolTable::SymbolTable() : inFunc(false) {
    last = glob = new BlockInternal();
    glob->prev = nullptr;
}

void SymbolTable::newBlock(Block b) {
    BlockInternal *s = new BlockInternal();
    s->block = b;
    
    s->prev = last;
    last = s;
}

void SymbolTable::endBlock() {
    BlockInternal *s = last;
    last = last->prev;
    delete s;
}

void SymbolTable::addVar(NamePool::Id name, NodeVal var) {
    last->vars.insert(make_pair(name, move(var)));
}

const NodeVal* SymbolTable::getVar(NamePool::Id name) const {
    for (BlockInternal *s = last; s != nullptr; s = s->prev) {
        auto loc = s->vars.find(name);
        if (loc != s->vars.end()) return &loc->second;
    }

    return nullptr;
}

NodeVal* SymbolTable::getVar(NamePool::Id name) {
    for (BlockInternal *s = last; s != nullptr; s = s->prev) {
        auto loc = s->vars.find(name);
        if (loc != s->vars.end()) return &loc->second;
    }

    return nullptr;
}

void SymbolTable::registerFunc(const FuncValue &val) {
    funcs[val.name] = val;
}

const FuncValue* SymbolTable::getFunction(NamePool::Id name) const {
    auto loc = funcs.find(name);
    if (loc == funcs.end()) return nullptr;
    return &loc->second;
}

void SymbolTable::registerMacro(const MacroValue &val) {
    macros[val.name] = move(val);
}

const MacroValue* SymbolTable::getMacro(NamePool::Id name) const {
    auto loc = macros.find(name);
    if (loc == macros.end()) return nullptr;
    return &loc->second;
}

const SymbolTable::Block* SymbolTable::getBlock(NamePool::Id name) const {
    for (const BlockInternal *s = last; s != nullptr; s = s->prev) {
        if (s->block.name == name) return &s->block;
    }

    return nullptr;
}

SymbolTable::Block* SymbolTable::getBlock(NamePool::Id name) {
    for (BlockInternal *s = last; s != nullptr; s = s->prev) {
        if (s->block.name == name) return &s->block;
    }

    return nullptr;
}

optional<FuncValue> SymbolTable::getCurrFunc() const {
    if (inFunc) return currFunc;
    else return nullopt;
}

bool SymbolTable::isFuncName(NamePool::Id name) const {
    return getFunction(name) != nullptr;
}

bool SymbolTable::isMacroName(NamePool::Id name) const {
    return getMacro(name) != nullptr;
}

bool SymbolTable::nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable) const {
    if (isReserved(name) || typeTable->isType(name)) return false;

    if (last == glob) {
        // you can have vars with same name as funcs, except in global
        if (isFuncName(name) || isMacroName(name)) return false;
    }

    return last->vars.find(name) == last->vars.end();
}

SymbolTable::~SymbolTable() {
    while (last != nullptr) {
        BlockInternal *s = last;
        last = last->prev;
        delete s;
    }
}