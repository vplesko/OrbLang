#include "EvalVal.h"
#include "LiteralVal.h"
#include "NodeVal.h"
#include "SymbolTable.h"
using namespace std;

void EvalVal::removeRef() {
    ref = nullptr;
}

optional<VarId> EvalVal::getVarId() const {
    if (holds_alternative<VarId>(ref)) return get<VarId>(ref);
    return nullopt;
}

EvalVal EvalVal::makeVal(TypeTable::Id t, TypeTable *typeTable) {
    EvalVal evalVal;
    evalVal.type = t;

    if (typeTable->worksAsTuple(t)) {
        const TypeTable::Tuple *tup = typeTable->extractTuple(t);
        evalVal.elems.reserve(tup->elements.size());
        for (TypeTable::Id elem : tup->elements) {
            evalVal.elems.push_back(NodeVal(CodeLoc(), makeVal(elem, typeTable)));
        }
    } else if (typeTable->worksAsDataType(t)) {
        const TypeTable::DataType *data = typeTable->extractDataType(t);
        evalVal.elems.reserve(data->elements.size());
        for (const auto &elem : data->elements) {
            evalVal.elems.push_back(NodeVal(CodeLoc(), makeVal(elem.type, typeTable)));
        }
    } else if (typeTable->worksAsTypeArr(t)) {
        size_t len = typeTable->extractLenOfArr(t).value();
        TypeTable::Id elemType = typeTable->addTypeIndexOf(t).value();
        evalVal.elems = vector<NodeVal>(len, NodeVal(CodeLoc(), makeVal(elemType, typeTable)));
    }

    return evalVal;
}

EvalVal EvalVal::makeZero(TypeTable::Id t, NamePool *namePool, TypeTable *typeTable) {
    EvalVal evalVal;
    evalVal.type = t;

    if (typeTable->worksAsTuple(t)) {
        const TypeTable::Tuple *tup = typeTable->extractTuple(t);
        evalVal.elems.reserve(tup->elements.size());
        for (TypeTable::Id elem : tup->elements) {
            evalVal.elems.push_back(NodeVal(CodeLoc(), makeZero(elem, namePool, typeTable)));
        }
    } if (typeTable->worksAsDataType(t)) {
        const TypeTable::DataType *data = typeTable->extractDataType(t);
        evalVal.elems.reserve(data->elements.size());
        for (const auto &elem : data->elements) {
            evalVal.elems.push_back(NodeVal(CodeLoc(), makeZero(elem.type, namePool, typeTable)));
        }
    } else if (typeTable->worksAsTypeArr(t)) {
        size_t len = typeTable->extractLenOfArr(t).value();
        TypeTable::Id elemType = typeTable->addTypeIndexOf(t).value();
        evalVal.elems = vector<NodeVal>(len, NodeVal(CodeLoc(), makeZero(elemType, namePool, typeTable)));
    } else if (typeTable->worksAsTypeStr(t)) {
        // do nothing, std::optional is already nullopt
    } else if (typeTable->worksAsPrimitive(t, TypeTable::P_ID)) {
        evalVal.id = namePool->getMainId();
    } else if (typeTable->worksAsPrimitive(t, TypeTable::P_TYPE)) {
        evalVal.ty = typeTable->getPrimTypeId(TypeTable::P_ID);
    } else {
        // do nothing, zeroing of primitives taken care of in the default ctor
    }

    return evalVal;
}

EvalVal EvalVal::copyNoRef(const EvalVal &k, LifetimeInfo lifetimeInfo) {
    EvalVal evalVal(k);
    evalVal.removeRef();
    evalVal.lifetimeInfo = lifetimeInfo;
    return evalVal;
}

EvalVal EvalVal::moveNoRef(EvalVal &&k, LifetimeInfo lifetimeInfo) {
    k.removeRef();
    k.lifetimeInfo = lifetimeInfo;
    return std::move(k);
}

bool EvalVal::isId(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsPrimitive(type.value(), TypeTable::P_ID);
}

bool EvalVal::isType(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsPrimitive(type.value(), TypeTable::P_TYPE);
}

bool EvalVal::isRaw(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsPrimitive(type.value(), TypeTable::P_RAW);
}

bool EvalVal::isFunc(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsCallable(type.value(), true);
}

bool EvalVal::isMacro(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsCallable(type.value(), false);
}

bool EvalVal::isI(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeI(type.value());
}

bool EvalVal::isU(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeU(type.value());
}

bool EvalVal::isF(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeF(type.value());
}

bool EvalVal::isB(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeB(type.value());
}

bool EvalVal::isC(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeC(type.value());
}

bool EvalVal::isP(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeP(type.value());
}

bool EvalVal::isStr(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeStr(type.value());
}

