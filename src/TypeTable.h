#pragma once

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>
#include "llvm/IR/Instructions.h"
#include "NamePool.h"
#include "utils.h"

class TypeTable {
public:
    struct Id {
    private:
        friend class TypeTable;

        enum Kind {
            kPrim,
            kTuple,
            kDescr,
            kExplicit,
            kData,
            kCallable
        };

        Kind kind;
        std::size_t index;

        Id(Kind k, std::size_t ind) : kind(k), index(ind) {}

    public:
        Id() {}

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
        std::vector<Id> elements;

        Tuple() {}
        explicit Tuple(std::vector<Id> elems) : elements(std::move(elems)) {}

        void addElement(Id m);

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
        explicit TypeDescr(Id base, bool cn_ = false) : base(base), cn(cn_) {}

        bool isEmpty() const { return decors.empty() && !cn; }

        void addDecor(Decor d, bool cn_ = false);
        void setLastCn();

        bool eq(const TypeDescr &other) const;
    };

    struct ExplicitType {
        Id type;
        NamePool::Id name;

        ExplicitType() {}
        ExplicitType(Id type, NamePool::Id name) : type(type), name(name) {}
    };

    struct DataType {
        struct ElemEntry {
            NamePool::Id name;
            TypeTable::Id type;
            bool noZeroInit = false;
        };

        bool defined = false;
        NamePool::Id name;
        std::vector<ElemEntry> elements;

        DataType() {}
        DataType(NamePool::Id name, std::vector<ElemEntry> elems) : name(name), elements(std::move(elems)) {}

        std::optional<std::size_t> getElemInd(NamePool::Id name) const;
    };

    struct Callable {
    private:
        friend class TypeTable;

        struct ArgEntry {
            TypeTable::Id ty;
            bool noDrop = false;

            bool eq(const ArgEntry &other) const
            { return ty == other.ty && noDrop == other.noDrop; }
        };

        std::vector<ArgEntry> args;

    public:
        bool isFunc;
        std::optional<TypeTable::Id> retType;
        bool variadic = false;

        std::size_t getArgCnt() const { return args.size(); }
        // args are left undefined
        void setArgCnt(std::size_t argCnt) { args.resize(argCnt); }

        TypeTable::Id getArgType(std::size_t ind) const { return args[ind].ty; }
        void setArgType(std::size_t ind, TypeTable::Id ty) { args[ind].ty = ty; }
        // assumed to have the same arg size
        void setArgTypes(const std::vector<TypeTable::Id> &argTys);

        bool getArgNoDrop(std::size_t ind) const { return args[ind].noDrop; }
        void setArgNoDrop(std::size_t ind, bool b) { args[ind].noDrop = b; }
        // assumed to have the same arg size
        void setArgNoDrops(const std::vector<bool> &argNoDrops);

        bool hasRet() const { return retType.has_value(); }

        bool eq(const Callable &other) const;
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
        P_F32,
        P_F64,
        P_C8,
        P_PTR,
        P_ID,
        P_TYPE,
        P_RAW,
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
    std::vector<std::pair<ExplicitType, llvm::Type*>> explicitTypes;
    std::vector<std::pair<DataType, llvm::Type*>> dataTypes;
    std::vector<std::pair<Callable, llvm::Type*>> callables;

    std::unordered_map<NamePool::Id, Id, NamePool::Id::Hasher> typeIds;
    std::unordered_map<Id, NamePool::Id, Id::Hasher> typeNames;

    template <typename T>
    bool worksAsTypeDescrSatisfyingCondition(Id t, T cond) const;
    TypeDescr normalize(const TypeDescr &descr) const;

    void addTypeStr();

public:
    TypeTable();

    void addPrimType(NamePool::Id name, PrimIds primId, llvm::Type *type);
    // if typeDescr is secretly a base type, that type's Id is returned instead
    // if the base is also typeDescr, its decors will be combined with this one's
    Id addTypeDescr(TypeDescr typeDescr);
    // should have at least one element. if there is exactly one element,
    // the created type will not be a tuple, but the type of that element.
    std::optional<Id> addTuple(Tuple tup);
    std::optional<Id> addExplicitType(ExplicitType c);
    // if already exists, errors on redefinition, overrides otherwise
    std::optional<Id> addDataType(DataType data);
    Id addCallable(Callable call);

    std::optional<Id> addTypeDerefOf(Id typeId);
    std::optional<Id> addTypeIndexOf(Id typeId);
    Id addTypeAddrOf(Id typeId);
    Id addTypeArrOfLenIdOf(Id typeId, std::size_t len);
    Id addTypeCnOf(Id typeId);

