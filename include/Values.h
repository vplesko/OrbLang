#pragma once

#include <string>
#include "Token.h"
#include "StringPool.h"
#include "TypeTable.h"

struct CompilerAction {
    enum class Kind {
        kNone,
        kCodegen,
        kImport
    };

    Kind kind = Kind::kNone;
    StringPool::Id file;

    CompilerAction() {}
    CompilerAction(Kind k) : kind(k) {}
};

// TODO get rid of these in favor of compile-time values
// TODO! LiteralVal for passing from Parser to others, KnownVal for evaulated typed values
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
        kUntyVal,
        kType
    };

    Kind kind = Kind::kInvalid;
    union {
        Token::Type keyword;
        Token::Oper oper;
        NamePool::Id id;
        Token::Attr attribute;
        LlvmVal llvmVal;
        UntypedVal untyVal;
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
    bool isLlvmVal() const { return kind == Kind::kLlvmVal; }
    bool isUntyVal() const { return kind == Kind::kUntyVal; }
    bool isType() const { return kind == Kind::kType; }
    bool isInvalid() const { return kind == Kind::kInvalid; }

    // returns true if this is not unty val nor llvm val with valid value
    bool valueBroken() const { return
        (!isLlvmVal() && !isUntyVal()) ||
        (isLlvmVal() && llvmVal.valBroken()) ||
        (isUntyVal() && untyVal.kind == UntypedVal::Kind::kNone);
    }
};