#include "TypeTable.h"
#include <sstream>
using namespace std;

void TypeTable::Tuple::addMember(TypeTable::Id m) {
    members.push_back(m);
}

bool TypeTable::Tuple::eq(const Tuple &other) const {
    if (members.size() != other.members.size()) return false;

    for (size_t i = 0; i < members.size(); ++i)
        if (members[i] != other.members[i]) return false;
    
    return true;
}

bool TypeTable::TypeDescr::eq(const TypeDescr &other) const {
    if (base != other.base || cn != other.cn || decors.size() != other.decors.size()) return false;
    for (size_t i = 0; i < decors.size(); ++i) {
        if (!decors[i].eq(other.decors[i]) || cns[i] != other.cns[i]) return false;
    }
    return true;
}

optional<size_t> TypeTable::DataType::getMembInd(NamePool::Id name) const {
    for (size_t i = 0; i < members.size(); ++i) {
        if (members[i].first == name) return i;
    }

    return nullopt;
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

TypeTable::PrimIds TypeTable::shortestFittingPrimTypeI(int64_t x) {
    if (x >= numeric_limits<int8_t>::min() && x <= numeric_limits<int8_t>::max()) return P_I8;
    if (x >= numeric_limits<int16_t>::min() && x <= numeric_limits<int16_t>::max()) return P_I16;
    if (x >= numeric_limits<int32_t>::min() && x <= numeric_limits<int32_t>::max()) return P_I32;
    return P_I64;
}

TypeTable::PrimIds TypeTable::shortestFittingPrimTypeF(double x) {
    if (std::isinf(x) || std::isnan(x)) return P_F32;

    if (std::abs(x) <= numeric_limits<float>::max()) return P_F32;
    return P_F64;
}

TypeTable::TypeTable() {
    addTypeStr();
}

void TypeTable::addPrimType(NamePool::Id name, PrimIds primId, llvm::Type *type) {
    Id id{Id::kPrim, primId};

    typeIds.insert(make_pair(name, id));
    typeNames.insert(make_pair(id, name));
    primTypes[primId] = type;
}

TypeTable::Id TypeTable::addTypeDescr(TypeDescr typeDescr) {
    if (typeDescr.decors.empty() && typeDescr.cn == false)
        return typeDescr.base;
    
    Id id;
    id.kind = Id::kDescr;

    for (size_t i = 0; i < typeDescrs.size(); ++i) {
        if (typeDescrs[i].first.eq(typeDescr)) {
            id.index = i;
            return id;
        }
    }

    id.index = typeDescrs.size();
    typeDescrs.push_back(make_pair(move(typeDescr), nullptr));
    return id;
}

optional<TypeTable::Id> TypeTable::addTuple(Tuple tup) {
    if (tup.members.empty()) return nullopt;

    if (tup.members.size() == 1) return tup.members[0];

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

optional<TypeTable::Id> TypeTable::addCustom(Custom c) {
    if (typeIds.find(c.name) != typeIds.end()) return nullopt;

    Id id;
    id.kind = Id::kCustom;
    id.index = customs.size();

    typeIds.insert(make_pair(c.name, id));
    typeNames.insert(make_pair(id, c.name));

    customs.push_back(make_pair(c, nullptr));

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

optional<TypeTable::Id> TypeTable::addTypeDerefOf(Id typeId) {
    if (!worksAsTypeP(typeId)) return nullopt;

    if (isTypeDescr(typeId)) {
        const TypeDescr &typeDescr = typeDescrs[typeId.index].first;
        
        TypeDescr typeDerefDescr(typeDescr.base, typeDescr.cn);
        typeDerefDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end()-1);
        typeDerefDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end()-1);
        
        return addTypeDescr(move(typeDerefDescr));
    } else if (isCustom(typeId)) {
        return addTypeDerefOf(getCustom(typeId).type);
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
    } else if (isCustom(typeId)) {
        return addTypeIndexOf(getCustom(typeId).type);
    } else {
        return nullopt;
    }
}

TypeTable::Id TypeTable::addTypeAddrOf(Id typeId) {
    if (isTypeDescr(typeId)) {
        const TypeDescr &typeDescr = typeDescrs[typeId.index].first;

        TypeDescr typeAddrDescr(typeDescr.base, typeDescr.cn);
        typeAddrDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.size()+1);
        typeAddrDescr.cns = vector<bool>(typeDescr.cns.size()+1);
        for (size_t i = 0; i < typeDescr.decors.size(); ++i) {
            typeAddrDescr.decors[i] = typeDescr.decors[i];
            typeAddrDescr.cns[i] = typeDescr.cns[i];
        }
        typeAddrDescr.decors.back() = {.type=TypeDescr::Decor::D_PTR};

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

void TypeTable::addTypeStr() {
    Id c8Id{Id::kPrim, P_C8};

    TypeDescr typeDescr(c8Id, true);
    typeDescr.addDecor({.type=TypeDescr::Decor::D_ARR_PTR}, false);

    strType = addTypeDescr(move(typeDescr));
}

llvm::Type* TypeTable::getType(Id id) {
    switch (id.kind) {
    case Id::kPrim: return primTypes[id.index];
    case Id::kTuple: return tuples[id.index].second;
    case Id::kDescr: return typeDescrs[id.index].second;
    case Id::kCustom: return customs[id.index].second;
    case Id::kData: return dataTypes[id.index].second;
    default: return nullptr;
    }
}

void TypeTable::setType(Id id, llvm::Type *type) {
    switch (id.kind) {
    case Id::kPrim: primTypes[id.index] = type; break;
    case Id::kTuple: tuples[id.index].second = type; break;
    case Id::kDescr: typeDescrs[id.index].second = type; break;
    case Id::kCustom: customs[id.index].second = type; break;
    case Id::kData: dataTypes[id.index].second = type; break;
    }
}

llvm::Type* TypeTable::getPrimType(PrimIds id) const {
    return primTypes[id];
}

TypeTable::Id TypeTable::getPrimTypeId(PrimIds id) const {
    return Id{Id::kPrim, (size_t) id};
}

const TypeTable::Tuple& TypeTable::getTuple(Id id) const {
    return tuples[id.index].first;
}

const TypeTable::TypeDescr& TypeTable::getTypeDescr(Id id) const {
    return typeDescrs[id.index].first;
}

const TypeTable::Custom& TypeTable::getCustom(Id id) const {
    return customs[id.index].first;
}

const TypeTable::DataType& TypeTable::getDataType(Id id) const {
    return dataTypes[id.index].first;
}

TypeTable::DataType& TypeTable::getDataType(Id id) {
    return dataTypes[id.index].first;
}

TypeTable::Id TypeTable::getTypeCharArrOfLenId(std::size_t len) {
    Id c8Id{Id::kPrim, P_C8};
    return addTypeArrOfLenIdOf(c8Id, len);
}

optional<size_t> TypeTable::extractLenOfArr(Id arrTypeId) const {
    if (!worksAsTypeArr(arrTypeId)) return nullopt;

    if (isTypeDescr(arrTypeId))
        return getTypeDescr(arrTypeId).decors.back().len;
    else if (isCustom(arrTypeId))
        return extractLenOfArr(getCustom(arrTypeId).type);
    else
        return nullopt;
}

optional<size_t> TypeTable::extractLenOfTuple(Id tupleTypeId) const {
    if (!worksAsTuple(tupleTypeId)) return nullopt;

    if (isTuple(tupleTypeId))
        return getTuple(tupleTypeId).members.size();
    else if (isCustom(tupleTypeId))
        return extractLenOfTuple(getCustom(tupleTypeId).type);
    else
        return nullopt;
}

TypeTable::Id TypeTable::extractBaseType(Id t) const {
    if (isCustom(t)) return extractBaseType(getCustom(t).type);
    if (isTypeDescr(t)) return extractBaseType(getTypeDescr(t).base);
    return t;
}

TypeTable::Id TypeTable::extractCustomBaseType(Id t) const {
    // only pass through customs and cn
    if (isCustom(t)) return extractCustomBaseType(getCustom(t).type);
    if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return t;
        return extractCustomBaseType(descr.base);
    }
    return t;
}

bool TypeTable::isValidType(Id t) const {
    switch (t.kind) {
    case Id::kPrim: return t.index < primTypes.size();
    case Id::kTuple: return t.index < tuples.size();
    case Id::kDescr: return t.index < typeDescrs.size();
    case Id::kCustom: return t.index < customs.size();
    case Id::kData: return t.index < dataTypes.size();
    default: return false;
    }
}

bool TypeTable::isType(NamePool::Id name) const {
    return typeIds.find(name) != typeIds.end();
}

std::optional<TypeTable::Id> TypeTable::getTypeId(NamePool::Id name) const {
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
    if (!isValidType(t)) return false;

    if (isPrimitive(t)) {
        return true;
    } else if (isCustom(t)) {
        return worksAsPrimitive(getCustom(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base);
    } else {
        return false;
    }
}

bool TypeTable::worksAsPrimitive(Id t, PrimIds p) const {
    if (!isValidType(t)) return false;

    if (isPrimitive(t)) {
        return t.index == p;
    } else if (isCustom(t)) {
        return worksAsPrimitive(getCustom(t).type, p);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base, p);
    } else {
        return false;
    }
}

bool TypeTable::worksAsPrimitive(Id t, PrimIds lo, PrimIds hi) const {
    if (!isValidType(t)) return false;

    if (isPrimitive(t)) {
        return between((PrimIds) t.index, lo, hi);
    } else if (isCustom(t)) {
        return worksAsPrimitive(getCustom(t).type, lo, hi);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base, lo, hi);
    } else {
        return false;
    }
}

