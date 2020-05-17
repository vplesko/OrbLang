#pragma once

#include <string>
#include "Token.h"
#include "StringPool.h"
#include "TypeTable.h"

struct UntypedVal {
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
        int64_t val_si;
        double val_f;
        char val_c;
        bool val_b;
        StringPool::Id val_str;
    };

    static std::size_t getStringLen(const std::string &str) { return str.size()+1; }
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
    UntypedVal val = { .kind = UntypedVal::Kind::kNone };

    TerminalVal() : kind(Kind::kEmpty) {}
    explicit TerminalVal(Token::Type k) : kind(Kind::kKeyword), keyword(k) {}
    explicit TerminalVal(Token::Oper o) : kind(Kind::kOper), oper(o) {}
    explicit TerminalVal(NamePool::Id n) : kind(Kind::kId), id(n) {}
    explicit TerminalVal(Token::Attr a) : kind(Kind::kAttribute), attribute(a) {}
    explicit TerminalVal(UntypedVal v) : kind(Kind::kVal), val(v) {}
};

struct LlvmVal {
    TypeTable::Id type;
    llvm::Value *val = nullptr;
    llvm::Value *ref = nullptr;

    bool valBroken() const { return val == nullptr; }
    bool refBroken() const { return ref == nullptr; }
};

struct NodeVal {
    enum class Kind {
        kInvalid,
        kEmpty,
        kImport,
        kLlvmVal,
        kUntyVal
    };

    Kind kind = Kind::kInvalid;
    union {
        LlvmVal llvmVal;
        UntypedVal untyVal;
        StringPool::Id file;
    };

    NodeVal() : kind(Kind::kInvalid) {}
    NodeVal(Kind kind);

    bool isLlvmVal() const { return kind == Kind::kLlvmVal; }
    bool isUntyVal() const { return kind == Kind::kUntyVal; }

    // returns true if this is not unty val nor llvm val with valid value
    bool valueBroken() const { return
        !(isLlvmVal() && !llvmVal.valBroken()) &&
        !(isUntyVal() && untyVal.kind != UntypedVal::Kind::kNone);
    }
};