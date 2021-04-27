#pragma once

#include <optional>
#include <unordered_map>
#include "NamePool.h"

enum class Meaningful {
    MAIN,
    CN,
    ASTERISK,
    SQUARE,
    TYPE,
    UNKNOWN
};

enum class Keyword {
    SYM,
    CAST,
    BLOCK,
    EXIT,
    LOOP,
    PASS,
    EXPLICIT,
    DATA,
    FNC,
    RET,
    MAC,
    EVAL,
    TYPE_OF,
    LEN_OF,
    SIZE_OF,
    IS_DEF,
    ATTR_OF,
    ATTR_IS_DEF,
    IS_EVAL,
    IMPORT,
    MESSAGE,
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
    UNKNOWN
};

struct OperInfo {
    bool unary = false;
    bool binary = false;
    bool comparison = false;
};

extern std::unordered_map<NamePool::Id, Meaningful, NamePool::Id::Hasher> meaningfuls;
extern std::unordered_map<NamePool::Id, Keyword, NamePool::Id::Hasher> keywords;
extern std::unordered_map<NamePool::Id, Oper, NamePool::Id::Hasher> opers;
extern const std::unordered_map<Oper, OperInfo> operInfos;

bool isMeaningful(NamePool::Id name);
std::optional<Meaningful> getMeaningful(NamePool::Id name);
NamePool::Id getMeaningfulNameId(Meaningful m);
bool isMeaningful(NamePool::Id name, Meaningful m);
bool isKeyword(NamePool::Id name);
std::optional<Keyword> getKeyword(NamePool::Id name);
NamePool::Id getKeywordNameId(Keyword k);
bool isKeyword(NamePool::Id name, Keyword k);
bool isOper(NamePool::Id name);
std::optional<Oper> getOper(NamePool::Id name);
NamePool::Id getOperNameId(Oper o);
bool isOper(NamePool::Id name, Oper o);
bool isReserved(NamePool::Id name);
bool isTypeDescrDecor(Meaningful m);
bool isTypeDescrDecor(NamePool::Id name);