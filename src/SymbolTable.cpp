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

llvm::AllocaInst* SymbolTable::get(NamePool::Id name) {
    auto loc = symbols.find(name);
    if (loc == symbols.end()) return nullptr;

    return loc->second;
}