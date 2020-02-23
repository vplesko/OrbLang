#include "TypeTable.h"
using namespace std;

bool TypeTable::TypeDescr::operator==(const TypeDescr &other) const {
    if (base != other.base || decors.size() != other.decors.size()) return false;
    for (size_t i = 0; i < decors.size(); ++i) {
        if (decors[i] != other.decors[i]) return false;
    }
    return true;
}

bool TypeTable::fitsType(int64_t x, Id t) {
    int64_t lo, hi;
    switch (t) {
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
}

TypeTable::Id TypeTable::addType(TypeDescr typeDescr) {
    // TODO this is somewhat slow, find a way to more quickly ensure uniqueness
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i].first == typeDescr)
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

void TypeTable::addPrimType(NamePool::Id name, PrimIds id, llvm::Type *type) {
    typeIds.insert(make_pair(name, id));
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

bool TypeTable::isType(NamePool::Id name) const {
    return typeIds.find(name) != typeIds.end();
}

bool TypeTable::isTypeAnyP(Id t) {
    return t == P_PTR || (!types[t].first.decors.empty() && types[t].first.decors.back() == TypeDescr::D_PTR);
}