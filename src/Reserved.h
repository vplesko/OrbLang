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
    TUP,
    TYPE_OF,
    LEN_OF,
    IS_DEF,
    IMPORT,
    UNKNOWN
};

enum class Oper {
    ASGN,
    NOT,
    BIT_NOT,
    EQ,
    NE,
    LT,
    LE,
    GT,
    GE,
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    BIT_AND,
    BIT_OR,
    BIT_XOR,
    SHL,
    SHR,
    IND,
    DOT,
    UNKNOWN
};

struct OperInfo {
    bool unary = false;
    bool binary = false;
    bool comparison = false;
};

extern std::unordered_map<NamePool::Id, Meaningful> meaningfuls;
extern std::unordered_map<NamePool::Id, Keyword> keywords;
extern std::unordered_map<NamePool::Id, Oper> opers;
extern const std::unordered_map<Oper, OperInfo> operInfos;

bool isMeaningful(NamePool::Id name);
std::optional<Meaningful> getMeaningful(NamePool::Id name);
std::optional<NamePool::Id> getMeaningfulNameId(Meaningful m);
bool isMeaningful(NamePool::Id name, Meaningful m);
bool isKeyword(NamePool::Id name);
std::optional<Keyword> getKeyword(NamePool::Id name);
std::optional<NamePool::Id> getKeywordNameId(Keyword k);
bool isKeyword(NamePool::Id name, Keyword k);
bool isOper(NamePool::Id name);
std::optional<Oper> getOper(NamePool::Id name);
bool isOper(NamePool::Id name, Oper o);
bool isReserved(NamePool::Id name);
bool isTypeDescr(Meaningful m);
bool isTypeDescr(NamePool::Id name);