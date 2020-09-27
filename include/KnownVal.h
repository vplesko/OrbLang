#pragma once

#include <optional>
#include <vector>
#include "NamePool.h"
#include "StringPool.h"
#include "TypeTable.h"

class SymbolTable;

struct KnownVal {
    std::optional<TypeTable::Id> type;
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
        std::optional<StringPool::Id> str;
        NamePool::Id id;
        TypeTable::Id ty;
    };
    std::vector<KnownVal> elems;
    
    KnownVal *ref = nullptr;

    KnownVal() {}

    std::optional<TypeTable::Id> getType() const { return type; }

    // if is callable, type is meaningless
    // TODO after callables are first-class, rework that
    bool isCallable() const { return !type.has_value(); }
    std::optional<NamePool::Id> getCallableId() const;

    static KnownVal makeVal(TypeTable::Id t, TypeTable *typeTable);
    static KnownVal copyNoRef(const KnownVal &k);

    static bool isId(const KnownVal &val, const TypeTable *typeTable);
    static bool isType(const KnownVal &val, const TypeTable *typeTable);
    static bool isMacro(const KnownVal &val, const SymbolTable *symbolTable);
    static bool isFunc(const KnownVal &val, const SymbolTable *symbolTable);
    static bool isI(const KnownVal &val, const TypeTable *typeTable);
    static bool isU(const KnownVal &val, const TypeTable *typeTable);
    static bool isF(const KnownVal &val, const TypeTable *typeTable);
    static bool isB(const KnownVal &val, const TypeTable *typeTable);
    static bool isC(const KnownVal &val, const TypeTable *typeTable);
    static bool isStr(const KnownVal &val, const TypeTable *typeTable);
    static bool isAnyP(const KnownVal &val, const TypeTable *typeTable);
    static bool isArr(const KnownVal &val, const TypeTable *typeTable);
    static bool isTuple(const KnownVal &val, const TypeTable *typeTable);
    static bool isNull(const KnownVal &val, const TypeTable *typeTable);

    static std::optional<std::int64_t> getValueI(const KnownVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueU(const KnownVal &val, const TypeTable *typeTable);
    static std::optional<double> getValueF(const KnownVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueNonNeg(const KnownVal &val, const TypeTable *typeTable);

    static bool isImplicitCastable(const KnownVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable);
};