#include "TypeTable.h"
#include <algorithm>
#include <cassert>
#include <sstream>
using namespace std;

void TypeTable::Tuple::addElement(TypeTable::Id m) {
    elements.push_back(m);
}

bool TypeTable::Tuple::eq(const Tuple &other) const {
    if (elements.size() != other.elements.size()) return false;

    for (size_t i = 0; i < elements.size(); ++i)
        if (elements[i] != other.elements[i]) return false;

    return true;
}

bool TypeTable::TypeDescr::eq(const TypeDescr &other) const {
    if (base != other.base || cn != other.cn || decors.size() != other.decors.size()) return false;
    for (size_t i = 0; i < decors.size(); ++i) {
        if (!decors[i].eq(other.decors[i]) || cns[i] != other.cns[i]) return false;
    }
    return true;
}

void TypeTable::TypeDescr::addDecor(Decor d, bool cn_) {
    bool prevIsCn = cns.empty() ? cn : cns.back();

    decors.push_back(d);
    cns.push_back(false);

    // if all of the elems are cn, the entire arr is cn
    if (cn_ || (prevIsCn && d.type == Decor::D_ARR))
        setLastCn();
}

void TypeTable::TypeDescr::setLastCn() {
    if (cns.empty()) cn = true;
    else cns[cns.size()-1] = true;
}

optional<size_t> TypeTable::DataType::getElemInd(NamePool::Id name) const {
    for (size_t i = 0; i < elements.size(); ++i) {
        if (elements[i].name == name) return i;
    }

    return nullopt;
}

void TypeTable::Callable::setArgTypes(const vector<TypeTable::Id> &argTys) {
    assert(args.size() == argTys.size());

    for (size_t i = 0; i < argTys.size(); ++i)
        args[i].ty = argTys[i];
}

void TypeTable::Callable::setArgNoDrops(const vector<bool> &argNoDrops) {
    assert(args.size() == argNoDrops.size());

    for (size_t i = 0; i < argNoDrops.size(); ++i)
        args[i].noDrop = argNoDrops[i];
}

bool TypeTable::Callable::eq(const Callable &other) const {
    if (isFunc != other.isFunc || getArgCnt() != other.getArgCnt() ||
        retType != other.retType || variadic != other.variadic) return false;

    if (isFunc) {
        for (size_t i = 0; i < getArgCnt(); ++i) {
            if (!args[i].eq(other.args[i])) return false;
        }
    }

    return true;
}

TypeTable::PrimIds TypeTable::shortestFittingPrimTypeI(int64_t x) {
    if (x >= numeric_limits<int8_t>::min() && x <= numeric_limits<int8_t>::max()) return P_I8;
    if (x >= numeric_limits<int16_t>::min() && x <= numeric_limits<int16_t>::max()) return P_I16;
    if (x >= numeric_limits<int32_t>::min() && x <= numeric_limits<int32_t>::max()) return P_I32;
    return P_I64;
}

TypeTable::PrimIds TypeTable::shortestFittingPrimTypeF(double x) {
    return std::isinf(x) || std::isnan(x) || (std::abs(x) <= numeric_limits<float>::max()) ? P_F32 : P_F64;
}

TypeTable::TypeTable() {
    addTypeStr();
}

void TypeTable::addPrimType(NamePool::Id name, PrimIds primId, llvm::Type *type) {
    Id id(Id::kPrim, primId);

    typeIds.insert(make_pair(name, id));
    typeNames.insert(make_pair(id, name));
    primTypes[primId] = type;
}

TypeTable::TypeDescr TypeTable::normalize(const TypeDescr &descr) const {
    if (descr.isEmpty() || !isTypeDescr(descr.base)) return descr;

    TypeDescr normalized = normalize(getTypeDescr(descr.base));
    if (descr.cn) normalized.setLastCn();
    for (size_t i = 0; i < descr.decors.size(); ++i) {
        normalized.addDecor(descr.decors[i], descr.cns[i]);
    }

    return normalized;
}

