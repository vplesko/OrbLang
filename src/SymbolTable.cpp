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

bool FuncSignature::operator==(const FuncSignature &other) const {
    if (name != other.name) return false;
    if (argTypes.size() != other.argTypes.size()) return false;
    for (std::size_t i = 0; i < argTypes.size(); ++i)
        if (argTypes[i] != other.argTypes[i]) return false;
    
    return true;
}

std::size_t FuncSignature::Hasher::operator()(const FuncSignature &k) const {
    // le nice hashe functione
    std::size_t sum = k.argTypes.size();
    for (const auto &it : k.argTypes) sum += it;
    return (17*31+k.name)*31+sum;
}

void TypeTable::addType(TypeId id, llvm::Type *type) {
    types.insert(make_pair(id, type));
}

llvm::Type* TypeTable::getType(TypeId id) {
    auto loc = types.find(id);
    if (loc == types.end()) return nullptr;

    return loc->second;
}

bool TypeTable::isType(NamePool::Id name) const {
    return types.find((TypeId) name) != types.end();
}

void TypeTable::addI64Type(TypeId id, llvm::Type *type) {
    i64Id = id;
    types.insert(make_pair(id, type));
}

llvm::Type* TypeTable::getI64Type() {
    return types.find(i64Id)->second;
}

SymbolTable::SymbolTable(TypeTable *typeTable) : typeTable(typeTable) {
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

void SymbolTable::addVar(NamePool::Id name, const VarPayload &var) {
    last->vars.insert(make_pair(name, var));
}

const SymbolTable::VarPayload* SymbolTable::getVar(NamePool::Id name) const {
    for (Scope *s = last; s != nullptr; s = s->prev) {
        auto loc = s->vars.find(name);
        if (loc != s->vars.end()) return &loc->second;
    }

    return nullptr;
}

void SymbolTable::addFunc(const FuncSignature &sig, const FuncValue &val) {
    funcs.insert(make_pair(sig, val));
}

FuncValue* SymbolTable::getFunc(const FuncSignature &sig) {
    auto loc = funcs.find(sig);
    if (loc == funcs.end()) return nullptr;

    return &loc->second;
}

bool SymbolTable::varNameTaken(NamePool::Id name) const {
    if (typeTable->isType(name)) return true;

    if (last == glob) {
        // you can have vars with same name as funcs, except in global
        for (const auto &p : funcs)
            if (p.first.name == name)
                return true;
    }

    return last->vars.find(name) != last->vars.end();
}

// only checks for name collisions with global vars, NOT with funcs of same sig!
bool SymbolTable::funcNameTaken(NamePool::Id name) const {
    return typeTable->isType(name) || last->vars.find(name) != last->vars.end();
}

SymbolTable::~SymbolTable() {
    while (last != nullptr) {
        Scope *s = last;
        last = last->prev;
        delete s;
    }
}