#include "KnownVal.h"
#include "LiteralVal.h"
#include "SymbolTable.h"
using namespace std;

optional<NamePool::Id> KnownVal::getCallableId() const {
    if (!isCallable()) return nullopt;
    return id;
}

KnownVal KnownVal::makeVal(TypeTable::Id t, TypeTable *typeTable) {
    KnownVal knownVal;
    knownVal.type = t;

    if (typeTable->worksAsTuple(t)) {
        const TypeTable::Tuple *tup = typeTable->extractTuple(t).value();
        knownVal.elems.reserve(tup->members.size());
        for (TypeTable::Id membType : tup->members) {
            knownVal.elems.push_back(makeVal(membType, typeTable));
        }
    } else if (typeTable->worksAsTypeArr(t)) {
        size_t len = typeTable->extractLenOfArr(t).value();
        TypeTable::Id elemType = typeTable->addTypeIndexOf(t).value();
        knownVal.elems = vector<KnownVal>(len, makeVal(elemType, typeTable));
    }

    return knownVal;
}

KnownVal KnownVal::copyNoRef(const KnownVal &k) {
    KnownVal knownVal(k);
    knownVal.ref = nullptr;
    return knownVal;
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

bool KnownVal::isArr(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeArr(type.value());
}

bool KnownVal::isTuple(const KnownVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTuple(type.value());
}

bool KnownVal::isNull(const KnownVal &val, const TypeTable *typeTable) {
    if (isStr(val, typeTable)) return !val.str.has_value();
    return isAnyP(val, typeTable);
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

    // if a known val is ptr, it has to be null
    if (typeTable->worksAsTypePtr(val.getType().value()) && typeTable->worksAsTypeAnyP(t)) return true;
    
    if (isStr(val, typeTable) && !isNull(val, typeTable))
        return typeTable->worksAsTypeStr(t) ||
            typeTable->worksAsTypeCharArrOfLen(t, LiteralVal::getStringLen(stringPool->get(val.str.value())));

    return false;
}

bool KnownVal::isCastable(const KnownVal &val, TypeTable::Id dstTypeId, const StringPool *stringPool, const TypeTable *typeTable) {
    TypeTable::Id srcTypeId = val.type.value();

    if (srcTypeId == dstTypeId) return true;

    if (typeTable->worksAsTypeI(srcTypeId)) {
        int64_t x = KnownVal::getValueI(val, typeTable).value();

        return typeTable->worksAsTypeI(dstTypeId) ||
            typeTable->worksAsTypeU(dstTypeId) ||
            typeTable->worksAsTypeF(dstTypeId) ||
            typeTable->worksAsTypeC(dstTypeId) ||
            typeTable->worksAsTypeB(dstTypeId) ||
            (typeTable->worksAsTypeStr(dstTypeId) && x == 0) ||
            (typeTable->worksAsTypeAnyP(dstTypeId) && x == 0);
    } else if (typeTable->worksAsTypeU(srcTypeId)) {
        uint64_t x = KnownVal::getValueU(val, typeTable).value();

        return typeTable->worksAsTypeI(dstTypeId) ||
            typeTable->worksAsTypeU(dstTypeId) ||
            typeTable->worksAsTypeF(dstTypeId) ||
            typeTable->worksAsTypeC(dstTypeId) ||
            typeTable->worksAsTypeB(dstTypeId) ||
            (typeTable->worksAsTypeStr(dstTypeId) && x == 0) ||
            (typeTable->worksAsTypeAnyP(dstTypeId) && x == 0);
    } else if (typeTable->worksAsTypeF(srcTypeId)) {
        return typeTable->worksAsTypeI(dstTypeId) ||
            typeTable->worksAsTypeU(dstTypeId) ||
            typeTable->worksAsTypeF(dstTypeId);
    } else if (typeTable->worksAsTypeC(srcTypeId)) {
        return typeTable->worksAsTypeI(dstTypeId) ||
            typeTable->worksAsTypeU(dstTypeId) ||
            typeTable->worksAsTypeC(dstTypeId) ||
            typeTable->worksAsTypeB(dstTypeId);
    } else if (typeTable->worksAsTypeB(srcTypeId)) {
        return typeTable->worksAsTypeI(dstTypeId) ||
            typeTable->worksAsTypeU(dstTypeId) ||
            typeTable->worksAsTypeB(dstTypeId);
    } else if (typeTable->worksAsTypeStr(srcTypeId)) {
        if (val.str.has_value()) {
            const string &str = stringPool->get(val.str.value());

            return typeTable->worksAsTypeStr(dstTypeId) ||
                typeTable->worksAsTypeB(dstTypeId) ||
                typeTable->worksAsTypeCharArrOfLen(dstTypeId, LiteralVal::getStringLen(str));
        } else {
            // if it's not string, it's null
            return typeTable->worksAsTypeI(dstTypeId) ||
                typeTable->worksAsTypeU(dstTypeId) ||
                typeTable->worksAsTypeB(dstTypeId) ||
                typeTable->worksAsTypeAnyP(dstTypeId);
        }
    } else if (typeTable->worksAsTypeAnyP(srcTypeId)) {
        // it's null
        return typeTable->worksAsTypeI(dstTypeId) ||
            typeTable->worksAsTypeU(dstTypeId) ||
            typeTable->worksAsTypeB(dstTypeId) ||
            typeTable->worksAsTypeAnyP(dstTypeId);
    } else if (typeTable->worksAsTypeArr(srcTypeId) || typeTable->worksAsTuple(srcTypeId) ||
        typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_ID) || typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_TYPE)) {
        // these types are only castable when changing constness
        return typeTable->isImplicitCastable(srcTypeId, dstTypeId);
    }
    
    return false;
}