TypeTable::Id TypeTable::addTypeDescr(TypeDescr typeDescr) {
    if (typeDescr.isEmpty()) return typeDescr.base;

    TypeDescr normalized = normalize(typeDescr);

    Id id;
    id.kind = Id::kDescr;

    for (size_t i = 0; i < typeDescrs.size(); ++i) {
        if (typeDescrs[i].first.eq(normalized)) {
            id.index = i;
            return id;
        }
    }

    id.index = typeDescrs.size();
    typeDescrs.push_back(make_pair(move(normalized), nullptr));
    return id;
}

optional<TypeTable::Id> TypeTable::addTuple(Tuple tup) {
    if (tup.elements.empty()) return nullopt;

    if (tup.elements.size() == 1) return tup.elements[0];

    for (size_t i = 0; i < tuples.size(); ++i) {
        if (tup.eq(tuples[i].first)) {
            Id id;
            id.kind = Id::kTuple;
            id.index = i;
            return id;
        }
    }

    Id id;
    id.kind = Id::kTuple;
    id.index = tuples.size();

    tuples.push_back(make_pair(move(tup), nullptr));

    return id;
}

optional<TypeTable::Id> TypeTable::addExplicitType(ExplicitType c) {
    if (typeIds.find(c.name) != typeIds.end()) return nullopt;

    Id id;
    id.kind = Id::kExplicit;
    id.index = explicitTypes.size();

    typeIds.insert(make_pair(c.name, id));
    typeNames.insert(make_pair(id, c.name));

    explicitTypes.push_back(make_pair(c, nullptr));

    return id;
}

optional<TypeTable::Id> TypeTable::addDataType(DataType data) {
    auto loc = typeIds.find(data.name);
    if (loc != typeIds.end()) {
        if (!isDataType(loc->second)) return nullopt;

        DataType &existing = getDataType(loc->second);
        if (data.defined && existing.defined) return nullopt;

        if (data.defined) {
            existing = data;
        }

        return loc->second;
    } else {
        Id id;
        id.kind = Id::kData;
        id.index = dataTypes.size();

        typeIds.insert(make_pair(data.name, id));
        typeNames.insert(make_pair(id, data.name));

        dataTypes.push_back(make_pair(move(data), nullptr));

        return id;
    }
}

TypeTable::Id TypeTable::addCallable(Callable call) {
    for (size_t i = 0; i < callables.size(); ++i) {
        if (call.eq(callables[i].first)) {
            Id id;
            id.kind = Id::kCallable;
            id.index = i;
            return id;
        }
    }

    Id id;
    id.kind = Id::kCallable;
    id.index = callables.size();

    callables.push_back(make_pair(move(call), nullptr));

    return id;
}

optional<TypeTable::Id> TypeTable::addTypeDerefOf(Id typeId) {
    if (!worksAsTypeP(typeId)) return nullopt;

    if (isTypeDescr(typeId)) {
        const TypeDescr &typeDescr = typeDescrs[typeId.index].first;

        TypeDescr typeDerefDescr(typeDescr.base, typeDescr.cn);
        typeDerefDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end()-1);
        typeDerefDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end()-1);

        return addTypeDescr(move(typeDerefDescr));
    } else if (isExplicitType(typeId)) {
        return addTypeDerefOf(getExplicitType(typeId).type);
    } else {
        return nullopt;
    }
}

optional<TypeTable::Id> TypeTable::addTypeIndexOf(Id typeId) {
    if (!worksAsTypeArrP(typeId) && !worksAsTypeArr(typeId)) return nullopt;

    if (isTypeDescr(typeId)) {
        const TypeDescr &typeDescr = typeDescrs[typeId.index].first;

        TypeDescr typeIndexDescr(typeDescr.base, typeDescr.cn);
        typeIndexDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end()-1);
        typeIndexDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end()-1);

        return addTypeDescr(move(typeIndexDescr));
    } else if (isExplicitType(typeId)) {
        return addTypeIndexOf(getExplicitType(typeId).type);
    } else {
        return nullopt;
    }
}

TypeTable::Id TypeTable::addTypeAddrOf(Id typeId) {
    if (isTypeDescr(typeId)) {
        const TypeDescr &typeDescr = typeDescrs[typeId.index].first;

        TypeDescr typeAddrDescr(typeDescr);
        typeAddrDescr.addDecor({.type=TypeDescr::Decor::D_PTR}, false);

        return addTypeDescr(move(typeAddrDescr));
    } else {
        TypeDescr typeAddrDescr(typeId);
        typeAddrDescr.addDecor({.type=TypeDescr::Decor::D_PTR});

        return addTypeDescr(move(typeAddrDescr));
    }
}

