#include "KnownVal.h"
#include "LiteralVal.h"
#include "SymbolTable.h"
using namespace std;

optional<NamePool::Id> KnownVal::getCallableId() const {
    if (!isCallable()) return nullopt;
    return id;
}

bool KnownVal::isId(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsPrimitive(type.value(), TypeTable::P_ID);
}

bool KnownVal::isType(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsPrimitive(type.value(), TypeTable::P_TYPE);
}

bool KnownVal::isMacro(const KnownVal &val, const SymbolTable *symbolTable) {
    return val.isCallable() && symbolTable->isMacroName(val.id);
}

bool KnownVal::isFunc(const KnownVal &val, const SymbolTable *symbolTable) {
    return val.isCallable() && symbolTable->isFuncName(val.id);
}

bool KnownVal::isI(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeI(type.value());
}

bool KnownVal::isU(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeU(type.value());
}

bool KnownVal::isF(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeF(type.value());
}

bool KnownVal::isB(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeB(type.value());
}

bool KnownVal::isC(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeC(type.value());
}

bool KnownVal::isStr(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeStr(type.value());
}

bool KnownVal::isAnyP(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeAnyP(type.value());
}

optional<int64_t> KnownVal::getValueI(const KnownVal &val, const TypeTable *typeTable) {
    if (!val.getType().has_value()) return nullopt;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_I8)) return val.i8;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_I16)) return val.i16;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_I32)) return val.i32;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_I64)) return val.i64;
    return nullopt;
}

optional<uint64_t> KnownVal::getValueU(const KnownVal &val, const TypeTable *typeTable) {
    if (!val.getType().has_value()) return nullopt;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_U8)) return val.u8;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_U16)) return val.u16;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_U32)) return val.u32;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_U64)) return val.u64;
    return nullopt;
}

optional<double> KnownVal::getValueF(const KnownVal &val, const TypeTable *typeTable) {
    if (!val.getType().has_value()) return nullopt;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_F32)) return val.f32;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_F64)) return val.f64;
    return nullopt;
}

optional<uint64_t> KnownVal::getValueNonNeg(const KnownVal &val, const TypeTable *typeTable) {
    optional<int64_t> valI = getValueI(val, typeTable);
    if (valI.has_value()) {
        if (valI.value() < 0) return nullopt;
        return valI.value();
    }

    optional<uint64_t> valU = getValueU(val, typeTable);
    if (valU.has_value()) {
        return valU;
    }

    return nullopt;
}

bool KnownVal::isImplicitCastable(const KnownVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable) {
    if (typeTable->isImplicitCastable(val.getType().value(), t))
        return true;

    optional<int64_t> valI = getValueI(val, typeTable);
    if (valI.has_value()) return typeTable->fitsTypeI(valI.value(), t);

    optional<uint64_t> valU = getValueU(val, typeTable);
    if (valU.has_value()) return typeTable->fitsTypeU(valU.value(), t);
    
    optional<double> valF = getValueF(val, typeTable);
    if (valF.has_value()) return typeTable->fitsTypeF(valF.value(), t);
    
    if (typeTable->worksAsTypeStr(val.getType().value()))
        return typeTable->worksAsTypeStr(t) ||
            typeTable->worksAsTypeCharArrOfLen(t, LiteralVal::getStringLen(stringPool->get(val.str)));

    return false;
}