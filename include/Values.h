#pragma once

#include <string>
#include "StringPool.h"
#include "TypeTable.h"

/*
UntypedVal is needed to represent a literal value whose exact type is yet unknown.

Let's say we interpret an integer literal as i32.
Then the user couldn't do this: i8 i = 0;

Let's say we interpret it as the shortest type it can fit.
Then this would overflow: i32 i = 250 + 10;

Therefore, we need this intermediate value holder.

NOTE
Does not hold uint values. Users can overcome this by casting.
*/
struct UntypedVal {
    enum Type {
        T_NONE,
        T_SINT,
        T_FLOAT,
        T_CHAR,
        T_STRING,
        T_BOOL,
        T_NULL
    };

    Type type;
    union {
        int64_t val_si;
        double val_f;
        char val_c;
        bool val_b;
        StringPool::Id val_str;
    };

    static std::size_t getStringLen(const std::string &str) { return str.size()+1; }
};

struct ExprGenPayload {
    TypeTable::Id type;
    llvm::Value *val = nullptr;
    llvm::Value *ref = nullptr;
    UntypedVal untyVal = { .type = UntypedVal::T_NONE };

    bool isUntyVal() const { return untyVal.type != UntypedVal::T_NONE; }
    void resetUntyVal() { untyVal.type = UntypedVal::T_NONE; }

    // checks if val is invalid
    bool valBroken() const { return val == nullptr; }
    // checks if ref is invalid
    bool refBroken() const { return ref == nullptr; }
    // checks if both val and unty val are invalid
    bool valueBroken() const { return valBroken() && !isUntyVal(); }
};

struct CompilerAction {
    enum class Kind {
        kNoAction,
        kImport
    };

    Kind kind;

    StringPool::Id file;

    CompilerAction() : kind(Kind::kNoAction) {}
};