TypeTable::Id TypeTable::addTypeArrOfLenIdOf(Id typeId, std::size_t len) {
    if (isTypeDescr(typeId)) {
        const TypeDescr &typeDescr = typeDescrs[typeId.index].first;

        TypeDescr typeArrDescr(typeDescr.base, typeDescr.cn);
        typeArrDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end());
        typeArrDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end());

        typeArrDescr.addDecor({TypeDescr::Decor::D_ARR, len}, false);
        typeArrDescr.setLastCn();

        return addTypeDescr(move(typeArrDescr));
    } else {
        TypeDescr typeArrDescr(typeId);

        typeArrDescr.addDecor({TypeDescr::Decor::D_ARR, len}, false);
        typeArrDescr.setLastCn();

        return addTypeDescr(move(typeArrDescr));
    }
}

TypeTable::Id TypeTable::addTypeCnOf(Id typeId) {
    if (isDirectCn(typeId)) return typeId;

    if (isTypeDescr(typeId)) {
        const TypeDescr &typeDescr = typeDescrs[typeId.index].first;

        TypeDescr typeCnDescr(typeDescr.base, typeDescr.cn);
        typeCnDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end());
        typeCnDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end());

        typeCnDescr.setLastCn();

        return addTypeDescr(move(typeCnDescr));
    } else {
        TypeDescr typeCnDescr(typeId);

        typeCnDescr.setLastCn();

        return addTypeDescr(move(typeCnDescr));
    }
}

TypeTable::Id TypeTable::addTypeDescrForSig(const TypeDescr &typeDescr) {
    TypeDescr typeDescrSig(typeDescr);

    bool pastRef = false;
    for (size_t i = 0; i < typeDescrSig.cns.size(); ++i) {
        size_t ind = typeDescrSig.cns.size()-1-i;

        if (!pastRef) typeDescrSig.cns[ind] = false;

        if (typeDescrSig.decors[ind].type == TypeDescr::Decor::D_PTR ||
            typeDescrSig.decors[ind].type == TypeDescr::Decor::D_ARR_PTR) pastRef = true;
    }
    if (!pastRef) typeDescrSig.cn = false;

    return addTypeDescr(move(typeDescrSig));
}

TypeTable::Id TypeTable::addTypeDescrForSig(Id t) {
    if (!isTypeDescr(t)) return t;
    return addTypeDescrForSig(getTypeDescr(t));
}

TypeTable::Id TypeTable::addCallableSig(const Callable &call) {
    Callable sig(call);
    for (auto &it : sig.args) {
        if (isTypeDescr(it.ty)) it.ty = addTypeDescrForSig(getTypeDescr(it.ty));
        // noDrop not part of sig
        it.noDrop = false;
    }
    // ret type is not part of sig
    sig.retType.reset();
    return addCallable(sig);
}

TypeTable::Id TypeTable::addCallableSig(Id t) {
    return addCallableSig(*extractCallable(t));
}

void TypeTable::addTypeStr() {
    Id c8Id(Id::kPrim, P_C8);

    TypeDescr typeDescr(c8Id, true);
    typeDescr.addDecor({.type=TypeDescr::Decor::D_ARR_PTR}, false);

    strType = addTypeDescr(move(typeDescr));
}

llvm::Type* TypeTable::getLlvmType(Id id) {
    switch (id.kind) {
    case Id::kPrim: return primTypes[id.index];
    case Id::kTuple: return tuples[id.index].second;
    case Id::kDescr: return typeDescrs[id.index].second;
    case Id::kExplicit: return explicitTypes[id.index].second;
    case Id::kData: return dataTypes[id.index].second;
    case Id::kCallable: return callables[id.index].second;
    default: return nullptr;
    }
}

