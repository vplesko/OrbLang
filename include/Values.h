#pragma once

#include <string>
#include "Token.h"
#include "StringPool.h"
#include "TypeTable.h"

struct UntypedVal {
    enum class Kind {
        kNone, // TODO get rid of this one
        kSint,
        kFloat,
        kChar,
        kString,
        kBool,
        kNull
    };

    Kind kind;
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

struct ExprGenPayload {
    TypeTable::Id type;
    llvm::Value *val = nullptr;
    llvm::Value *ref = nullptr;
    UntypedVal untyVal = { .kind = UntypedVal::Kind::kNone };

    bool isUntyVal() const { return untyVal.kind != UntypedVal::Kind::kNone; }
    void resetUntyVal() { untyVal.kind = UntypedVal::Kind::kNone; }

    // checks if val is invalid
    bool valBroken() const { return val == nullptr; }
    // checks if ref is invalid
    bool refBroken() const { return ref == nullptr; }
    // checks if both val and unty val are invalid
    bool valueBroken() const { return valBroken() && !isUntyVal(); }
};

struct CompilerAction {
    enum class Kind {
        kNoAction,
        kImport
    };

    Kind kind;
    StringPool::Id file;

    CompilerAction() : kind(Kind::kNoAction) {}
};