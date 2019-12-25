#pragma once

#include <unordered_map>
#include "llvm/IR/Instructions.h"
#include "utils.h"
#include "NamePool.h"

class TypeTable {
public:
    // TODO at some point, have Type struct that can represent ptrs, arrs, constness...
    typedef unsigned Id;

    enum PrimIds {
        P_BOOL,
        P_I8,
        P_I16,
        P_I32,
        P_I64,
        P_U8,
        P_U16,
        P_U32,
        P_U64,
        P_F16,
        P_F32,
        P_F64,
        P_ENUM_END // length marker, do not reference
    };

    static const PrimIds WIDEST_I = P_I64;
    static const PrimIds WIDEST_U = P_U64;
    static const PrimIds WIDEST_F = P_F64;

    static bool isTypeI(Id t) {
        return between((PrimIds) t, P_I8, P_I64);
    }

    static bool isTypeU(Id t) {
        return between((PrimIds) t, P_U8, P_U64);
    }

    static bool isTypeF(Id t) {
        return between((PrimIds) t, P_F16, P_F64);
    }

    static bool fitsType(int64_t x, Id t);
    static Id shortestFittingTypeI(int64_t x);

    static bool isImplicitCastable(Id from, Id into) {
        PrimIds s = (PrimIds) from, d = (PrimIds) into;
        return (isTypeI(s) && between(d, s, P_I64)) ||
            (isTypeU(s) && between(d, s, P_U64)) ||
            (isTypeF(s) && between(d, s, P_F64));
    }

private:
    Id next;

    std::unordered_map<NamePool::Id, Id> typeIds;
    std::vector<llvm::Type*> types;

public:
    TypeTable();

    TypeTable::Id addType(NamePool::Id name, llvm::Type *type);
    void addPrimType(NamePool::Id name, PrimIds id, llvm::Type *type);
    llvm::Type* getType(Id id);

    bool isType(NamePool::Id name) const;
    TypeTable::Id getTypeId(NamePool::Id name) const { return typeIds.at(name); }
};