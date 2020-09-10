#include "Reserved.h"
using namespace std;

std::unordered_map<NamePool::Id, Meaningful> meaningfuls;
std::unordered_map<NamePool::Id, Keyword> keywords;
std::unordered_map<NamePool::Id, Oper> opers;

const unordered_map<Oper, OperInfo> operInfos = {
    {Oper::ASGN, {.binary=true}},
    {Oper::NOT, {.unary=true}},
    {Oper::BIT_NOT, {.unary=true}},
    {Oper::EQ, {.binary=true, .comparison=true}},
    {Oper::NEQ, {.binary=true, .comparison=true}},
    {Oper::LT, {.binary=true, .comparison=true}},
    {Oper::LTEQ, {.binary=true, .comparison=true}},
    {Oper::GT, {.binary=true, .comparison=true}},
    {Oper::GTEQ, {.binary=true, .comparison=true}},
    {Oper::ADD, {.unary=true, .binary=true}},
    {Oper::SUB, {.unary=true, .binary=true}},
    {Oper::MUL, {.unary=true, .binary=true}},
    {Oper::DIV, {.binary=true}},
    {Oper::REM, {.binary=true}},
    {Oper::BIT_AND, {.unary=true, .binary=true}},
    {Oper::BIT_OR, {.binary=true}},
    {Oper::BIT_XOR, {.binary=true}},
    {Oper::SHL, {.binary=true}},
    {Oper::SHR, {.binary=true}},
    {Oper::IND, {.binary=true}},
    {Oper::DOT, {.binary=true}}
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