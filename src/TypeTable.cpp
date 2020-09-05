#include "TypeTable.h"
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

TypeTable::Id TypeTable::addTypeDescr(TypeDescr typeDescr, llvm::Type *type) {
    Id id = addTypeDescr(move(typeDescr));
    setType(id, type);
    return id;
}

void TypeTable::addPrimType(NamePool::Id name, PrimIds primId, llvm::Type *type) {
    Id id{Id::kPrim, primId};

    typeIds.insert(make_pair(name, id));
    typeNames.insert(make_pair(id, name));
    primTypes[primId] = type;
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

optional<TypeTable::Id> TypeTable::addTypeDerefOf(Id typeId) {
    // ptr cannot be dereferenced
    if (worksAsTypePtr(typeId) || !worksAsTypeP(typeId)) return nullopt;

    const TypeDescr &typeDescr = typeDescrs[typeId.index].first;
    
    TypeDescr typeDerefDescr(typeDescr.base, typeDescr.cn);
    typeDerefDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end()-1);
    typeDerefDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end()-1);
    
    return addTypeDescr(move(typeDerefDescr));
}

optional<TypeTable::Id> TypeTable::addTypeIndexOf(Id typeId) {
    if (!worksAsTypeArrP(typeId) && !worksAsTypeArr(typeId)) return nullopt;

    const TypeDescr &typeDescr = typeDescrs[typeId.index].first;
    
    TypeDescr typeIndexDescr(typeDescr.base, typeDescr.cn);
    typeIndexDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end()-1);
    typeIndexDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end()-1);
    
    return addTypeDescr(move(typeIndexDescr));
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
    // TODO should it be c8 cn [] cn?
    typeDescr.addDecor({.type=TypeDescr::Decor::D_ARR_PTR}, false);

    strType = addTypeDescr(move(typeDescr));
}

llvm::Type* TypeTable::getType(Id id) {
    switch (id.kind) {
    case Id::kPrim: return primTypes[id.index];
    case Id::kTuple: return tuples[id.index].second;
    case Id::kDescr: return typeDescrs[id.index].second;
    default: return nullptr;
    }
}

void TypeTable::setType(Id id, llvm::Type *type) {
    switch (id.kind) {
    case Id::kPrim: primTypes[id.index] = type; break;
    case Id::kTuple: tuples[id.index].second = type; break;
    case Id::kDescr: typeDescrs[id.index].second = type; break;
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

TypeTable::Id TypeTable::getTypeCharArrOfLenId(std::size_t len) {
    Id c8Id{Id::kPrim, P_C8};
    return addTypeArrOfLenIdOf(c8Id, len);
}

optional<size_t> TypeTable::extractLenOfArr(Id arrTypeId) const {
    if (!worksAsTypeArr(arrTypeId)) return nullopt;
    return typeDescrs[arrTypeId.index].first.decors.back().len;
}

bool TypeTable::isValidType(Id t) const {
    switch (t.kind) {
    case Id::kPrim: return t.index < primTypes.size();
    case Id::kTuple: return t.index < tuples.size();
    case Id::kDescr: return t.index < typeDescrs.size();
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
    } else if (isTuple(t)) {
        return false;
    } else {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base);
    }
}

bool TypeTable::worksAsPrimitive(Id t, PrimIds p) const {
    if (!isValidType(t)) return false;

    if (isPrimitive(t)) {
        return t.index == p;
    } else if (isTuple(t)) {
        return false;
    } else {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base);
    }
}

bool TypeTable::worksAsPrimitive(Id t, PrimIds lo, PrimIds hi) const {
    if (!isValidType(t)) return false;

    if (isPrimitive(t)) {
        return between((PrimIds) t.index, lo, hi);
    } else if (isTuple(t)) {
        return false;
    } else {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsPrimitive(descr.base, lo, hi);
    }
}

bool TypeTable::worksAsTuple(Id t) const {
    if (!isValidType(t)) return false;
    
    if (isPrimitive(t)) {
        return false;
    } else if (isTuple(t)) {
        return true;
    } else {
        const TypeDescr &descr = typeDescrs[t.index].first;
        if (!descr.decors.empty()) return false;
        return worksAsTuple(descr.base);
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

optional<const TypeTable::Tuple*> TypeTable::extractTuple(Id t) const {
    if (!isValidType(t)) return nullopt;

    if (isTuple(t)) return &(getTuple(t));

    if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);

        if (descr.decors.empty() && isTuple(descr.base))
            return &(getTuple(descr.base));
    }

    return nullopt;
}

bool TypeTable::worksAsTypeAnyP(Id t) const {
    return worksAsTypeP(t) || worksAsTypeArrP(t);
}

