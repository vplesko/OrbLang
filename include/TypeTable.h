#pragma once

#include <vector>
#include <unordered_map>
#include "llvm/IR/Instructions.h"
#include "utils.h"
#include "NamePool.h"

class TypeTable {
public:
    typedef unsigned Id;

    struct TypeDescr {
        struct Decor {
            enum Type {
                D_PTR,
                D_ARR,
                D_ARR_PTR,
                D_INVALID
            };

            Type type;
            unsigned long len;
            
            bool eq(const Decor &other) const {
                return type == other.type && len == other.len;
            }
        };

        Id base;
        std::vector<Decor> decors;
        bool cn;
        std::vector<bool> cns;

        TypeDescr() {}
        TypeDescr(Id base, bool cn_ = false) : base(base), cn(cn_) {}

        TypeDescr(TypeDescr&&) = default;
        TypeDescr& operator=(TypeDescr&&) = default;

        TypeDescr(const TypeDescr&) = delete;
        void operator=(const TypeDescr&) = delete;

        void addDecor(Decor d, bool cn_ = false);
        void setLastCn();

        bool eq(const TypeDescr &other) const;
    };

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
        P_C8,
        P_PTR,
        P_ENUM_END // length marker, do not reference
    };

    static const PrimIds WIDEST_I = P_I64;
    static const PrimIds WIDEST_U = P_U64;
    static const PrimIds WIDEST_F = P_F64;

    static Id shortestFittingTypeI(int64_t x);

private:
    Id next;

    std::unordered_map<NamePool::Id, Id> typeIds;
    std::vector<std::pair<TypeDescr, llvm::Type*>> types;

public:
    TypeTable();

    TypeTable::Id addType(TypeDescr typeDescr);
    std::pair<bool, TypeTable::Id> addTypeDeref(Id typeId);
    std::pair<bool, TypeTable::Id> addTypeIndex(Id typeId);
    TypeTable::Id addTypeAddr(Id typeId);
    TypeTable::Id addType(TypeDescr typeDescr, llvm::Type *type);
    void addPrimType(NamePool::Id name, PrimIds id, llvm::Type *type);

    llvm::Type* getType(Id id);
    void setType(Id id, llvm::Type *type);
    const TypeDescr& getTypeDescr(Id id);

    bool isType(NamePool::Id name) const;
    TypeTable::Id getTypeId(NamePool::Id name) const { return typeIds.at(name); }

    bool isTypeI(Id t) const;
    bool isTypeU(Id t) const;
    bool isTypeF(Id t) const;
    bool isTypeC(Id t) const;
    bool isTypeB(Id t) const;
    bool isTypeAnyP(Id t) const;
    bool isTypeP(Id t) const;
    bool isTypeArr(Id t) const;
    bool isTypeArrP(Id t) const;
    bool isTypeCn(Id t) const;

    bool fitsType(int64_t x, Id t) const;
    bool isImplicitCastable(Id from, Id into) const;
    TypeTable::Id getTypeDropCns(Id t);
    TypeTable::Id getTypeFuncSigParam(Id t) { return getTypeDropCns(t); }
    bool isArgTypeProper(Id callArg, Id fncParam) const { return isImplicitCastable(callArg, fncParam); }
};