bool TypeTable::worksAsTuple(Id t) const {
    if (!isValidType(t)) return false;
    
    if (isTuple(t)) {
        return true;
    } else if (isCustom(t)) {
        return worksAsTuple(getCustom(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsTuple(descr.base);
    } else {
        return false;
    }
}

bool TypeTable::worksAsCustom(Id t) const {
    if (!isValidType(t)) return false;
    
    if (isCustom(t)) {
        return true;
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsCustom(descr.base);
    } else {
        return false;
    }
}

bool TypeTable::worksAsDataType(Id t) const {
    if (!isValidType(t)) return false;
    
    if (isDataType(t)) {
        return true;
    } else if (isCustom(t)) {
        return worksAsDataType(getCustom(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsDataType(descr.base);
    } else {
        return false;
    }
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

bool TypeTable::isCustom(Id t) const {
    return t.kind == Id::kCustom && t.index < customs.size();
}

bool TypeTable::isDataType(Id t) const {
    return t.kind == Id::kData && t.index < dataTypes.size();
}

optional<const TypeTable::Tuple*> TypeTable::extractTuple(Id t) const {
    if (!isValidType(t)) return nullopt;

    if (isTuple(t)) return &(getTuple(t));

    if (isCustom(t)) return extractTuple(getCustom(t).type);

    if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);
        if (descr.decors.empty()) return extractTuple(descr.base);
    }

    return nullopt;
}

optional<TypeTable::Id> TypeTable::extractTupleMemberType(Id t, size_t ind) {
    optional<const Tuple*> tup = extractTuple(t);
    if (!tup.has_value()) return nullopt;

    if (ind >= tup.value()->members.size()) return nullopt;

    Id id = tup.value()->members[ind];
    return isDirectCn(t) ? addTypeCnOf(id) : id;
}

optional<const TypeTable::DataType*> TypeTable::extractDataType(Id t) const {
    if (!isValidType(t)) return nullopt;

    if (isDataType(t)) return &(getDataType(t));

    if (isCustom(t)) return extractDataType(getCustom(t).type);

    if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);
        if (descr.decors.empty()) return extractDataType(descr.base);
    }

    return nullopt;
}

optional<TypeTable::Id> TypeTable::extractDataTypeMemberType(Id t, NamePool::Id memb) {
    optional<const DataType*> data = extractDataType(t);
    if (!data.has_value()) return nullopt;

    optional<size_t> ind = data.value()->getMembInd(memb);
    if (!ind.has_value()) return nullopt;

    Id id = data.value()->members[ind.value()].second;
    return isDirectCn(t) ? addTypeCnOf(id) : id;
}

bool TypeTable::worksAsTypeAnyP(Id t) const {
    return worksAsTypeP(t) || worksAsTypePtr(t) || worksAsTypeArrP(t);
}

bool TypeTable::worksAsTypeP(Id t) const {
    if (!isValidType(t)) return false;

    if (isCustom(t)) {
        return worksAsTypeP(getCustom(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &ty = typeDescrs[t.index].first;
        return (ty.decors.empty() && worksAsTypePtr(ty.base)) ||
            (!ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_PTR);
    }

    return false;
}

bool TypeTable::worksAsTypePtr(Id t) const {
    if (isPrimitive(t)) {
        return t.index == P_PTR;
    } else if (isCustom(t)) {
        return worksAsTypePtr(getCustom(t).type);
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

        for (auto it : tup.members) {
            if (worksAsTypeCn(it)) return true;
        }

        return false;
    } else if (isCustom(t)) {
        return worksAsTypeCn(getCustom(t).type);
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
        return std::isinf(x) || std::isnan(x) || (std::abs(x) <= numeric_limits<float>::max());
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
    if (isCustom(t)) {
        return isDirectCn(getCustom(t).type);
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);

        return descr.cns.empty() ? descr.cn : descr.cns.back();
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

        const TypeDescr &s = typeDescrs[from.index].first, &d = typeDescrs[into.index].first;

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
    } else {
        return false;
    }
}

optional<string> TypeTable::makeBinString(Id t, const NamePool *namePool) const {
    // don't forget special delim after custom types are cast safe
    stringstream ss;

    if (isPrimitive(t)) {
        ss << "$p";
        switch (t.index) {
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
    } else if (isTuple(t)) {
        ss << "$t";
        const Tuple &tup = getTuple(t);
        for (auto it : tup.members) {
            optional<string> membStr = makeBinString(it, namePool);
            if (!membStr.has_value()) return nullopt;
            ss << membStr.value();
        }
    } else if (isCustom(t)) {
        ss << "$c" << "$" << namePool->get(getCustom(t).name);
    } else if (isDataType(t)) {
        ss << "$s" << "$" << namePool->get(getDataType(t).name);
    } else if (isTypeDescr(t)) {
        ss << "$d";
        stringstream ss;
        const TypeDescr &descr = getTypeDescr(t);
        for (size_t i = descr.decors.size()-1;; --i) {
            if (descr.cns[i]) ss << "$cn";
            if (descr.decors[i].type == TypeDescr::Decor::D_ARR) ss << "$arr";
            else if (descr.decors[i].type == TypeDescr::Decor::D_ARR_PTR) ss << "$[]";
            else if (descr.decors[i].type == TypeDescr::Decor::D_PTR) ss << "$*";
            else return nullopt;

            if (i == 0) break;
        }
        if (descr.cn) ss << "$cn";
        optional<string> baseStr = makeBinString(descr.base, namePool);
        if (!baseStr.has_value()) return nullopt;
        ss << baseStr.value();
        return ss.str();
    } else {
        return nullopt;
    }

    return ss.str();
}