bool TypeTable::worksAsTypeP(Id t) const {
    if (!isValidType(t)) return false;

    if (worksAsTypePtr(t)) return true;

    if (isTypeDescr(t)) {
        const TypeDescr &ty = typeDescrs[t.index].first;
        return (ty.decors.empty() && worksAsTypePtr(ty.base)) ||
            (!ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_PTR);
    }

    return false;
}

bool TypeTable::worksAsTypePtr(Id t) const {
    if (isPrimitive(t)) {
        return t.index == P_PTR;
    } else if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);
        return descr.decors.empty() && worksAsTypePtr(descr.base);
    }
    
    return false;
}

bool TypeTable::worksAsTypeArr(Id t) const {
    return isTypeDescrSatisfyingCondition(t, [](const TypeDescr &ty) {
        return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR;
    });
}

bool TypeTable::worksAsTypeArrOfLen(Id t, std::size_t len) const {
    return isTypeDescrSatisfyingCondition(t, [=](const TypeDescr &ty) {
        return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR &&
            ty.decors.back().len == len;
    });
}

bool TypeTable::worksAsTypeArrP(Id t) const {
    return isTypeDescrSatisfyingCondition(t, [](const TypeDescr &ty) {
        return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR_PTR;
    });
}

bool TypeTable::worksAsTypeStr(Id t) const {
    return isTypeDescrSatisfyingCondition(t, [this](const TypeDescr &ty) {
        return ty.decors.size() == 1 && ty.decors[0].type == TypeDescr::Decor::D_ARR_PTR &&
            worksAsTypeC(ty.base) && ty.cn;
    });
}

bool TypeTable::worksAsTypeCharArrOfLen(Id t, size_t len) const {
    return isTypeDescrSatisfyingCondition(t, [=](const TypeDescr &ty) {
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
    } else {
        return false;
    }
}

bool TypeTable::isDirectCn(Id t) const {
    if (isTypeDescr(t)) {
        const TypeDescr &descr = getTypeDescr(t);

        return descr.cns.empty() ? descr.cn : descr.cns.back();
    } else {
        return false;
    }
}

bool TypeTable::fitsTypeI(int64_t x, Id t) const {
    if (!worksAsPrimitive(t)) return false;

    PrimIds primId;
    if (isPrimitive(t)) primId = (PrimIds) t.index;
    else primId = (PrimIds) typeDescrs[t.index].first.base.index;
    
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

    PrimIds primId;
    if (isPrimitive(t)) primId = (PrimIds) t.index;
    else primId = (PrimIds) typeDescrs[t.index].first.base.index;
    
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

    PrimIds primId;
    if (isPrimitive(t)) primId = (PrimIds) t.index;
    else primId = (PrimIds) typeDescrs[t.index].first.base.index;

    if (primId == P_F32) {
        return std::isinf(x) || std::isnan(x) || (std::abs(x) <= numeric_limits<float>::max());
    } else {
        return true;
    }
}

TypeTable::Id TypeTable::shortestFittingTypeIId(int64_t x) const {
    return getPrimTypeId(shortestFittingPrimTypeI(x));
}

bool TypeTable::isImplicitCastable(Id from, Id into) const {
    if (from == into) return true;

    // if it's <type> cn, just look at <type>
    if (isTypeDescr(from) && typeDescrs[from.index].first.decors.empty())
        from = typeDescrs[from.index].first.base;
    if (isTypeDescr(into) && typeDescrs[into.index].first.decors.empty())
        into = typeDescrs[into.index].first.base;
    
    if ((worksAsTypePtr(from) && worksAsTypeAnyP(into)) ||
        (worksAsTypePtr(into) && worksAsTypeAnyP(from)))
        return true;

    if (isPrimitive(from)) {
        if (!isPrimitive(into)) return false;

        PrimIds s = (PrimIds) from.index;
        PrimIds d = (PrimIds) into.index;

        return (s == d ||
            (worksAsTypeI(from) && between(d, s, P_I64)) ||
            (worksAsTypeU(from) && between(d, s, P_U64)) ||
            (worksAsTypeF(from) && between(d, s, P_F64)));
    } else if (isTuple(from)) {
        if (!isTuple(into)) return false;

        // TODO rethink the rules
        return from.index == into.index;
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

TypeTable::Id TypeTable::addTypeDropCnsOf(Id t) {
    if (isTypeDescr(t)) {
        const TypeDescr &old = getTypeDescr(t);

        TypeDescr now(addTypeDropCnsOf(old.base), false);
        now.decors = vector<TypeDescr::Decor>(old.decors.begin(), old.decors.end());
        now.cns = vector<bool>(old.cns.size(), false);

        return addTypeDescr(move(now));
    } else if (isTuple(t)) {
        const Tuple &old = getTuple(t);

        Tuple now;
        now.members.resize(old.members.size());
        for (size_t i = 0; i < old.members.size(); ++i) {
            now.members[i] = addTypeDropCnsOf(old.members[i]);
        }

        return addTuple(move(now)).value();
    } else {
        return t;
    }
}