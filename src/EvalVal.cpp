#include "EvalVal.h"
#include "LiteralVal.h"
#include "SymbolTable.h"
using namespace std;

optional<NamePool::Id> EvalVal::getCallableId() const {
    if (!isCallable()) return nullopt;
    return id;
}

EvalVal EvalVal::makeVal(TypeTable::Id t, TypeTable *typeTable) {
    EvalVal evalVal;
    evalVal.type = t;

    if (typeTable->worksAsTuple(t)) {
        const TypeTable::Tuple *tup = typeTable->extractTuple(t).value();
        evalVal.elems.reserve(tup->members.size());
        for (TypeTable::Id membType : tup->members) {
            evalVal.elems.push_back(makeVal(membType, typeTable));
        }
    } else if (typeTable->worksAsTypeArr(t)) {
        size_t len = typeTable->extractLenOfArr(t).value();
        TypeTable::Id elemType = typeTable->addTypeIndexOf(t).value();
        evalVal.elems = vector<EvalVal>(len, makeVal(elemType, typeTable));
    }

    return evalVal;
}

EvalVal EvalVal::copyNoRef(const EvalVal &k) {
    EvalVal evalVal(k);
    evalVal.ref = nullptr;
    return evalVal;
}

bool EvalVal::isId(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsPrimitive(type.value(), TypeTable::P_ID);
}

bool EvalVal::isType(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsPrimitive(type.value(), TypeTable::P_TYPE);
}

bool EvalVal::isMacro(const EvalVal &val, const SymbolTable *symbolTable) {
    return val.isCallable() && symbolTable->isMacroName(val.id);
}

bool EvalVal::isFunc(const EvalVal &val, const SymbolTable *symbolTable) {
    return val.isCallable() && symbolTable->isFuncName(val.id);
}

bool EvalVal::isI(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeI(type.value());
}

bool EvalVal::isU(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeU(type.value());
}

bool EvalVal::isF(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeF(type.value());
}

bool EvalVal::isB(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeB(type.value());
}

bool EvalVal::isC(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeC(type.value());
}

bool EvalVal::isStr(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeStr(type.value());
}

bool EvalVal::isAnyP(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeAnyP(type.value());
}

bool EvalVal::isArr(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTypeArr(type.value());
}

bool EvalVal::isTuple(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsTuple(type.value());
}

bool EvalVal::isNull(const EvalVal &val, const TypeTable *typeTable) {
    if (isStr(val, typeTable)) return !val.str.has_value();
    return isAnyP(val, typeTable);
}

optional<int64_t> EvalVal::getValueI(const EvalVal &val, const TypeTable *typeTable) {
    if (!val.getType().has_value()) return nullopt;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_I8)) return val.i8;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_I16)) return val.i16;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_I32)) return val.i32;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_I64)) return val.i64;
    return nullopt;
}

optional<uint64_t> EvalVal::getValueU(const EvalVal &val, const TypeTable *typeTable) {
    if (!val.getType().has_value()) return nullopt;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_U8)) return val.u8;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_U16)) return val.u16;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_U32)) return val.u32;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_U64)) return val.u64;
    return nullopt;
}

optional<double> EvalVal::getValueF(const EvalVal &val, const TypeTable *typeTable) {
    if (!val.getType().has_value()) return nullopt;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_F32)) return val.f32;
    if (typeTable->worksAsPrimitive(val.getType().value(), TypeTable::P_F64)) return val.f64;
    return nullopt;
}

optional<uint64_t> EvalVal::getValueNonNeg(const EvalVal &val, const TypeTable *typeTable) {
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

bool EvalVal::isImplicitCastable(const EvalVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable) {
    if (typeTable->isImplicitCastable(val.getType().value(), t))
        return true;

    optional<int64_t> valI = getValueI(val, typeTable);
    if (valI.has_value()) return typeTable->fitsTypeI(valI.value(), t);

    optional<uint64_t> valU = getValueU(val, typeTable);
    if (valU.has_value()) return typeTable->fitsTypeU(valU.value(), t);
    
    optional<double> valF = getValueF(val, typeTable);
    if (valF.has_value()) return typeTable->fitsTypeF(valF.value(), t);

    // if an eval val is ptr, it has to be null
    if (typeTable->worksAsTypePtr(val.getType().value()) && typeTable->worksAsTypeAnyP(t)) return true;
    
    if (isStr(val, typeTable) && !isNull(val, typeTable))
        return typeTable->worksAsTypeStr(t) ||
            typeTable->worksAsTypeCharArrOfLen(t, LiteralVal::getStringLen(stringPool->get(val.str.value())));

    return false;
}

bool EvalVal::isCastable(const EvalVal &val, TypeTable::Id dstTypeId, const StringPool *stringPool, const TypeTable *typeTable) {
    TypeTable::Id srcTypeId = val.type.value();

    if (srcTypeId == dstTypeId) return true;

    if (typeTable->worksAsTypeI(srcTypeId)) {
        int64_t x = EvalVal::getValueI(val, typeTable).value();

        return typeTable->worksAsTypeI(dstTypeId) ||
            typeTable->worksAsTypeU(dstTypeId) ||
            typeTable->worksAsTypeF(dstTypeId) ||
            typeTable->worksAsTypeC(dstTypeId) ||
            typeTable->worksAsTypeB(dstTypeId) ||
            (typeTable->worksAsTypeStr(dstTypeId) && x == 0) ||
            (typeTable->worksAsTypeAnyP(dstTypeId) && x == 0);
    } else if (typeTable->worksAsTypeU(srcTypeId)) {
        uint64_t x = EvalVal::getValueU(val, typeTable).value();

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