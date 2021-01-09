#pragma once

#include <optional>
#include <vector>
#include "EscapeScore.h"
#include "NamePool.h"
#include "StringPool.h"
#include "TypeTable.h"

class NodeVal;
class SymbolTable;
struct FuncValue;
struct MacroValue;

struct EvalVal {
    // type of this evaluation value
    TypeTable::Id type;
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
        // contains id value
        NamePool::Id id;
        // contains type value in case this is type or type cn
        TypeTable::Id ty;
        const FuncValue *f;
        const MacroValue *m;
        NodeVal *p;
    };
    std::optional<StringPool::Id> str;
    std::vector<NodeVal> elems;

    NodeVal *ref = nullptr;

    EscapeScore escapeScore = 0;

    EvalVal() {
        // because of union, this takes care of primitives other than id and type
        // this init is expected by Evaluator::makeCast, TODO+ break this expectation
        u64 = 0LL;
    }

    bool isEscaped() const { return escapeScore > 0; }

    std::uint64_t& getWidestU() { return u64; }
    const std::uint64_t& getWidestU() const { return u64; }

    static EvalVal makeVal(TypeTable::Id t, TypeTable *typeTable);
    static EvalVal makeZero(TypeTable::Id t, NamePool *namePool, TypeTable *typeTable);
    // use NodeVal::copyNoRef unless sure attrs aren't needed
    static EvalVal copyNoRef(const EvalVal &k);

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
    static bool isP(const EvalVal &val, const TypeTable *typeTable);
    static bool isStr(const EvalVal &val, const TypeTable *typeTable);
    static bool isNonNullStr(const EvalVal &val, const TypeTable *typeTable);
    static bool isAnyP(const EvalVal &val, const TypeTable *typeTable);
    static bool isArr(const EvalVal &val, const TypeTable *typeTable);
    static bool isTuple(const EvalVal &val, const TypeTable *typeTable);
    static bool isDataType(const EvalVal &val, const TypeTable *typeTable);
    static bool isNull(const EvalVal &val, const TypeTable *typeTable);
    static bool isCallableNoValue(const EvalVal &val, const TypeTable *typeTable);

    static std::optional<std::int64_t> getValueI(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueU(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<double> getValueF(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueNonNeg(const EvalVal &val, const TypeTable *typeTable);
    static std::optional<const FuncValue*> getValueFunc(const EvalVal &val, const TypeTable *typeTable);

    static bool isImplicitCastable(const EvalVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable);
    static bool isCastable(const EvalVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable);

    static void equalizeAllRawElemTypes(EvalVal &val, const TypeTable *typeTable);
};