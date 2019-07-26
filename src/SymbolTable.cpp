#include "SymbolTable.h"

NamePool::NamePool() {
    next = 0;
}

NamePool::Id NamePool::add(const std::string &name) {
    auto loc = ids.find(name);
    if (loc != ids.end())
        return loc->second;

    ids[name] = next;
    names[next] = name;
    next += 1;
    return next-1;
}

llvm::AllocaInst* SymbolTable::getVar(NamePool::Id name) const {
    auto loc = vars.find(name);
    if (loc == vars.end()) return nullptr;

    return loc->second;
}

llvm::Function* SymbolTable::getFunc(NamePool::Id name) const {
    auto loc = funcs.find(name);
    if (loc == funcs.end()) return nullptr;

    return loc->second;
}