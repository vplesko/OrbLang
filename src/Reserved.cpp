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

bool isMeaningful(NamePool::Id name) {
    return meaningfuls.contains(name);
}

optional<Meaningful> getMeaningful(NamePool::Id name) {
    auto loc = meaningfuls.find(name);
    if (loc == meaningfuls.end()) return nullopt;
    return loc->second;
}

bool isMeaningful(NamePool::Id name, Meaningful m) {
    optional<Meaningful> opt = getMeaningful(name);
    return opt.has_value() && opt.value() == m;
}

bool isKeyword(NamePool::Id name) {
    return keywords.contains(name);
}

optional<Keyword> getKeyword(NamePool::Id name) {
    auto loc = keywords.find(name);
    if (loc == keywords.end()) return nullopt;
    return loc->second;
}

bool isKeyword(NamePool::Id name, Keyword k) {
    optional<Keyword> opt = getKeyword(name);
    return opt.has_value() && opt.value() == k;
}

bool isOper(NamePool::Id name) {
    return opers.contains(name);
}

optional<Oper> getOper(NamePool::Id name) {
    auto loc = opers.find(name);
    if (loc == opers.end()) return nullopt;
    return loc->second;
}

bool isOper(NamePool::Id name, Oper o) {
    optional<Oper> opt = getOper(name);
    return opt.has_value() && opt.value() == o;
}