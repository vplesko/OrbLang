#pragma once

#include <string>
#include "NamePool.h"
#include "StringPool.h"

struct LiteralVal {
    enum class Kind {
        kNone,
        kId,
        kSint,
        kFloat,
        kChar,
        kBool,
        kString,
        kNull
    };

    Kind kind = Kind::kNone;
    union {
        NamePool::Id val_id;
        std::int64_t val_si;
        double val_f;
        char val_c;
        bool val_b;
        StringPool::Id val_str;
    };
    bool escaped = false;

    static std::size_t getStringLen(const std::string &str) { return str.size()+1; }
};