bool EvalVal::isNonNullStr(const EvalVal &val, const TypeTable *typeTable) {
    return isStr(val, typeTable) && val.str.has_value();
}

bool EvalVal::isAnyP(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeAnyP(type.value());
}

bool EvalVal::isArr(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTypeArr(type.value());
}

bool EvalVal::isTuple(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsTuple(type.value());
}

bool EvalVal::isDataType(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() && typeTable->worksAsDataType(type.value());
}

bool EvalVal::isZero(const EvalVal &val, const TypeTable *typeTable) {
    if (isI(val, typeTable)) return getValueI(val, typeTable).value() == 0;
    if (isU(val, typeTable)) return getValueU(val, typeTable).value() == 0;
    if (isF(val, typeTable)) return getValueF(val, typeTable).value() == 0.0;
    return false;
}

bool EvalVal::isNull(const EvalVal &val, const TypeTable *typeTable) {
    if (isP(val, typeTable)) return isNull(val.p);
    if (isStr(val, typeTable)) return !val.str.has_value();
    return isAnyP(val, typeTable);
}

bool EvalVal::isCallableNoValue(const EvalVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.type;
    return type.has_value() &&
        ((typeTable->worksAsCallable(type.value(), true) && !val.f.has_value()) ||
        (typeTable->worksAsCallable(type.value(), false) && !val.m.has_value()));
}

bool EvalVal::isNull(const Pointer &ptr) {
    return holds_alternative<NodeVal*>(ptr) && get<NodeVal*>(ptr) == nullptr;
}

NodeVal& EvalVal::deref(const Pointer &ptr, SymbolTable *symbolTable) {
    if (holds_alternative<VarId>(ptr)) {
        return symbolTable->getVar(get<VarId>(ptr)).var;
    } else {
        return *get<NodeVal*>(ptr);
    }
}

NodeVal& EvalVal::getPointee(const EvalVal &val, SymbolTable *symbolTable) {
    return deref(val.p, symbolTable);
}

NodeVal& EvalVal::getRefee(const EvalVal &val, SymbolTable *symbolTable) {
    return deref(val.ref, symbolTable);
}

optional<int64_t> EvalVal::getValueI(const EvalVal &val, const TypeTable *typeTable) {
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_I8)) return val.i8;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_I16)) return val.i16;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_I32)) return val.i32;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_I64)) return val.i64;
    return nullopt;
}

optional<uint64_t> EvalVal::getValueU(const EvalVal &val, const TypeTable *typeTable) {
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_U8)) return val.u8;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_U16)) return val.u16;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_U32)) return val.u32;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_U64)) return val.u64;
    return nullopt;
}

optional<double> EvalVal::getValueF(const EvalVal &val, const TypeTable *typeTable) {
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_F32)) return val.f32;
    if (typeTable->worksAsPrimitive(val.type, TypeTable::P_F64)) return val.f64;
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

optional<optional<FuncId>> EvalVal::getValueFunc(const EvalVal &val, const TypeTable *typeTable) {
    if (typeTable->worksAsCallable(val.type, true)) return val.f;
    return nullopt;
}

bool EvalVal::isImplicitCastable(const EvalVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable) {
    if (typeTable->isImplicitCastable(val.type, t))
        return true;

    if (typeTable->worksAsFixedType(val.type) || typeTable->worksAsFixedType(t))
        return false;

    optional<int64_t> valI = getValueI(val, typeTable);
    if (valI.has_value()) return typeTable->fitsTypeI(valI.value(), t);

    optional<uint64_t> valU = getValueU(val, typeTable);
    if (valU.has_value()) return typeTable->fitsTypeU(valU.value(), t);
    
    optional<double> valF = getValueF(val, typeTable);
    if (valF.has_value()) return typeTable->fitsTypeF(valF.value(), t);

    // if an eval val is ptr, it has to be null
    if (typeTable->worksAsTypePtr(val.type) &&
        (typeTable->worksAsTypeAnyP(t) || typeTable->worksAsCallable(t))) return true;
    
    if (isNonNullStr(val, typeTable))
        return typeTable->worksAsTypeStr(t) ||
            typeTable->worksAsTypeCharArrOfLen(t, LiteralVal::getStringLen(stringPool->get(val.str.value())));

    if (typeTable->worksAsTuple(val.type) && typeTable->worksAsTuple(t)) {
        const TypeTable::Tuple &tupSrc = *typeTable->extractTuple(val.type);
        const TypeTable::Tuple &tupDst = *typeTable->extractTuple(t);

        if (tupSrc.elements.size() != tupDst.elements.size()) return false;

        for (size_t i = 0; i < tupSrc.elements.size(); ++i) {
            if (!isImplicitCastable(val.elems[i].getEvalVal(), tupDst.elements[i], stringPool, typeTable)) return false;
        }

        return true;
    }

    return false;
}