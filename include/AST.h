#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "Lexer.h"
#include "SymbolTable.h"

struct AstTerminal {
    enum class Kind {
        kKeyword,
        kOper,
        kId,
        kAttribute,
        kVal,
        kEmpty
    };
    const Kind kind;

    union {
        const Token::Type keyword;
        const Token::Oper oper;
        const NamePool::Id id;
        const Token::Attr attribute;
    };
    const UntypedVal val = { .type = UntypedVal::T_NONE };

    AstTerminal() : kind(Kind::kEmpty) {}
    explicit AstTerminal(Token::Type k) : kind(Kind::kKeyword), keyword(k) {}
    explicit AstTerminal(Token::Oper o) : kind(Kind::kOper), oper(o) {}
    explicit AstTerminal(NamePool::Id n) : kind(Kind::kId), id(n) {}
    explicit AstTerminal(Token::Attr a) : kind(Kind::kAttribute), attribute(a) {}
    explicit AstTerminal(UntypedVal v) : kind(Kind::kVal), val(std::move(v)) {}
};

struct AstNode {
    const CodeLoc codeLoc;

    enum class Kind {
        kTuple,
        kTerminal
    };
    const Kind kind;
    
    std::vector<std::unique_ptr<AstNode>> children;
    std::unique_ptr<AstTerminal> terminal;

    std::optional<std::unique_ptr<AstNode>> type;

    AstNode(CodeLoc loc, Kind k) : codeLoc(loc), kind(k) {}
};