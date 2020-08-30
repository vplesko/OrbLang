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

bool isMeaningful(NamePool::Id name);
std::optional<Meaningful> getMeaningful(NamePool::Id name);
bool isMeaningful(NamePool::Id name, Meaningful m);
bool isKeyword(NamePool::Id name);
std::optional<Keyword> getKeyword(NamePool::Id name);
bool isKeyword(NamePool::Id name, Keyword k);
bool isOper(NamePool::Id name);
std::optional<Oper> getOper(NamePool::Id name);
bool isOper(NamePool::Id name, Oper o);