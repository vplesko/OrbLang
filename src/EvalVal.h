#pragma once

#include <optional>
#include <variant>
#include <vector>
#include "EscapeScore.h"
#include "LifetimeInfo.h"
#include "NamePool.h"
#include "StringPool.h"
#include "SymbolTableIds.h"
#include "TypeTable.h"

class NodeVal;
class SymbolTable;
struct FuncValue;
struct MacroValue;

// TODO eval array pointers (non-null)
struct EvalVal {
    typedef std::variant<NodeVal*, VarId> Pointer;

private:
    struct EasyZeroVals {
        union {
            std::int8_t i8;
            std::int16_t i16;
            std::int32_t i32;
            std::int64_t i64;
            std::uint8_t u8;
            std::uint16_t u16;
            std::uint32_t u32;
            std::uint64_t u64;
            float f32;
            double f64;
            char c8;
            bool b;
        };

        EasyZeroVals() {
            // because of union, this takes care of all primitives
            u64 = 0LL;
        }
    };

    TypeTable::Id type;

    std::variant<
        EasyZeroVals,
        NamePool::Id,
        TypeTable::Id,
        Pointer,
        std::optional<StringPool::Id>,
        std::optional<FuncId>,
        std::optional<MacroId>,
        std::vector<NodeVal>> value;

    Pointer ref = nullptr;
    LifetimeInfo lifetimeInfo;

    EscapeScore escapeScore = 0;

public:
    // type of this evaluation value
    // if modifying, make sure to init value; consider using makeVal or makeZero
    TypeTable::Id& getType() { return type; }
    // type of this evaluation value
    const TypeTable::Id& getType() const { return type; }

    std::int8_t& i8() { return std::get<EasyZeroVals>(value).i8; }
    std::int8_t i8() const { return std::get<EasyZeroVals>(value).i8; }
    std::int16_t& i16() { return std::get<EasyZeroVals>(value).i16; }
    std::int16_t i16() const { return std::get<EasyZeroVals>(value).i16; }
    std::int32_t& i32() { return std::get<EasyZeroVals>(value).i32; }
    std::int32_t i32() const { return std::get<EasyZeroVals>(value).i32; }
    std::int64_t& i64() { return std::get<EasyZeroVals>(value).i64; }
    std::int64_t i64() const { return std::get<EasyZeroVals>(value).i64; }

    std::uint8_t& u8() { return std::get<EasyZeroVals>(value).u8; }
    std::uint8_t u8() const { return std::get<EasyZeroVals>(value).u8; }
    std::uint16_t& u16() { return std::get<EasyZeroVals>(value).u16; }
    std::uint16_t u16() const { return std::get<EasyZeroVals>(value).u16; }
    std::uint32_t& u32() { return std::get<EasyZeroVals>(value).u32; }
    std::uint32_t u32() const { return std::get<EasyZeroVals>(value).u32; }
    std::uint64_t& u64() { return std::get<EasyZeroVals>(value).u64; }
    std::uint64_t u64() const { return std::get<EasyZeroVals>(value).u64; }

    std::uint64_t& getWidestU() { return u64(); }
    std::uint64_t getWidestU() const { return u64(); }

    float& f32() { return std::get<EasyZeroVals>(value).f32; }
    float f32() const { return std::get<EasyZeroVals>(value).f32; }
    double& f64() { return std::get<EasyZeroVals>(value).f64; }
    double f64() const { return std::get<EasyZeroVals>(value).f64; }

    char& c8() { return std::get<EasyZeroVals>(value).c8; }
    char c8() const { return std::get<EasyZeroVals>(value).c8; }

    bool& b() { return std::get<EasyZeroVals>(value).b; }
    bool b() const { return std::get<EasyZeroVals>(value).b; }

    NamePool::Id& id() { return std::get<NamePool::Id>(value); }
    const NamePool::Id& id() const { return std::get<NamePool::Id>(value); }

    // value of type 'type', contained within this evaluated value
    TypeTable::Id& ty() { return std::get<TypeTable::Id>(value); }
    // value of type 'type', contained within this evaluated value
    const TypeTable::Id& ty() const { return std::get<TypeTable::Id>(value); }

