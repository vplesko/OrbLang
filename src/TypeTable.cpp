#include "TypeTable.h"
using namespace std;

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

    if (cn_ || (prevIsCn && d.type == Decor::D_ARR))
        setLastCn();
}

void TypeTable::TypeDescr::setLastCn() {
    if (cns.empty()) cn = true;
    else {
        // TODO write negative tests for this clusterfudge
        bool setBaseCn = false;
        for (size_t i = cns.size()-1;; --i) {
            if (cns[i]) break;

            cns[i] = true;

            if (decors[i].type != Decor::D_ARR) break;
            if (i == 0) { setBaseCn = true; break; }
        }
        if (setBaseCn) cn = true;
    }
}

TypeTable::Id TypeTable::shortestFittingTypeI(int64_t x) {
    if (x >= numeric_limits<int8_t>::min() && x <= numeric_limits<int8_t>::max()) return P_I8;
    if (x >= numeric_limits<int16_t>::min() && x <= numeric_limits<int16_t>::max()) return P_I16;
    if (x >= numeric_limits<int32_t>::min() && x <= numeric_limits<int32_t>::max()) return P_I32;
    return P_I64;
}

TypeTable::TypeTable() : next(P_ENUM_END), types(P_ENUM_END) {
    for (size_t i = 0; i < types.size(); ++i) {
        types[i].first = TypeDescr(i);
        types[i].second = nullptr;
    }

    addTypeStr();
}

TypeTable::Id TypeTable::addType(TypeDescr typeDescr) {
    // TODO this is somewhat slow, find a way to more quickly ensure uniqueness
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i].first.eq(typeDescr))
            return i;
    }

    types.push_back(make_pair(move(typeDescr), nullptr));
    return next++;
}

TypeTable::Id TypeTable::addType(TypeDescr typeDescr, llvm::Type *type) {
    Id id = addType(move(typeDescr));
    setType(id, type);
    return id;
}

optional<TypeTable::Id> TypeTable::addTypeDeref(Id typeId) {
    const TypeDescr &typeDescr = types[typeId].first;
    
    // ptr cannot be dereferenced
    if (typeId == TypeTable::P_PTR || !isTypeP(typeId))
        return nullopt;
    
    TypeDescr typeDerefDescr(typeDescr.base, typeDescr.cn);
    typeDerefDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end()-1);
    typeDerefDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end()-1);
    
    return addType(move(typeDerefDescr));
}

optional<TypeTable::Id> TypeTable::addTypeIndex(Id typeId) {
    const TypeDescr &typeDescr = types[typeId].first;
    
    if (!isTypeArrP(typeId) && !isTypeArr(typeId))
        return nullopt;
    
    TypeDescr typeIndexDescr(typeDescr.base, typeDescr.cn);
    typeIndexDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end()-1);
    typeIndexDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end()-1);
    
    return addType(move(typeIndexDescr));
}

TypeTable::Id TypeTable::addTypeAddr(Id typeId) {
    const TypeDescr &typeDescr = types[typeId].first;

    TypeDescr typeAddrDescr(typeDescr.base, typeDescr.cn);
    typeAddrDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.size()+1);
    typeAddrDescr.cns = vector<bool>(typeDescr.cns.size()+1);
    for (size_t i = 0; i < typeDescr.decors.size(); ++i) {
        typeAddrDescr.decors[i] = typeDescr.decors[i];
        typeAddrDescr.cns[i] = typeDescr.cns[i];
    }
    typeAddrDescr.decors.back() = {TypeDescr::Decor::D_PTR};

    return addType(move(typeAddrDescr));
}

TypeTable::Id TypeTable::addTypeArrOfLenId(Id typeId, std::size_t len) {
    const TypeDescr &typeDescr = types[typeId].first;
    
    TypeDescr typeArrDescr(typeDescr.base, typeDescr.cn);
    typeArrDescr.decors = vector<TypeDescr::Decor>(typeDescr.decors.begin(), typeDescr.decors.end());
    typeArrDescr.cns = vector<bool>(typeDescr.cns.begin(), typeDescr.cns.end());

    typeArrDescr.addDecor({TypeDescr::Decor::D_ARR, len}, false);
    typeArrDescr.setLastCn();
    
    return addType(move(typeArrDescr));
}

void TypeTable::addTypeStr() {
    TypeDescr typeDescr(P_C8, true);
    typeDescr.addDecor({TypeDescr::Decor::D_ARR_PTR}, false);
    strType = addType(move(typeDescr));
}

