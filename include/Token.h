#pragma once

#include <unordered_map>
#include "NamePool.h"

struct Token {
    enum Type {
        T_NUM,
        T_FNUM,
        T_BVAL,
        T_NULL,
        T_OPER,
        T_COMMA,
        T_SEMICOLON,
        T_QUESTION,
        T_COLON,
        T_FNC,
        T_BRACE_L_REG,
        T_BRACE_R_REG,
        T_BRACE_L_CUR,
        T_BRACE_R_CUR,
        T_BRACE_L_SQR,
        T_BRACE_R_SQR,
        T_ID,
        T_IF,
        T_ELSE,
        T_FOR,
        T_WHILE,
        T_DO,
        T_BREAK,
        T_CONTINUE,
        T_SWITCH,
        T_CASE,
        T_DEFAULT,
        T_RET,
        T_END,
        T_UNKNOWN
    };

    enum Oper {
        O_ADD,
        O_SUB,
        O_MUL,
        O_DIV,
        O_REM,
        O_SHL,
        O_SHR,
        O_BIT_AND,
        O_BIT_XOR,
        O_BIT_OR,
        O_AND,
        O_OR,
        O_EQ,
        O_NEQ,
        O_LT,
        O_LTEQ,
        O_GT,
        O_GTEQ,
        O_ASGN,
        O_ADD_ASGN,
        O_SUB_ASGN,
        O_MUL_ASGN,
        O_DIV_ASGN,
        O_REM_ASGN,
        O_SHL_ASGN,
        O_SHR_ASGN,
        O_BIT_AND_ASGN,
        O_BIT_XOR_ASGN,
        O_BIT_OR_ASGN,
        O_INC,
        O_DEC,
        O_NOT,
        O_BIT_NOT
    };

    Type type;
    union {
        long int num;
        double fnum;
        bool bval;
        Oper op;
        NamePool::Id nameId;
    };
};

typedef int OperPrec;
struct OperInfo {
    OperPrec prec;
    bool l_assoc = true;
    bool unary = false;
    bool binary = true;
    bool assignment = false;
};

extern const OperPrec minOperPrec;
extern const std::unordered_map<Token::Oper, OperInfo> operInfos;
extern const std::unordered_map<std::string, Token> keywords;
