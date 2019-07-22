#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include "SymbolTable.h"

struct Token {
    enum Type {
        T_NUM,
        T_OPER,
        T_COMMA,
        T_SEMICOLON,
        T_VAR, // TODO var types
        T_ID,
        T_END,
        T_UNKNOWN
    };

    enum Oper {
        O_ADD,
        O_SUB,
        O_MUL,
        O_DIV,
        O_ASGN
    };

    Type type;
    union {
        int num;
        Oper op;
        NamePool::Id nameId;
    };
};

typedef int OperPrec;
struct OperInfo {
    OperPrec prec;
    bool l_assoc = true;
};

extern const OperPrec minOperPrec;
extern const std::unordered_map<Token::Oper, OperInfo> operInfos;

class Lexer {
    std::istream &in;
    std::unique_ptr<NamePool> namePool;
    std::string line;
    int col;
    char ch;
    Token tok;

    char peekCh() const { return ch; }
    char nextCh();

public:
    Lexer(std::istream &istr);

    const NamePool* getNamePool() const { return namePool.get(); }

    Token peek() const { return tok; }
    Token next();
    bool match(Token::Type type);
};