void TypeTable::addPrimType(NamePool::Id name, PrimIds id, llvm::Type *type) {
    typeIds.insert(make_pair(name, id));
    typeNames.insert(make_pair(id, name));
    types[id].first.base = id;
    types[id].second = type;
}

llvm::Type* TypeTable::getType(Id id) {
    return types[id].second;
}

void TypeTable::setType(Id id, llvm::Type *type) {
    types[id].second = type;
}

const TypeTable::TypeDescr& TypeTable::getTypeDescr(Id id) {
    return types[id].first;
}

TypeTable::Id TypeTable::getTypeCharArrOfLenId(std::size_t len) {
    return addTypeArrOfLenId(P_C8, len);
}

optional<size_t> TypeTable::getArrLen(Id arrTypeId) const {
    if (arrTypeId >= types.size()) return nullopt;
    const TypeDescr &ty = types[arrTypeId].first;
    if (!ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR)
        return ty.decors.back().len;
    return nullopt;
}

bool TypeTable::isType(NamePool::Id name) const {
    return typeIds.find(name) != typeIds.end();
}

optional<NamePool::Id> TypeTable::getTypeName(Id t) const {
    auto loc = typeNames.find(t);
    if (loc == typeNames.end()) return nullopt;
    return loc->second;
}

bool TypeTable::isTypeI(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return ty.decors.empty() && between((PrimIds) ty.base, P_I8, P_I64);
}

bool TypeTable::isTypeU(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return ty.decors.empty() && between((PrimIds) ty.base, P_U8, P_U64);
}

bool TypeTable::isTypeF(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return ty.decors.empty() && between((PrimIds) ty.base, P_F16, P_F64);
}

bool TypeTable::isTypeC(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return ty.decors.empty() && (PrimIds) ty.base == P_C8;
}

bool TypeTable::isTypeB(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return ty.decors.empty() && ty.base == P_BOOL;
}

bool TypeTable::isTypeAnyP(Id t) const {
    return isTypeP(t) || isTypeArrP(t);
}

bool TypeTable::isTypeP(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return (ty.decors.empty() && ty.base == P_PTR) ||
        (!ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_PTR);
}

bool TypeTable::isTypeArr(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR;
}

bool TypeTable::isTypeArrOfLen(Id t, std::size_t len) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR && ty.decors.back().len == len;
}

bool TypeTable::isTypeArrP(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return !ty.decors.empty() && ty.decors.back().type == TypeDescr::Decor::D_ARR_PTR;
}

bool TypeTable::isTypeStr(Id t) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return ty.decors.size() == 1 && ty.decors[0].type == TypeDescr::Decor::D_ARR_PTR &&
        ty.base == P_C8 && ty.cn;
}

bool TypeTable::isTypeCharArrOfLen(Id t, size_t len) const {
    if (t >= types.size()) return false;
    const TypeDescr &ty = types[t].first;
    return ty.decors.size() == 1 && ty.base == P_C8 &&
        ty.decors[0].type == TypeDescr::Decor::D_ARR && ty.decors[0].len == len;
}

bool TypeTable::isTypeCn(Id t) const {
    const TypeDescr &ty = types[t].first;
    if (ty.cns.empty()) return ty.cn;
    else return ty.cns.back();
}

bool TypeTable::fitsType(int64_t x, Id t) const {
    if (t >= types.size() || !types[t].first.decors.empty())
        return false;
    
    int64_t lo, hi;
    switch (types[t].first.base) {
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

bool TypeTable::isImplicitCastable(Id from, Id into) const {
    if (from == into) return true;

    const TypeDescr &s = types[from].first, &d = types[into].first;

    if (s.decors.empty()) {
        return d.decors.empty() &&
            (s.base == d.base ||
            (isTypeI(s.base) && between(d.base, s.base, (Id) P_I64)) ||
            (isTypeU(s.base) && between(d.base, s.base, (Id) P_U64)) ||
            (isTypeF(s.base) && between(d.base, s.base, (Id) P_F64)));
    } else {
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
    }
}

TypeTable::Id TypeTable::getTypeDropCns(Id t) {
    TypeDescr &old = types[t].first;

    TypeDescr now(old.base, false);
    now.decors = vector<TypeDescr::Decor>(old.decors.begin(), old.decors.end());
    now.cns = vector<bool>(old.cns.size(), false);

    return addType(move(now));
}