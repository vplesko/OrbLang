#include "SymbolTable.h"
using namespace std;

NamePool::NamePool() {
    next = 0;
}

NamePool::Id NamePool::add(const string &name) {
    auto loc = ids.find(name);
    if (loc != ids.end())
        return loc->second;

    ids[name] = next;
    names[next] = name;
    next += 1;
    return next-1;
}

SymbolTable::SymbolTable() {
    last = glob = new Scope();
    glob->prev = nullptr;
}

void SymbolTable::newScope() {
    Scope *s = new Scope();
    s->prev = last;
    last = s;
}

void SymbolTable::endScope() {
    Scope *s = last;
    last = last->prev;
    delete s;
}

void SymbolTable::addVar(NamePool::Id name, llvm::AllocaInst *val) {
    last->vars.insert(make_pair(name, val));
}

llvm::AllocaInst* SymbolTable::getVar(NamePool::Id name) const {
    for (Scope *s = last; s != nullptr; s = s->prev) {
        auto loc = s->vars.find(name);
        if (loc != s->vars.end()) return loc->second;
    }

    return nullptr;
}

void SymbolTable::addFunc(NamePool::Id name, llvm::Function *val) {
    funcs.insert(make_pair(name, val));
}

llvm::Function* SymbolTable::getFunc(NamePool::Id name) const {
    auto loc = funcs.find(name);
    if (loc == funcs.end()) return nullptr;

    return loc->second;
}

bool SymbolTable::taken(NamePool::Id name) const {
    if (last == glob && funcs.find(name) != funcs.end()) return true;
    return last->vars.find(name) != last->vars.end();
}