void TypeTable::setLlvmType(Id id, llvm::Type *type) {
    switch (id.kind) {
    case Id::kPrim: primTypes[id.index] = type; break;
    case Id::kTuple: tuples[id.index].second = type; break;
    case Id::kDescr: typeDescrs[id.index].second = type; break;
    case Id::kExplicit: explicitTypes[id.index].second = type; break;
    case Id::kData: dataTypes[id.index].second = type; break;
    case Id::kCallable: callables[id.index].second = type; break;
    }
}

TypeTable::Id TypeTable::getPrimTypeId(PrimIds id) const {
    return Id(Id::kPrim, (size_t) id);
}

const TypeTable::Tuple& TypeTable::getTuple(Id id) const {
    return tuples[id.index].first;
}

const TypeTable::TypeDescr& TypeTable::getTypeDescr(Id id) const {
    return typeDescrs[id.index].first;
}

const TypeTable::ExplicitType& TypeTable::getExplicitType(Id id) const {
    return explicitTypes[id.index].first;
}

const TypeTable::DataType& TypeTable::getDataType(Id id) const {
    return dataTypes[id.index].first;
}

TypeTable::DataType& TypeTable::getDataType(Id id) {
    return dataTypes[id.index].first;
}

const TypeTable::Callable& TypeTable::getCallable(Id id) const {
    return callables[id.index].first;
}

TypeTable::Id TypeTable::getTypeCharArrOfLenId(std::size_t len) {
    Id c8Id(Id::kPrim, P_C8);
    return addTypeArrOfLenIdOf(c8Id, len);
}

optional<size_t> TypeTable::extractLenOfArr(Id arrTypeId) const {
    TypeTable::Id baseTypeId = extractExplicitTypeBaseType(arrTypeId);
    if (!worksAsTypeArr(baseTypeId)) return nullopt;
    return getTypeDescr(baseTypeId).decors.back().len;
}

optional<size_t> TypeTable::extractLenOfTuple(Id tupleTypeId) const {
    TypeTable::Id baseTypeId = extractExplicitTypeBaseType(tupleTypeId);
    if (!isTuple(baseTypeId)) return nullopt;
    return getTuple(baseTypeId).elements.size();
}

optional<size_t> TypeTable::extractLenOfDataType(Id dataTypeId) const {
    TypeTable::Id baseTypeId = extractExplicitTypeBaseType(dataTypeId);
    if (!isDataType(baseTypeId)) return nullopt;
    return getDataType(baseTypeId).elements.size();
}

const TypeTable::Callable* TypeTable::extractCallable(Id callTypeId) const {
    TypeTable::Id baseTypeId = extractExplicitTypeBaseType(callTypeId);
    if (!isCallable(baseTypeId)) return nullptr;
    return &getCallable(baseTypeId);
}

TypeTable::Id TypeTable::extractBaseType(Id t) const {
    if (isExplicitType(t)) return extractBaseType(getExplicitType(t).type);
    if (isTypeDescr(t)) return extractBaseType(getTypeDescr(t).base);
    return t;
}

TypeTable::Id TypeTable::extractExplicitTypeBaseType(Id t) const {
    if (isExplicitType(t)) return extractExplicitTypeBaseType(getExplicitType(t).type);
    if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return t;
        return extractExplicitTypeBaseType(descr.base);
    }
    return t;
}

bool TypeTable::isValidType(Id t) const {
    switch (t.kind) {
    case Id::kPrim: return t.index < primTypes.size();
    case Id::kTuple: return t.index < tuples.size();
    case Id::kDescr: return t.index < typeDescrs.size();
    case Id::kExplicit: return t.index < explicitTypes.size();
    case Id::kData: return t.index < dataTypes.size();
    case Id::kCallable: return t.index < callables.size();
    default: return false;
    }
}

bool TypeTable::isType(NamePool::Id name) const {
    return typeIds.find(name) != typeIds.end();
}

optional<TypeTable::Id> TypeTable::getTypeId(NamePool::Id name) const {
    auto loc = typeIds.find(name);
    if (loc == typeIds.end()) return nullopt;
    return loc->second;
}

optional<NamePool::Id> TypeTable::getTypeName(Id t) const {
    auto loc = typeNames.find(t);
    if (loc == typeNames.end()) return nullopt;
    return loc->second;
}

