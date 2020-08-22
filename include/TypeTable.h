#pragma once

#include <array>
#include <vector>
#include <unordered_map>
#include <optional>
#include "utils.h"
#include "NamePool.h"
#include "llvm/IR/Instructions.h"

class TypeTable {
public:
    struct Id {
        enum Kind {
            kPrim,
            kTuple,
            kDescr
        };

        Kind kind;
        std::size_t index;

        friend bool operator==(const Id &l, const Id &r)
        { return l.kind == r.kind && l.index == r.index; }

        friend bool operator!=(const Id &l, const Id &r)
        { return !(l == r); }

        struct Hasher {
            std::size_t operator()(const Id &id) const {
                return leNiceHasheFunctione(id.kind, id.index);
            }
        };
    };

    struct Tuple {
        std::vector<Id> members;

        Tuple() {}
        Tuple(std::vector<Id> membs) : members(std::move(membs)) {}

        void addMember(Id m);

        bool eq(const Tuple &other) const;
    };

    struct TypeDescr {
        struct Decor {
            enum Type {
                D_PTR,
                D_ARR,
                D_ARR_PTR,
                D_INVALID
            };

            Type type = D_INVALID;
            std::size_t len;
            
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

    // TODO add i128, u128, f16, f128
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
        P_F32,
        P_F64,
        P_C8,
        P_PTR,
        P_ID,
        P_TYPE,
        P_ENUM_END // length marker, do not reference
    };

    static const PrimIds WIDEST_I = P_I64;
    static const PrimIds WIDEST_U = P_U64;
    static const PrimIds WIDEST_F = P_F64;

    static PrimIds shortestFittingPrimTypeI(int64_t x);
    static PrimIds shortestFittingPrimTypeF(double x);

private:
    Id strType;

    std::array<llvm::Type*, P_ENUM_END> primTypes;
    std::vector<std::pair<Tuple, llvm::Type*>> tuples;
    std::vector<std::pair<TypeDescr, llvm::Type*>> typeDescrs;
    
    std::unordered_map<NamePool::Id, Id> typeIds;
    std::unordered_map<Id, NamePool::Id, Id::Hasher> typeNames;

    template <typename T>
    bool isTypeDescrSatisfyingCondition(Id t, T cond) const;

    void addTypeStr();

public:
    TypeTable();

    void addPrimType(NamePool::Id name, PrimIds primId, llvm::Type *type);
    // should have at least one member. if there is exactly one member,
    // the created type will not be a tuple, but the type of that member.
    std::optional<Id> addTuple(Tuple tup);

    // if typeDescr is secretly a prim/tuple, that type's Id is returned instead
    Id addTypeDescr(TypeDescr typeDescr);
    // if typeDescr is secretly a prim/tuple, that type's Id is returned instead
    Id addTypeDescr(TypeDescr typeDescr, llvm::Type *type);

    std::optional<Id> addTypeDerefOf(Id typeId);
    std::optional<Id> addTypeIndexOf(Id typeId);
    Id addTypeAddrOf(Id typeId);
    Id addTypeArrOfLenIdOf(Id typeId, std::size_t len);
    Id addTypeCnOf(Id typeId);
    Id addTypeDropCnsOf(Id t);

    llvm::Type* getType(Id id);
    void setType(Id id, llvm::Type *type);
    
    llvm::Type* getPrimType(PrimIds id) const;
    Id getPrimTypeId(PrimIds id) const;
    const Tuple& getTuple(Id id) const;
    const TypeDescr& getTypeDescr(Id id) const;

    Id getTypeIdStr() { return strType; }
    Id getTypeCharArrOfLenId(std::size_t len);

    bool isValidType(Id t) const;
    bool isType(NamePool::Id name) const;
    std::optional<Id> getTypeId(NamePool::Id name) const;
    std::optional<NamePool::Id> getTypeName(Id t) const;

    bool isPrimitive(Id t) const;
    bool isTuple(Id t) const;
    bool isTypeDescr(Id t) const;
    
    bool worksAsPrimitive(Id t) const;
    bool worksAsPrimitive(Id t, PrimIds p) const;
    bool worksAsPrimitive(Id t, PrimIds lo, PrimIds hi) const;
    bool worksAsTypeI(Id t) const { return worksAsPrimitive(t, P_I8, P_I64); }
    bool worksAsTypeU(Id t) const { return worksAsPrimitive(t, P_U8, P_U64); }
    bool worksAsTypeF(Id t) const { return worksAsPrimitive(t, P_F32, P_F64); }
    bool worksAsTypeC(Id t) const { return worksAsPrimitive(t, P_C8); }
    bool worksAsTypeB(Id t) const { return worksAsPrimitive(t, P_BOOL); }
    // specifically, P_PTR
    bool worksAsTypePtr(Id t) const;
    bool worksAsTypeAnyP(Id t) const;
    bool worksAsTypeP(Id t) const;
    bool worksAsTypeArr(Id t) const;
    bool worksAsTypeArrOfLen(Id t, std::size_t len) const;
    bool worksAsTypeArrP(Id t) const;
    bool worksAsTypeStr(Id t) const;
    bool worksAsTypeCharArrOfLen(Id t, std::size_t len) const;
    bool worksAsTypeCn(Id t) const;

    std::optional<const Tuple*> extractTuple(Id t) const;
    std::optional<std::size_t> extractLenOfArr(Id arrTypeId) const;

    bool fitsTypeI(int64_t x, Id t) const;
    bool fitsTypeU(uint64_t x, Id t) const;
    bool fitsTypeF(double x, Id t) const;
    Id shortestFittingTypeIId(int64_t x) const;
    bool isImplicitCastable(Id from, Id into) const;
    bool isArgTypeProper(Id callArg, Id fncParam) const { return isImplicitCastable(callArg, fncParam); }
    bool isDirectCn(Id t) const;
};

template <typename T>
bool TypeTable::isTypeDescrSatisfyingCondition(Id t, T cond) const {
    if (!isTypeDescr(t)) return false;

    const TypeDescr &ty = typeDescrs[t.index].first;
    return cond(ty);
}