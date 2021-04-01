#pragma once

#include <optional>
#include "NamePool.h"

struct VarId {
private:
    friend class SymbolTable;

    std::optional<std::size_t> callable;
    std::size_t block;
    std::size_t index;

public:
    friend bool operator==(const VarId &l, const VarId &r)
    { return l.callable == r.callable && l.block == r.block && l.index == r.index; }

    friend bool operator!=(const VarId &l, const VarId &r)
    { return !(l == r); }
};

struct FuncId {
private:
    friend class SymbolTable;

    NamePool::Id name;
    std::size_t index;

public:
    friend bool operator==(const FuncId &l, const FuncId &r)
    { return l.name == r.name && l.index == r.index; }

    friend bool operator!=(const FuncId &l, const FuncId &r)
    { return !(l == r); }
};

struct MacroId {
private:
    friend class SymbolTable;

    NamePool::Id name;
    std::size_t index;

public:
    friend bool operator==(const MacroId &l, const MacroId &r)
    { return l.name == r.name && l.index == r.index; }

    friend bool operator!=(const MacroId &l, const MacroId &r)
    { return !(l == r); }
};