#pragma once

#include <unordered_map>
#include <optional>
#include "NamePool.h"

enum class Meaningful {
    MAIN,
    CN,
    ASTERISK,
    SQUARE,
    ELLIPSIS,
    UNKNOWN
};

enum class Keyword {
    SYM,
    CAST,
    BLOCK,
    EXIT,
    LOOP,
    PASS,
    FNC,
    RET,
    MAC,
    EVAL,
    IMPORT,
    UNKNOWN
};

enum class Oper {
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    SHL,
    SHR,
    BIT_AND,
    BIT_XOR,
    BIT_OR,
    EQ,
    NEQ,
    LT,
    LTEQ,
    GT,
    GTEQ,
    ASGN,
    NOT,
    BIT_NOT,
    IND,
    DOT,
    UNKNOWN
};

struct OperInfo {
    bool unary = false;
    bool binary = true;
    bool variadic = true;
    bool assignment = false;
    bool l_assoc = true;
};

extern std::unordered_map<NamePool::Id, Meaningful> meaningfuls;
extern std::unordered_map<NamePool::Id, Keyword> keywords;
extern std::unordered_map<NamePool::Id, Oper> opers;
extern const std::unordered_map<Oper, OperInfo> operInfos;

std::optional<Meaningful> getMeaningful(NamePool::Id name) {
    auto loc = meaningfuls.find(name);
    if (loc == meaningfuls.end()) return nullopt;
    return loc->second;
}

std::optional<Keyword> getKeyword(NamePool::Id name) {
    auto loc = keywords.find(name);
    if (loc == keywords.end()) return nullopt;
    return loc->second;
}

std::optional<Oper> getOper(NamePool::Id name) {
    auto loc = opers.find(name);
    if (loc == opers.end()) return nullopt;
    return loc->second;
}