bool TypeTable::worksAsPrimitive(Id t) const {
    assert(isValidType(t));

    if (isPrimitive(t)) {
        return true;
    } else if (isExplicitType(t)) {
        return worksAsPrimitive(getExplicitType(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base);
    } else {
        return false;
    }
}

bool TypeTable::worksAsPrimitive(Id t, PrimIds p) const {
    assert(isValidType(t));

    if (isPrimitive(t)) {
        return t.index == p;
    } else if (isExplicitType(t)) {
        return worksAsPrimitive(getExplicitType(t).type, p);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base, p);
    } else {
        return false;
    }
}

bool TypeTable::worksAsPrimitive(Id t, PrimIds lo, PrimIds hi) const {
    assert(isValidType(t));

    if (isPrimitive(t)) {
        return between((PrimIds) t.index, lo, hi);
    } else if (isExplicitType(t)) {
        return worksAsPrimitive(getExplicitType(t).type, lo, hi);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base, lo, hi);
    } else {
        return false;
    }
}

bool TypeTable::worksAsTuple(Id t) const {
    assert(isValidType(t));

    if (isTuple(t)) {
        return true;
    } else if (isExplicitType(t)) {
        return worksAsTuple(getExplicitType(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsTuple(descr.base);
    } else {
        return false;
    }
}

bool TypeTable::worksAsExplicitType(Id t) const {
    assert(isValidType(t));

    if (isExplicitType(t)) {
        return true;
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsExplicitType(descr.base);
    } else {
        return false;
    }
}

bool TypeTable::worksAsDataType(Id t) const {
    assert(isValidType(t));

    if (isDataType(t)) {
        return true;
    } else if (isExplicitType(t)) {
        return worksAsDataType(getExplicitType(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsDataType(descr.base);
    } else {
        return false;
    }
}

bool TypeTable::worksAsCallable(Id t) const {
    return extractCallable(t) != nullptr;
}

bool TypeTable::worksAsCallable(Id t, bool isFunc) const {
    const Callable *call = extractCallable(t);
    if (call == nullptr) return false;
    return call->isFunc == isFunc;
}

bool TypeTable::worksAsMacroWithArgs(Id t, std::size_t argCnt, bool variadic) const {
    const Callable *call = extractCallable(t);
    if (call == nullptr) return false;
    return !call->isFunc && call->getArgCnt() == argCnt && call->variadic == variadic;
}

bool TypeTable::isPrimitive(Id t) const {
    return t.kind == Id::kPrim && t.index < primTypes.size();
}

bool TypeTable::isTuple(Id t) const {
    return t.kind == Id::kTuple && t.index < tuples.size();
}

bool TypeTable::isTypeDescr(Id t) const {
    return t.kind == Id::kDescr && t.index < typeDescrs.size();
}

bool TypeTable::isExplicitType(Id t) const {
    return t.kind == Id::kExplicit && t.index < explicitTypes.size();
}

bool TypeTable::isDataType(Id t) const {
    return t.kind == Id::kData && t.index < dataTypes.size();
}

bool TypeTable::isCallable(Id t) const {
    return t.kind == Id::kCallable && t.index < callables.size();
}

const TypeTable::Tuple* TypeTable::extractTuple(Id t) const {
    assert(isValidType(t));

    if (isTuple(t)) return &(getTuple(t));

    if (isExplicitType(t)) return extractTuple(getExplicitType(t).type);

    if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);
        if (descr.decors.empty()) return extractTuple(descr.base);
    }

    return nullptr;
}

optional<TypeTable::Id> TypeTable::extractTupleElementType(Id t, size_t ind) {
    const Tuple *tup = extractTuple(t);
    if (tup == nullptr) return nullopt;

    if (ind >= tup->elements.size()) return nullopt;

    Id id = tup->elements[ind];
    // worksAsTypeCn would be incorrect, as that returns true if not direct cn but a different element is cn
    return isDirectCn(t) ? addTypeCnOf(id) : id;
}

const TypeTable::DataType* TypeTable::extractDataType(Id t) const {
    assert(isValidType(t));

    if (isDataType(t)) return &(getDataType(t));

    if (isExplicitType(t)) return extractDataType(getExplicitType(t).type);

    if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);
        if (descr.decors.empty()) return extractDataType(descr.base);
    }

    return nullptr;
}

