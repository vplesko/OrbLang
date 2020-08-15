#pragma once

#include <string>
#include "Token.h"
#include "StringPool.h"
#include "TypeTable.h"

struct LiteralVal {
    enum class Kind {
        kNone,
        kSint,
        kFloat,
        kChar,
        kString,
        kBool,
        kNull
    };

    Kind kind = Kind::kNone;
    union {
        std::int64_t val_si;
        double val_f;
        char val_c;
        bool val_b;
        StringPool::Id val_str;
    };

    static std::size_t getStringLen(const std::string &str) { return str.size()+1; }
};

struct KnownVal {
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
        StringPool::Id str;
    };

    KnownVal() {}
    explicit KnownVal(TypeTable::Id t) : type(t) {}

    static bool isI(const KnownVal &val, const TypeTable *typeTable);
    static bool isU(const KnownVal &val, const TypeTable *typeTable);
    static bool isF(const KnownVal &val, const TypeTable *typeTable);
    static bool isB(const KnownVal &val, const TypeTable *typeTable);
    static bool isC(const KnownVal &val, const TypeTable *typeTable);
    static bool isStr(const KnownVal &val, const TypeTable *typeTable);
    static bool isNull(const KnownVal &val, const TypeTable *typeTable);

    static std::optional<std::int64_t> getValueI(const KnownVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueU(const KnownVal &val, const TypeTable *typeTable);
    static std::optional<double> getValueF(const KnownVal &val, const TypeTable *typeTable);
    static std::optional<std::uint64_t> getValueNonNeg(const KnownVal &val, const TypeTable *typeTable);

    static bool isImplicitCastable(const KnownVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable);
};

struct TerminalVal {
    enum class Kind {
        kKeyword,
        kOper,
        kId,
        kAttribute,
        kVal,
        kEmpty
    };

    Kind kind;
    union {
        Token::Type keyword;
        Token::Oper oper;
        NamePool::Id id;
        Token::Attr attribute;
    };
    LiteralVal val = { .kind = LiteralVal::Kind::kNone };

    TerminalVal() : kind(Kind::kEmpty) {}
    explicit TerminalVal(Token::Type k) : kind(Kind::kKeyword), keyword(k) {}
    explicit TerminalVal(Token::Oper o) : kind(Kind::kOper), oper(o) {}
    explicit TerminalVal(NamePool::Id n) : kind(Kind::kId), id(n) {}
    explicit TerminalVal(Token::Attr a) : kind(Kind::kAttribute), attribute(a) {}
    explicit TerminalVal(LiteralVal v) : kind(Kind::kVal), val(v) {}
};

struct LlvmVal {
    TypeTable::Id type;
    llvm::Value *val = nullptr;
    llvm::Value *ref = nullptr;

    bool valBroken() const { return val == nullptr; }
    bool refBroken() const { return ref == nullptr; }
};

// TODO add CodeLoc to this class, refactor functions which take CodeLoc arg just for the NodeVal arg
struct NodeVal {
    enum class Kind {
        kInvalid,
        kEmpty,
        kKeyword,
        kOper,
        kId,
        kFuncId,
        kMacroId,
        kAttribute,
        kImport,
        kLlvmVal,
        kKnownVal,
        kType
    };

    Kind kind = Kind::kInvalid;
    union {
        Token::Type keyword;
        Token::Oper oper;
        NamePool::Id id;
        Token::Attr attribute;
        LlvmVal llvmVal;
        KnownVal knownVal;
        StringPool::Id file;
        TypeTable::Id type;
    };

    NodeVal() : kind(Kind::kInvalid) {}
    NodeVal(Kind kind);

    bool isEmpty() const { return kind == Kind::kEmpty; }
    bool isKeyword() const { return kind == Kind::kKeyword; }
    bool isOper() const { return kind == Kind::kOper; }
    bool isId() const { return kind == Kind::kId; }
    bool isFuncId() const { return kind == Kind::kFuncId; }
    bool isMacroId() const { return kind == Kind::kMacroId; }
    bool isAttribute() const { return kind == Kind::kAttribute; }
    bool isImport() const { return kind == Kind::kImport; }
    bool isLlvmVal() const { return kind == Kind::kLlvmVal; }
    bool isKnownVal() const { return kind == Kind::kKnownVal; }
    bool isType() const { return kind == Kind::kType; }
    bool isInvalid() const { return kind == Kind::kInvalid; }

    // returns true if this is not known val nor llvm val with valid value
    bool valueBroken() const { return
        (!isLlvmVal() && !isKnownVal()) ||
        (isLlvmVal() && llvmVal.valBroken());
    }
};