    Pointer& p() { return std::get<Pointer>(value); }
    const Pointer& p() const { return std::get<Pointer>(value); }

    std::optional<StringPool::Id>& str() { return std::get<std::optional<StringPool::Id>>(value); }
    const std::optional<StringPool::Id>& str() const { return std::get<std::optional<StringPool::Id>>(value); }

    std::optional<FuncId>& f() { return std::get<std::optional<FuncId>>(value); }
    const std::optional<FuncId>& f() const { return std::get<std::optional<FuncId>>(value); }

    std::optional<MacroId>& m() { return std::get<std::optional<MacroId>>(value); }
    const std::optional<MacroId>& m() const { return std::get<std::optional<MacroId>>(value); }

    std::vector<NodeVal>& elems() { return std::get<std::vector<NodeVal>>(value); }
    const std::vector<NodeVal>& elems() const { return std::get<std::vector<NodeVal>>(value); }

    bool hasRef() const { return !isNull(ref); }
    Pointer& getRef() { return ref; }
    const Pointer& getRef() const { return ref; }
    void removeRef();

    LifetimeInfo& getLifetimeInfo() { return lifetimeInfo; }
    const LifetimeInfo& getLifetimeInfo() const { return lifetimeInfo; }

    bool isEscaped() const { return escapeScore > 0; }
    EscapeScore getEscapeScore() const { return escapeScore; }
    EscapeScore& getEscapeScore() { return escapeScore; }

    std::optional<VarId> getVarId() const;

    static EvalVal makeVal(TypeTable::Id t, TypeTable *typeTable);
    static EvalVal makeZero(TypeTable::Id t, NamePool *namePool, TypeTable *typeTable);

    // use NodeVal::copyNoRef unless sure attrs aren't needed
    static EvalVal copyNoRef(const EvalVal &k, LifetimeInfo lifetimeInfo);
    // use NodeVal::moveNoRef unless sure attrs aren't needed
    static EvalVal moveNoRef(EvalVal &&k, LifetimeInfo lifetimeInfo);

    static bool isId(const EvalVal &val, const TypeTable *typeTable);
    static bool isType(const EvalVal &val, const TypeTable *typeTable);
    static bool isRaw(const EvalVal &val, const TypeTable *typeTable);
    static bool isFunc(const EvalVal &val, const TypeTable *TypeTable);
    static bool isMacro(const EvalVal &val, const TypeTable *TypeTable);
    static bool isI(const EvalVal &val, const TypeTable *typeTable);
    static bool isU(const EvalVal &val, const TypeTable *typeTable);
    static bool isF(const EvalVal &val, const TypeTable *typeTable);
    static bool isB(const EvalVal &val, const TypeTable *typeTable);
    static bool isC(const EvalVal &val, const TypeTable *typeTable);
    // specifically, pointer (with *)
    static bool isP(const EvalVal &val, const TypeTable *typeTable);
    static bool isStr(const EvalVal &val, const TypeTable *typeTable);
    static bool isNonNullStr(const EvalVal &val, const TypeTable *typeTable);
    // P_PTR or pointer or array pointer
    static bool isAnyP(const EvalVal &val, const TypeTable *typeTable);
    static bool isArr(const EvalVal &val, const TypeTable *typeTable);
    static bool isTuple(const EvalVal &val, const TypeTable *typeTable);
    static bool isDataType(const EvalVal &val, const TypeTable *typeTable);
    static bool isZero(const EvalVal &val, const TypeTable *typeTable);
    static bool isNull(const EvalVal &val, const TypeTable *typeTable);
    static bool isCallableNoValue(const EvalVal &val, const TypeTable *typeTable);

    static bool isNull(const Pointer &ptr);
    static NodeVal& deref(const Pointer &ptr, SymbolTable *symbolTable);
    static NodeVal& getPointee(const EvalVal &val, SymbolTable *symbolTable);
    static NodeVal& getRefee(const EvalVal &val, SymbolTable *symbolTable);

    static std::optional<std::int64_t> getValueI(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueU(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<double> getValueF(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueNonNeg(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<std::optional<FuncId>> getValueFunc(const EvalVal &val, const TypeTable *typeTable);

    static bool isImplicitCastable(const EvalVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable);
};