#pragma once

#include "NamePool.h"
#include "StringPool.h"

struct Token {
    enum Type {
        T_NUM,
        T_FNUM,
        T_CHAR,
        T_BVAL,
        T_STRING,
        T_NULL,
        T_SEMICOLON,
        T_COLON,
        T_DOUBLE_COLON,
        T_BACKSLASH,
        T_COMMA,
        T_BRACE_L_REG,
        T_BRACE_R_REG,
        T_BRACE_L_CUR,
        T_BRACE_R_CUR,
        T_ID,
        T_END,
        T_UNKNOWN
    };

    Type type;
    union {
        std::int64_t num;
        double fnum;
        char ch;
        bool bval;
        NamePool::Id nameId;
        StringPool::Id stringId;
    };
};
