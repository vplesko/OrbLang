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
            kData,
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

    struct DataType {
        struct Member {
            Id type;
            NamePool::Id name;
        };

        NamePool::Id name;
        std::vector<Member> members;

        DataType() {}
        explicit DataType(NamePool::Id name) : name(name) {}

        bool isDecl() const { return members.empty(); }

        void addMember(Member m);

        bool eq(const DataType &other) const {
            return name == other.name;
        }
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

    static PrimIds shortestFittingPrimTypeI(int64_t x);

private:
    Id strType;

    std::array<llvm::Type*, P_ENUM_END> primTypes;
    std::vector<std::pair<DataType, llvm::Type*>> dataTypes;
    std::vector<std::pair<TypeDescr, llvm::Type*>> typeDescrs;
    
    std::unordered_map<NamePool::Id, Id> typeIds;
    std::unordered_map<Id, NamePool::Id, Id::Hasher> typeNames;

    bool canWorkAsPrimitive(Id t) const;
    bool canWorkAsPrimitive(Id t, PrimIds p) const;
    bool canWorkAsPrimitive(Id t, PrimIds lo, PrimIds hi) const;

    template <typename T>
    bool isTypeDescrSatisfyingCondition(Id t, T cond) const;

    void addTypeStr();

public:
    TypeTable();

    void addPrimType(NamePool::Id name, PrimIds primId, llvm::Type *type);
    Id addDataType(NamePool::Id name);

    // if typeDescr is secretly a prim/data, that type's Id is returned instead
    Id addTypeDescr(TypeDescr typeDescr);
    // if typeDescr is secretly a prim/data, that type's Id is returned instead
    Id addTypeDescr(TypeDescr typeDescr, llvm::Type *type);

    std::optional<Id> addTypeDeref(Id typeId);
    std::optional<Id> addTypeIndex(Id typeId);
    Id addTypeAddr(Id typeId);
    Id addTypeArrOfLenId(Id typeId, std::size_t len);

    llvm::Type* getType(Id id);
    void setType(Id id, llvm::Type *type);
    
    llvm::Type* getPrimType(PrimIds id) const;
    Id getPrimTypeId(PrimIds id) const;
    DataType& getDataType(Id id);
    const TypeDescr& getTypeDescr(Id id) const;

    Id getTypeIdStr() { return strType; }
    Id getTypeCharArrOfLenId(std::size_t len);
    std::optional<std::size_t> getArrLen(Id arrTypeId) const;

    bool isValidType(Id t) const;
    bool isType(NamePool::Id name) const;
    std::optional<Id> getTypeId(NamePool::Id name) const;
    std::optional<NamePool::Id> getTypeName(Id t) const;

    bool isPrimitive(Id t) const;
    bool isDataType(Id t) const;
    bool isTypeDescr(Id t) const;
    bool isNonOpaqueType(Id t) const;
    bool isTypeI(Id t) const { return canWorkAsPrimitive(t, P_I8, P_I64); }
    bool isTypeU(Id t) const { return canWorkAsPrimitive(t, P_U8, P_U64); }
    bool isTypeF(Id t) const { return canWorkAsPrimitive(t, P_F16, P_F64); }
    bool isTypeC(Id t) const { return canWorkAsPrimitive(t, P_C8); }
    bool isTypeB(Id t) const { return canWorkAsPrimitive(t, P_BOOL); }
    bool isTypeAnyP(Id t) const;
    bool isTypeP(Id t) const;
    // specifically, P_PTR
    bool isTypePtr(Id t) const;
    bool isTypeArr(Id t) const;
    bool isTypeArrOfLen(Id t, std::size_t len) const;
    bool isTypeArrP(Id t) const;
    bool isTypeStr(Id t) const;
    bool isTypeCharArrOfLen(Id t, std::size_t len) const;
    bool isTypeCn(Id t) const;

    bool dataMayTakeName(NamePool::Id name) const;
    bool fitsType(int64_t x, Id t) const;
    Id shortestFittingTypeIId(int64_t x) const;
    bool isImplicitCastable(Id from, Id into) const;
    Id getTypeDropCns(Id t);
    Id getTypeFuncSigParam(Id t) { return getTypeDropCns(t); }
    bool isArgTypeProper(Id callArg, Id fncParam) const { return isImplicitCastable(callArg, fncParam); }
};

template <typename T>
bool TypeTable::isTypeDescrSatisfyingCondition(Id t, T cond) const {
    if (!isTypeDescr(t)) return false;

    const TypeDescr &ty = typeDescrs[t.index].first;
    return cond(ty);
}