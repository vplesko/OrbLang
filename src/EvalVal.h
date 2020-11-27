#pragma once

#include <optional>
#include <vector>
#include "EscapeScore.h"
#include "NamePool.h"
#include "StringPool.h"
#include "TypeTable.h"
#include "RawVal.h"

class SymbolTable;

struct EvalVal {
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
    RawVal raw;
    std::vector<EvalVal> elems;
    
    EvalVal *ref = nullptr;

    EscapeScore escapeScore = 0;

    EvalVal() {}

    std::optional<TypeTable::Id> getType() const { return type; }

    bool isEscaped() const { return escapeScore > 0; }

    // if is callable, type is meaningless
    // TODO after callables (fncs, macs, specs) are first-class, rework that
    bool isCallable() const { return !type.has_value(); }
    std::optional<NamePool::Id> getCallableId() const;

    static EvalVal makeVal(TypeTable::Id t, TypeTable *typeTable);
    static EvalVal copyNoRef(const EvalVal &k);

    static bool isId(const EvalVal &val, const TypeTable *typeTable);
    static bool isType(const EvalVal &val, const TypeTable *typeTable);
    static bool isRaw(const EvalVal &val, const TypeTable *typeTable);
    static bool isMacro(const EvalVal &val, const SymbolTable *symbolTable);
    static bool isFunc(const EvalVal &val, const SymbolTable *symbolTable);
    static bool isI(const EvalVal &val, const TypeTable *typeTable);
    static bool isU(const EvalVal &val, const TypeTable *typeTable);
    static bool isF(const EvalVal &val, const TypeTable *typeTable);
    static bool isB(const EvalVal &val, const TypeTable *typeTable);
    static bool isC(const EvalVal &val, const TypeTable *typeTable);
    static bool isStr(const EvalVal &val, const TypeTable *typeTable);
    static bool isAnyP(const EvalVal &val, const TypeTable *typeTable);
    static bool isArr(const EvalVal &val, const TypeTable *typeTable);
    static bool isTuple(const EvalVal &val, const TypeTable *typeTable);
    static bool isNull(const EvalVal &val, const TypeTable *typeTable);

    static std::optional<std::int64_t> getValueI(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueU(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<double> getValueF(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueNonNeg(const EvalVal &val, const TypeTable *typeTable);

    static bool isImplicitCastable(const EvalVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable);
    static bool isCastable(const EvalVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable);
};