    Id addTypeDescrForSig(const TypeDescr &typeDescr);
    Id addTypeDescrForSig(Id t);
    Id addCallableSig(const Callable &call);
    Id addCallableSig(Id t);

    llvm::Type* getLlvmType(Id id);
    void setLlvmType(Id id, llvm::Type *type);

    Id getPrimTypeId(PrimIds id) const;
    const Tuple& getTuple(Id id) const;
    const TypeDescr& getTypeDescr(Id id) const;
    const ExplicitType& getExplicitType(Id id) const;
    const DataType& getDataType(Id id) const;
    DataType& getDataType(Id id);
    const Callable& getCallable(Id id) const;

    Id getTypeIdStr() { return strType; }
    Id getTypeCharArrOfLenId(std::size_t len);

    bool isValidType(Id t) const;
    bool isType(NamePool::Id name) const;
    std::optional<Id> getTypeId(NamePool::Id name) const;
    std::optional<NamePool::Id> getTypeName(Id t) const;

    bool isPrimitive(Id t) const;
    bool isTuple(Id t) const;
    bool isTypeDescr(Id t) const;
    bool isExplicitType(Id t) const;
    bool isDataType(Id t) const;
    bool isCallable(Id t) const;

    bool worksAsPrimitive(Id t) const;
    bool worksAsPrimitive(Id t, PrimIds p) const;
    bool worksAsPrimitive(Id t, PrimIds lo, PrimIds hi) const;
    bool worksAsTuple(Id t) const;
    bool worksAsExplicitType(Id t) const;
    bool worksAsDataType(Id t) const;
    bool worksAsCallable(Id t) const;
    bool worksAsCallable(Id t, bool isFunc) const;
    bool worksAsMacroWithArgs(Id t, std::size_t argCnt, bool variadic = false) const;
    bool worksAsTypeI(Id t) const { return worksAsPrimitive(t, P_I8, P_I64); }
    bool worksAsTypeU(Id t) const { return worksAsPrimitive(t, P_U8, P_U64); }
    bool worksAsTypeF(Id t) const { return worksAsPrimitive(t, P_F32, P_F64); }
    bool worksAsTypeC(Id t) const { return worksAsPrimitive(t, P_C8); }
    bool worksAsTypeB(Id t) const { return worksAsPrimitive(t, P_BOOL); }
    // specifically, P_PTR
    bool worksAsTypePtr(Id t) const;
    // P_PTR or pointer or array pointer
    bool worksAsTypeAnyP(Id t) const;
    // specifically, pointer (with *)
    bool worksAsTypeP(Id t) const;
    bool worksAsTypeArr(Id t) const;
    bool worksAsTypeArrOfLen(Id t, std::size_t len) const;
    bool worksAsTypeArrP(Id t) const;
    bool worksAsTypeStr(Id t) const;
    bool worksAsTypeCharArrOfLen(Id t, std::size_t len) const;
    bool worksAsTypeCn(Id t) const;

    const Tuple* extractTuple(Id t) const;
    const DataType* extractDataType(Id t) const;
    const Callable* extractCallable(Id callTypeId) const;

    std::optional<std::size_t> extractLenOfArr(Id arrTypeId) const;
    std::optional<std::size_t> extractLenOfTuple(Id tupleTypeId) const;
    std::optional<std::size_t> extractLenOfDataType(Id dataTypeId) const;

    // handles constness of resulting type
    std::optional<Id> extractTupleElementType(Id t, std::size_t ind);
    // handles constness of resulting type
    std::optional<Id> extractDataTypeElementType(Id t, NamePool::Id elem);
    // handles constness of resulting type
    std::optional<Id> extractDataTypeElementType(Id t, std::size_t ind);

    // passes through decors and explicit types
    Id extractBaseType(Id t) const;
    // only passes through explicit types and cn
    Id extractExplicitTypeBaseType(Id t) const;

    bool fitsTypeI(int64_t x, Id t) const;
    bool fitsTypeU(uint64_t x, Id t) const;
    bool fitsTypeF(double x, Id t) const;
    Id shortestFittingTypeIId(int64_t x) const;
    bool isDirectCn(Id t) const;
    bool isUndef(Id t);

    bool isImplicitCastable(Id from, Id into) const;

    std::optional<std::string> makeBinString(Id t, const NamePool *namePool, bool makeSigTy);
};

template <typename T>
bool TypeTable::worksAsTypeDescrSatisfyingCondition(Id t, T cond) const {
    if (isExplicitType(t)) return worksAsTypeDescrSatisfyingCondition(getExplicitType(t).type, cond);

    if (isTypeDescr(t)) {
        const TypeDescr &ty = typeDescrs[t.index].first;
        return cond(ty);
    }

    return false;
}