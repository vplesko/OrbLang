#include "SymbolTable.h"
#include "utils.h"
using namespace std;

bool FuncSignature::operator==(const FuncSignature &other) const {
    if (name != other.name) return false;
    if (argTypes.size() != other.argTypes.size()) return false;
    for (size_t i = 0; i < argTypes.size(); ++i)
        if (argTypes[i] != other.argTypes[i]) return false;
    
    return true;
}

size_t FuncSignature::Hasher::operator()(const FuncSignature &k) const {
    size_t sum = k.argTypes.size();
    for (const auto &it : k.argTypes) sum += TypeTable::Id::Hasher()(it);
    return leNiceHasheFunctione(k.name, sum);
}

bool MacroSignature::operator==(const MacroSignature &other) const {
    return name == other.name && argCount == other.argCount;
}

size_t MacroSignature::Hasher::operator()(const MacroSignature &k) const {
    return leNiceHasheFunctione(k.name, k.argCount);
}

SymbolTable::SymbolTable(StringPool *stringPool, TypeTable *typeTable)
    : stringPool(stringPool), typeTable(typeTable), inFunc(false) {
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

optional<SymbolTable::VarPayload> SymbolTable::getVar(NamePool::Id name) const {
    for (Scope *s = last; s != nullptr; s = s->prev) {
        auto loc = s->vars.find(name);
        if (loc != s->vars.end()) return loc->second;
    }

    return nullopt;
}

FuncSignature SymbolTable::makeFuncSignature(NamePool::Id name, const std::vector<TypeTable::Id> &argTypes) const {
    FuncSignature sig;
    sig.name = name;
    sig.argTypes = std::vector<TypeTable::Id>(argTypes.size());
    for (size_t i = 0; i < sig.argTypes.size(); ++i)
        sig.argTypes[i] = typeTable->getTypeFuncSigParam(argTypes[i]);
    return sig;
}

optional<FuncSignature> SymbolTable::makeFuncSignature(const FuncCallSite &call) const {
    for (const UntypedVal &it : call.untypedVals) {
        if (it.kind != UntypedVal::Kind::kNone)
            return nullopt;
    }
    return makeFuncSignature(call.name, call.argTypes);
}

bool nonConflicting(const FuncValue &f1, const FuncValue &f2) {
    if (f1.name != f2.name) return true;

    if (f1.defined && f2.defined) return false;
    if (f1.hasRet() != f2.hasRet()) return false;
    if (f1.hasRet() && f1.retType.value() != f2.retType.value()) return false;
    if (f1.variadic != f2.variadic) return false;
    if (f1.noNameMangle != f2.noNameMangle) return false;

    if (f1.argTypes.size() != f2.argTypes.size()) return false;
    // cn is not part of func sig, but all decls and def must agree on this
    for (size_t i = 0; i < f1.argTypes.size(); ++i) {
        if (f1.argTypes[i] != f2.argTypes[i]) return false;
    }

    return true;
}

bool SymbolTable::canRegisterFunc(const FuncValue &val) const {
    if (val.noNameMangle) {
        auto loc = funcsNoNameMangle.find(val.name);
        if (loc != funcsNoNameMangle.end() && !nonConflicting(val, loc->second))
            return false;
    }

    FuncSignature sig = makeFuncSignature(val.name, val.argTypes);
    auto loc = funcs.find(sig);

    if (loc == funcs.end()) return true;

    if (!nonConflicting(val, loc->second)) return false;

    return true;
}

FuncValue SymbolTable::registerFunc(const FuncValue &val) {
    FuncSignature sig = makeFuncSignature(val.name, val.argTypes);
    funcs[sig] = val;
    if (val.noNameMangle) funcsNoNameMangle[val.name] = val;
    return funcs[sig];
}

llvm::Function* SymbolTable::getFunction(const FuncValue &val) const {
    FuncSignature sig = makeFuncSignature(val.name, val.argTypes);
    auto loc = funcs.find(sig);
    if (loc == funcs.end()) return nullptr;
    return loc->second.func;
}

bool SymbolTable::isCallArgsOk(const FuncCallSite &call, const FuncValue &func) const {
    for (size_t i = 0; i < func.argTypes.size(); ++i) {
        // if arg of same or implicitly castable type, we're good
        if (call.untypedVals[i].kind == UntypedVal::Kind::kNone) {
            if (!typeTable->isArgTypeProper(call.argTypes[i], func.argTypes[i]))
                return false;
        } else {
            // otherwise, if untyped val which can be used for this func, we're also good
            switch (call.untypedVals[i].kind) {
            case UntypedVal::Kind::kBool:
                if (!typeTable->worksAsTypeB(func.argTypes[i])) return false;
                break;
            case UntypedVal::Kind::kSint:
                if (!typeTable->worksAsTypeI(func.argTypes[i]) &&
                    !(typeTable->worksAsTypeU(func.argTypes[i]) && call.untypedVals[i].val_si >= 0))
                    return false;
                break;
            case UntypedVal::Kind::kChar:
                if (!typeTable->worksAsTypeC(func.argTypes[i])) return false;
                break;
            case UntypedVal::Kind::kFloat:
                if (!typeTable->worksAsTypeF(func.argTypes[i])) return false;
                break;
            case UntypedVal::Kind::kNull:
                if (!typeTable->worksAsTypeAnyP(func.argTypes[i])) return false;
                break;
            case UntypedVal::Kind::kString:
                {
                    const std::string &str = stringPool->get(call.untypedVals[i].val_str);
                    if (!typeTable->worksAsTypeStr(func.argTypes[i]) &&
                        !typeTable->worksAsTypeCharArrOfLen(func.argTypes[i], UntypedVal::getStringLen(str)))
                        return false;
                }
                break;
            default:
                return false;
            }
        }
    }

    return true;
}

SymbolTable::FuncForCallPayload SymbolTable::getFuncForCall(const FuncCallSite &call) {
    optional<FuncSignature> sig = makeFuncSignature(call);
    if (sig.has_value()) {
        // if there's a single function and doesn't need casting, return it
        auto loc = funcs.find(sig.value());
        if (loc != funcs.end()) {
            if (isCallArgsOk(call, loc->second)) return FuncForCallPayload(loc->second);
            else return FuncForCallPayload(FuncForCallPayload::kNotFound);
        }
    }

    const FuncSignature *foundSig = nullptr;
    FuncValue *foundVal = nullptr;

    for (auto &it : funcs) {
        // need func with that name
        if (it.second.name != call.name) continue;
        // and that num of args, unless variadic
        if (it.second.argTypes.size() != call.argTypes.size() &&
            (!it.second.variadic || it.second.argTypes.size() > call.argTypes.size())) continue;

        const FuncSignature *candSig = &it.first;
        FuncValue *candVal = &it.second;

        if (!isCallArgsOk(call, *candVal)) {
            candSig = nullptr;
            candVal = nullptr;
        }

        // found no suitable functions
        if (candVal == nullptr) continue;

        // in case of multiple possible funcs, error due to ambiguity
        if (foundVal != nullptr) return FuncForCallPayload(FuncForCallPayload::kAmbigious);
        
        // found a function that fits this description (which may or may not require casts)
        foundSig = candSig;
        foundVal = candVal;
    }

    if (foundVal) return FuncForCallPayload(*foundVal);
    else return FuncForCallPayload(FuncForCallPayload::kNotFound);
}

MacroSignature SymbolTable::makeMacroSignature(const MacroValue &val) const {
    MacroSignature sig;
    sig.name = val.name;
    sig.argCount = val.argNames.size();
    return sig;
}

bool SymbolTable::canRegisterMacro(const MacroValue &val) const {
    MacroSignature sig = makeMacroSignature(val);
    return macros.find(sig) == macros.end();
}

void SymbolTable::registerMacro(const MacroValue &val) {
    MacroSignature sig = makeMacroSignature(val);
    macros[sig] = val;
}

optional<MacroValue> SymbolTable::getMacro(const MacroSignature &sig) const {
    auto loc = macros.find(sig);
    if (loc == macros.end()) return nullopt;
    return loc->second;
}

optional<FuncValue> SymbolTable::getCurrFunc() const {
    if (inFunc) return currFunc;
    else return nullopt;
}

bool SymbolTable::isFuncName(NamePool::Id name) const {
    for (const auto &p : funcs)
        if (p.first.name == name)
            return true;
    
    return false;
}

bool SymbolTable::isMacroName(NamePool::Id name) const {
    for (const auto &p : macros)
        if (p.first.name == name)
            return true;
    
    return false;
}

bool SymbolTable::varMayTakeName(NamePool::Id name) const {
    if (typeTable->isType(name)) return false;

    if (last == glob) {
        // you can have vars with same name as funcs, except in global
        if (isFuncName(name)) return false;
    }

    return last->vars.find(name) == last->vars.end();
}

bool SymbolTable::funcMayTakeName(NamePool::Id name) const {
    return !typeTable->isType(name) && !isMacroName(name) && last->vars.find(name) == last->vars.end();
}

bool SymbolTable::macroMayTakeName(NamePool::Id name) const {
    return !typeTable->isType(name) && !isFuncName(name) && last->vars.find(name) == last->vars.end();
}

SymbolTable::~SymbolTable() {
    while (last != nullptr) {
        Scope *s = last;
        last = last->prev;
        delete s;
    }
}