#include "Values.h"
using namespace std;

NodeVal::NodeVal(Kind k) : kind(k) {
    switch (k) {
    case Kind::kKeyword:
        keyword = Token::T_UNKNOWN;
        break;
    case Kind::kLlvmVal:
        llvmVal = LlvmVal();
        break;
    default:
        // do nothing
        break;
    };
}

optional<int64_t> UntypedVal::getValueI(const UntypedVal &val, const TypeTable *typeTable) {
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_I8)) return val.i8;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_I16)) return val.i16;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_I32)) return val.i32;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_I64)) return val.i64;
    return nullopt;
}

optional<uint64_t> UntypedVal::getValueU(const UntypedVal &val, const TypeTable *typeTable) {
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_U8)) return val.u8;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_U16)) return val.u16;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_U32)) return val.u32;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_U64)) return val.u64;
    return nullopt;
}

optional<double> UntypedVal::getValueF(const UntypedVal &val, const TypeTable *typeTable) {
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_F32)) return val.f32;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_F64)) return val.f64;
    return nullopt;
}

optional<uint64_t> UntypedVal::getValueNonNeg(const UntypedVal &val, const TypeTable *typeTable) {
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

bool UntypedVal::isImplicitCastable(const UntypedVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable) {
    if (typeTable->isImplicitCastable(val.type, t))
        return true;

    optional<int64_t> valI = getValueI(val, typeTable);
    if (valI.has_value()) return typeTable->fitsTypeI(valI.value(), t);

    optional<uint64_t> valU = getValueU(val, typeTable);
    if (valU.has_value()) return typeTable->fitsTypeU(valU.value(), t);
    
    optional<double> valF = getValueF(val, typeTable);
    if (valF.has_value()) return typeTable->fitsTypeF(valF.value(), t);
    
    if (typeTable->worksAsTypeStr(val.type))
        return typeTable->worksAsTypeStr(t) ||
            typeTable->worksAsTypeCharArrOfLen(t, LiteralVal::getStringLen(stringPool->get(val.str)));

    return false;
}