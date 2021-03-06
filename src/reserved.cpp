#include "reserved.h"
#include <cassert>
using namespace std;

std::unordered_map<NamePool::Id, Meaningful, NamePool::Id::Hasher> meaningfuls;
std::unordered_map<NamePool::Id, Keyword, NamePool::Id::Hasher> keywords;
std::unordered_map<NamePool::Id, Oper, NamePool::Id::Hasher> opers;

const unordered_map<Oper, OperInfo> operInfos = {
    {Oper::ASGN, {.binary=true}},
    {Oper::NOT, {.unary=true}},
    {Oper::BIT_NOT, {.unary=true}},
    {Oper::EQ, {.binary=true, .comparison=true}},
    {Oper::NE, {.binary=true, .comparison=true}},
    {Oper::LT, {.binary=true, .comparison=true}},
    {Oper::LE, {.binary=true, .comparison=true}},
    {Oper::GT, {.binary=true, .comparison=true}},
    {Oper::GE, {.binary=true, .comparison=true}},
    {Oper::ADD, {.unary=true, .binary=true}},
    {Oper::SUB, {.unary=true, .binary=true}},
    {Oper::MUL, {.unary=true, .binary=true}},
    {Oper::DIV, {.binary=true}},
    {Oper::REM, {.binary=true}},
    {Oper::BIT_AND, {.unary=true, .binary=true}},
    {Oper::BIT_OR, {.binary=true}},
    {Oper::BIT_XOR, {.binary=true}},
    {Oper::SHL, {.binary=true}},
    {Oper::SHR, {.unary=true, .binary=true}},
    {Oper::IND, {.binary=true}}
};

bool isMeaningful(NamePool::Id name) {
    return meaningfuls.find(name) != meaningfuls.end();
}

optional<Meaningful> getMeaningful(NamePool::Id name) {
    auto loc = meaningfuls.find(name);
    if (loc == meaningfuls.end()) return nullopt;
    return loc->second;
}

NamePool::Id getMeaningfulNameId(Meaningful m) {
    for (const auto &it : meaningfuls) {
        if (it.second == m) return it.first;
    }

    assert(false && "getMeaningfulNameId failed to find!");
    return NamePool::Id();
}

bool isMeaningful(NamePool::Id name, Meaningful m) {
    optional<Meaningful> opt = getMeaningful(name);
    return opt.has_value() && opt.value() == m;
}

bool isKeyword(NamePool::Id name) {
    return keywords.find(name) != keywords.end();
}

optional<Keyword> getKeyword(NamePool::Id name) {
    auto loc = keywords.find(name);
    if (loc == keywords.end()) return nullopt;
    return loc->second;
}

NamePool::Id getKeywordNameId(Keyword k) {
    for (const auto &it : keywords) {
        if (it.second == k) return it.first;
    }

    assert(false && "getKeywordNameId failed to find!");
    return NamePool::Id();
}

bool isKeyword(NamePool::Id name, Keyword k) {
    optional<Keyword> opt = getKeyword(name);
    return opt.has_value() && opt.value() == k;
}

bool isOper(NamePool::Id name) {
    return opers.find(name) != opers.end();
}

optional<Oper> getOper(NamePool::Id name) {
    auto loc = opers.find(name);
    if (loc == opers.end()) return nullopt;
    return loc->second;
}

NamePool::Id getOperNameId(Oper o) {
    for (const auto &it : opers) {
        if (it.second == o) return it.first;
    }

    assert(false && "getOperNameId failed to find!");
    return NamePool::Id();
}

bool isOper(NamePool::Id name, Oper o) {
    optional<Oper> opt = getOper(name);
    return opt.has_value() && opt.value() == o;
}

bool isReserved(NamePool::Id name) {
    return isKeyword(name) || isOper(name) || isTypeDescrDecor(name);
}

bool isTypeDescrDecor(Meaningful m) {
    return m == Meaningful::CN || m == Meaningful::ASTERISK || m == Meaningful::SQUARE;
}

bool isTypeDescrDecor(NamePool::Id name) {
    optional<Meaningful> m = getMeaningful(name);
    if (!m.has_value()) return false;
    return isTypeDescrDecor(m.value());
}