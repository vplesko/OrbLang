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

bool TypeTable::fitsType(int64_t x, Id t) {
    int64_t lo, hi;
    switch (t) {
        case TypeTable::P_I8:
            lo = (int64_t) numeric_limits<int8_t>::min();
            hi = (int64_t) numeric_limits<int8_t>::max();
            break;
        case TypeTable::P_I16:
            lo = (int64_t) numeric_limits<int16_t>::min();
            hi = (int64_t) numeric_limits<int16_t>::max();
            break;
        case TypeTable::P_I32:
            lo = (int64_t) numeric_limits<int32_t>::min();
            hi = (int64_t) numeric_limits<int32_t>::max();
            break;
        case TypeTable::P_I64:
            lo = (int64_t) numeric_limits<int64_t>::min();
            hi = (int64_t) numeric_limits<int64_t>::max();
            break;
        case TypeTable::P_U8:
            lo = (int64_t) numeric_limits<uint8_t>::min();
            hi = (int64_t) numeric_limits<uint8_t>::max();
            break;
        case TypeTable::P_U16:
            lo = (int64_t) numeric_limits<uint16_t>::min();
            hi = (int64_t) numeric_limits<uint16_t>::max();
            break;
        case TypeTable::P_U32:
            lo = (int64_t) numeric_limits<uint32_t>::min();
            hi = (int64_t) numeric_limits<uint32_t>::max();
            break;
        case TypeTable::P_U64:
            lo = (int64_t) numeric_limits<uint64_t>::min();
            // max of uint64_t wouldn't fit into int64_t
            // this isn't strictly correct, but literals aren't allowed to exceed this value anyway
            hi = (int64_t) numeric_limits<int64_t>::max();
            break;
        default:
            return false;
    }
    return between(x, lo, hi);
}

TypeTable::Id TypeTable::shortestFittingTypeI(int64_t x) {
    if (x >= numeric_limits<int8_t>::min() && x <= numeric_limits<int8_t>::max()) return P_I8;
    if (x >= numeric_limits<int16_t>::min() && x <= numeric_limits<int16_t>::max()) return P_I16;
    if (x >= numeric_limits<int32_t>::min() && x <= numeric_limits<int32_t>::max()) return P_I32;
    return P_I64;
}

TypeTable::TypeTable() : next(P_ENUM_END), types(P_ENUM_END, nullptr) {
}

TypeTable::Id TypeTable::addType(NamePool::Id name, llvm::Type *type) {
    typeIds.insert(make_pair(name, next));
    types.push_back(type);
    return next++;
}

void TypeTable::addPrimType(NamePool::Id name, PrimIds id, llvm::Type *type) {
    typeIds.insert(make_pair(name, id));
    types[id] = type;
}

llvm::Type* TypeTable::getType(Id id) {
    return types[id];
}

bool TypeTable::isType(NamePool::Id name) const {
    return typeIds.find(name) != typeIds.end();
}

SymbolTable::SymbolTable(TypeTable *typeTable) : typeTable(typeTable), currFunc(nullptr) {
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
    if (loc != funcs.end()) return &loc->second;
    else return nullptr;
}

pair<const FuncSignature*, FuncValue*> SymbolTable::getFuncCastsAllowed(const FuncSignature &sig, const LiteralVal *litVals) {
    auto loc = funcs.find(sig);
    // if there's a single function and doesn't need casting, return it
    if (loc != funcs.end()) return {&loc->first, &loc->second};
    // if func with no args and not found, fail (no casting can be applied since no args)
    if (sig.argTypes.empty()) return {nullptr, nullptr};

    const FuncSignature *foundSig = nullptr;
    FuncValue *foundVal = nullptr;

    for (auto &it : funcs) {
        // need func with that name
        if (it.first.name != sig.name) continue;
        // and that num of args
        if (it.first.argTypes.size() != sig.argTypes.size()) continue;

        const FuncSignature *candSig = &it.first;
        FuncValue *candVal = &it.second;

        for (size_t i = 0; i < it.first.argTypes.size(); ++i) {
            // if arg of same or implicitly castable type, we're good
            bool argTypeOk = litVals[i].type == LiteralVal::T_NONE &&
                TypeTable::isImplicitCastable(sig.argTypes[i], it.first.argTypes[i]);

            // otherwise, if literal val which can be used for this func, we're also good
            if (!argTypeOk && litVals[i].type != LiteralVal::T_NONE) {
                switch (litVals[i].type) {
                case LiteralVal::T_BOOL:
                    argTypeOk = it.first.argTypes[i] == TypeTable::P_BOOL;
                    break;
                case LiteralVal::T_SINT:
                    argTypeOk = TypeTable::isTypeI(it.first.argTypes[i]) ||
                        (TypeTable::isTypeU(it.first.argTypes[i]) && litVals[i].val_si >= 0);
                    break;
                case LiteralVal::T_FLOAT:
                    argTypeOk = TypeTable::isTypeF(it.first.argTypes[i]);
                    break;
                default:
                    argTypeOk = false;
                    break;
                }
            }

            if (!argTypeOk) {
                candSig = nullptr;
                candVal = nullptr;
                break;
            }
        }

        // found no suitable functions
        if (candVal == nullptr) continue;

        // in case of multiple possible funcs, error due to ambiguity
        // TODO ret what the exact error was in ret val
        if (foundVal != nullptr) return {nullptr, nullptr};
            
        // found a function that fits this description (which may or may not require casts)
        foundSig = candSig;
        foundVal = candVal;
    }

    return {foundSig, foundVal};
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