optional<TypeTable::Id> TypeTable::extractDataTypeElementType(Id t, NamePool::Id elem) {
    const DataType *data = extractDataType(t);
    if (data == nullptr) return nullopt;

    optional<size_t> ind = data->getElemInd(elem);
    if (!ind.has_value()) return nullopt;

    Id id = data->elements[ind.value()].type;
    // worksAsTypeCn would be incorrect, as that returns true if not direct cn but a different element is cn
    return isDirectCn(t) ? addTypeCnOf(id) : id;
}

optional<TypeTable::Id> TypeTable::extractDataTypeElementType(Id t, size_t ind) {
    const DataType *data = extractDataType(t);
    if (data == nullptr) return nullopt;

    Id id = data->elements[ind].type;
    // worksAsTypeCn would be incorrect, as that returns true if not direct cn but a different element is cn
    return isDirectCn(t) ? addTypeCnOf(id) : id;
}

bool TypeTable::worksAsTypeAnyP(Id t) const {
    return worksAsTypeP(t) || worksAsTypePtr(t) || worksAsTypeArrP(t);
}

bool TypeTable::worksAsTypeP(Id t) const {
    assert(isValidType(t));

    if (isExplicitType(t)) {
        return worksAsTypeP(getExplicitType(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &ty = typeDescrs[t.index].first;
        return (ty.decors.empty() && worksAsTypeP(ty.base)) ||
            (!ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_PTR);
    }

    return false;
}

bool TypeTable::worksAsTypePtr(Id t) const {
    if (isPrimitive(t)) {
        return t.index == P_PTR;
    } else if (isExplicitType(t)) {
        return worksAsTypePtr(getExplicitType(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);
        return descr.decors.empty() && worksAsTypePtr(descr.base);
    }

    return false;
}

bool TypeTable::worksAsTypeArr(Id t) const {
    return worksAsTypeDescrSatisfyingCondition(t, [](const TypeDescr &ty) {
        return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR;
    });
}

bool TypeTable::worksAsTypeArrOfLen(Id t, std::size_t len) const {
    return worksAsTypeDescrSatisfyingCondition(t, [=](const TypeDescr &ty) {
        return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR &&
            ty.decors.back().len == len;
    });
}

bool TypeTable::worksAsTypeArrP(Id t) const {
    return worksAsTypeDescrSatisfyingCondition(t, [](const TypeDescr &ty) {
        return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR_PTR;
    });
}

bool TypeTable::worksAsTypeStr(Id t) const {
    return worksAsTypeDescrSatisfyingCondition(t, [this](const TypeDescr &ty) {
        return ty.decors.size() == 1 && ty.decors[0].type == TypeDescr::Decor::D_ARR_PTR &&
            worksAsTypeC(ty.base) && ty.cn;
    });
}

bool TypeTable::worksAsTypeCharArrOfLen(Id t, size_t len) const {
    return worksAsTypeDescrSatisfyingCondition(t, [=](const TypeDescr &ty) {
        return ty.decors.size() == 1 && worksAsTypeC(ty.base) &&
            ty.decors[0].type == TypeDescr::Decor::D_ARR && ty.decors[0].len == len;
    });
}

bool TypeTable::worksAsTypeCn(Id t) const {
    if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);

        if (!descr.cns.empty()) {
            for (size_t i = descr.cns.size()-1;; --i) {
                if (descr.cns[i]) return true;

                if (descr.decors[i].type != TypeDescr::Decor::D_ARR) return false;

                if (i == 0) break;
            }
        }

        if (descr.cn) return true;

        return worksAsTypeCn(descr.base);
    } else if (isTuple(t)) {
        const Tuple &tup = getTuple(t);

        for (auto it : tup.elements) {
            if (worksAsTypeCn(it)) return true;
        }

        return false;
    } else if (isExplicitType(t)) {
        return worksAsTypeCn(getExplicitType(t).type);
    } else {
        return false;
    }
}

bool TypeTable::fitsTypeI(int64_t x, Id t) const {
    if (!worksAsPrimitive(t)) return false;

    PrimIds primId = (PrimIds) extractBaseType(t).index;

    int64_t lo, hi;
    switch (primId) {
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
        // int64_t's max used intentionally, uint64_t's max couldn't fit and literals cannot exceed this value anyway
        hi = (int64_t) numeric_limits<int64_t>::max();
        break;
    default:
        return false;
    }
    return between(x, lo, hi);
}

bool TypeTable::fitsTypeU(uint64_t x, Id t) const {
    if (!worksAsPrimitive(t)) return false;

    PrimIds primId = (PrimIds) extractBaseType(t).index;

    uint64_t hi;
    switch (primId) {
    case TypeTable::P_I8:
        hi = numeric_limits<int8_t>::max();
        break;
    case TypeTable::P_I16:
        hi = numeric_limits<int16_t>::max();
        break;
    case TypeTable::P_I32:
        hi = numeric_limits<int32_t>::max();
        break;
    case TypeTable::P_I64:
        hi = numeric_limits<int64_t>::max();
        break;
    case TypeTable::P_U8:
        hi = numeric_limits<uint8_t>::max();
        break;
    case TypeTable::P_U16:
        hi = numeric_limits<uint16_t>::max();
        break;
    case TypeTable::P_U32:
        hi = numeric_limits<uint32_t>::max();
        break;
    case TypeTable::P_U64:
        hi = numeric_limits<uint64_t>::max();
        break;
    default:
        return false;
    }
    return x <= hi;
}

bool TypeTable::fitsTypeF(double x, Id t) const {
    if (!worksAsPrimitive(t)) return false;

    PrimIds primId = (PrimIds) extractBaseType(t).index;

    if (primId == P_F32) {
        return shortestFittingPrimTypeF(x) == P_F32;
    } else if (primId == P_F64) {
        return true;
    } else {
        return false;
    }
}

TypeTable::Id TypeTable::shortestFittingTypeIId(int64_t x) const {
    return getPrimTypeId(shortestFittingPrimTypeI(x));
}

bool TypeTable::isDirectCn(Id t) const {
    if (isExplicitType(t)) {
        return isDirectCn(getExplicitType(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);

        return descr.cns.empty() ? descr.cn : descr.cns.back();
    } else {
        return false;
    }
}

bool TypeTable::isUndef(Id t) {
    if (worksAsTuple(t)) {
        const Tuple *tup = extractTuple(t);
        for (Id elemTy : tup->elements) {
            if (isUndef(elemTy)) return true;
        }
        return false;
    } else if (worksAsTypeArr(t)) {
        Id elemTy = addTypeIndexOf(t).value();
        return isUndef(elemTy);
    } else if (worksAsDataType(t)) {
        return !extractDataType(t)->defined;
    } else {
        return false;
    }
}

bool TypeTable::isImplicitCastable(Id from, Id into) const {
    // if it's <type> cn, just look at <type>
    if (isTypeDescr(from) && typeDescrs[from.index].first.decors.empty())
        from = typeDescrs[from.index].first.base;
    if (isTypeDescr(into) && typeDescrs[into.index].first.decors.empty())
        into = typeDescrs[into.index].first.base;

    if (from == into) return true;

    if (isPrimitive(from)) {
        if (!isPrimitive(into)) return false;

        PrimIds s = (PrimIds) from.index;
        PrimIds d = (PrimIds) into.index;

        return (s == d ||
            (worksAsTypeI(from) && between(d, s, P_I64)) ||
            (worksAsTypeU(from) && between(d, s, P_U64)) ||
            (worksAsTypeF(from) && between(d, s, P_F64)));
    } else if (isTypeDescr(from)) {
        if (!isTypeDescr(into)) return false;

        const TypeDescr &s = getTypeDescr(from), &d = getTypeDescr(into);

        if (s.decors.size() != d.decors.size()) return false;

        bool pastRef = false;
        for (size_t i = s.decors.size()-1;; --i) {
            if (!s.decors[i].eq(d.decors[i])) return false;
            if (pastRef && s.cns[i] && !d.cns[i]) return false;
            if (d.decors[i].type == TypeDescr::Decor::D_PTR ||
                d.decors[i].type == TypeDescr::Decor::D_ARR_PTR) pastRef = true;

            if (i == 0) break;
        }
        if (s.base != d.base) return false;
        if (pastRef && s.cn && !d.cn) return false;

        return true;
    } else if (isTuple(from)) {
        if (!isTuple(into)) return false;

        const Tuple &s = getTuple(from), &d = getTuple(into);

        if (s.elements.size() != d.elements.size()) return false;
        for (size_t i = 0; i < s.elements.size(); ++i) {
            if (!isImplicitCastable(s.elements[i], d.elements[i])) return false;
        }

        return true;
    } else {
        return false;
    }
}

optional<string> TypeTable::makeBinString(Id t, const NamePool *namePool, bool makeSigTy) {
    Id ty = t;
    if (makeSigTy && isTypeDescr(ty)) ty = addTypeDescrForSig(getTypeDescr(ty));

    stringstream ss;

    if (isPrimitive(ty)) {
        ss << "$p";
        switch (ty.index) {
        case P_BOOL:
            ss << "$bool";
            break;
        case P_I8:
            ss << "$i8";
            break;
        case P_I16:
            ss << "$i16";
            break;
        case P_I32:
            ss << "$i32";
            break;
        case P_I64:
            ss << "$i64";
            break;
        case P_U8:
            ss << "$u8";
            break;
        case P_U16:
            ss << "$u16";
            break;
        case P_U32:
            ss << "$u32";
            break;
        case P_U64:
            ss << "$u64";
            break;
        case P_F32:
            ss << "$f32";
            break;
        case P_F64:
            ss << "$f64";
            break;
        case P_C8:
            ss << "$c8";
            break;
        case P_PTR:
            ss << "$ptr";
            break;
        case P_ID:
            ss << "$id";
            break;
        case P_TYPE:
            ss << "$type";
            break;
        case P_RAW:
            ss << "$raw";
            break;
        default:
            return nullopt;
        }
    } else if (isTuple(ty)) {
        ss << "$t";
        const Tuple &tup = getTuple(ty);
        for (auto it : tup.elements) {
            optional<string> elemStr = makeBinString(it, namePool, false);
            if (!elemStr.has_value()) return nullopt;
            ss << elemStr.value();
        }
    } else if (isExplicitType(ty)) {
        ss << "$c" << "$" << namePool->get(getExplicitType(ty).name);
    } else if (isDataType(ty)) {
        ss << "$s" << "$" << namePool->get(getDataType(ty).name);
    } else if (isTypeDescr(ty)) {
        ss << "$d";
        const TypeDescr &descr = getTypeDescr(ty);
        for (size_t i = 0; i < descr.decors.size(); ++i) {
            size_t ind = descr.decors.size()-1-i;

            if (descr.cns[ind]) ss << "$cn";
            if (descr.decors[ind].type == TypeDescr::Decor::D_ARR) ss << "$arr";
            else if (descr.decors[ind].type == TypeDescr::Decor::D_ARR_PTR) ss << "$[]";
            else if (descr.decors[ind].type == TypeDescr::Decor::D_PTR) ss << "$*";
            else return nullopt;
        }
        if (descr.cn) ss << "$cn";

        // type descrs are always normalized, so the base is never another type descr
        optional<string> baseStr = makeBinString(descr.base, namePool, false);
        if (!baseStr.has_value()) return nullopt;
        ss << baseStr.value();
        return ss.str();
    } else if (isCallable(ty)) {
        Callable callable = getCallable(ty);
        // inside of callable is done as sig unconditionally
        Callable sig = getCallable(addCallableSig(callable));

        ss << (sig.isFunc ? "$f" : "$m");
        ss << "$a" << sig.getArgCnt();
        if (sig.variadic) ss << "+";
        if (sig.isFunc) {
            for (size_t i = 0; i < sig.args.size(); ++i) {
                const auto &arg = sig.args[i];

                optional<string> str = makeBinString(arg.ty, namePool, false);
                if (!str.has_value()) return nullopt;
                ss << str.value();

                // noDrop is not part of sig, so fetch from original
                if (callable.getArgNoDrop(i)) ss << "$!";
            }
        }

        // ret type is not part of sig, so fetch from original
        if (callable.retType.has_value()) {
            optional<string> str = makeBinString(callable.retType.value(), namePool, false);
            if (!str.has_value()) return nullopt;
            ss << "$r" << str.value();
        }
    } else {
        return nullopt;
    }

    return ss.str();
}