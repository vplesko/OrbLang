#include "SymbolTable.h"
#include "utils.h"
using namespace std;

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

void SymbolTable::addVar(NamePool::Id name, const NodeVal &var) {
    last->vars.insert(make_pair(name, var));
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

llvm::Function* SymbolTable::getFunction(NamePool::Id name) const {
    auto loc = funcs.find(name);
    if (loc == funcs.end()) return nullptr;
    return loc->second.func;
}

void SymbolTable::registerMacro(const MacroValue &val) {
    macros[val.name] = val;
}

optional<MacroValue> SymbolTable::getMacro(NamePool::Id name) const {
    auto loc = macros.find(name);
    if (loc == macros.end()) return nullopt;
    return loc->second;
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
    return getMacro(name).has_value();
}

bool SymbolTable::nameAvailable(NamePool::Id name, const NamePool *namePool, const TypeTable *typeTable) const {
    if (namePool->isReserved(name) || typeTable->isType(name)) return false;

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