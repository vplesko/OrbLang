#include "Reserved.h"
using namespace std;

std::unordered_map<NamePool::Id, Meaningful> meaningfuls;
std::unordered_map<NamePool::Id, Keyword> keywords;
std::unordered_map<NamePool::Id, Oper> opers;

const unordered_map<Oper, OperInfo> operInfos = {
    {Oper::ASGN, {.assignment=true, .l_assoc=false}},
    {Oper::BIT_OR, {}},
    {Oper::BIT_XOR, {.variadic=false}},
    {Oper::BIT_AND, {.unary=true}},
    {Oper::EQ, {.variadic=false}},
    {Oper::NEQ, {.variadic=false}},
    {Oper::LT, {.variadic=false}},
    {Oper::LTEQ, {.variadic=false}},
    {Oper::GT, {.variadic=false}},
    {Oper::GTEQ, {.variadic=false}},
    {Oper::SHL, {}},
    {Oper::SHR, {}},
    {Oper::ADD, {.unary=true}},
    {Oper::SUB, {.unary=true}},
    {Oper::MUL, {.unary=true}},
    {Oper::DIV, {}},
    {Oper::REM, {}},
    {Oper::NOT, {.unary=true, .binary=false, .variadic=false}},
    {Oper::BIT_NOT, {.unary=true, .binary=false, .variadic=false}},
    {Oper::IND, {}},
    {Oper::DOT, {}}
};

optional<Meaningful> getMeaningful(NamePool::Id name) {
    auto loc = meaningfuls.find(name);
    if (loc == meaningfuls.end()) return nullopt;
    return loc->second;
}

optional<Keyword> getKeyword(NamePool::Id name) {
    auto loc = keywords.find(name);
    if (loc == keywords.end()) return nullopt;
    return loc->second;
}

optional<Oper> getOper(NamePool::Id name) {
    auto loc = opers.find(name);
    if (loc == opers.end()) return nullopt;
    return loc->second;
}