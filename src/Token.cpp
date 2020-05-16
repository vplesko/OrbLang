#include "Token.h"
#include <cmath>
using namespace std;

const unordered_map<Token::Oper, OperInfo> operInfos = {
    {Token::O_ASGN, {.assignment=true}},
    {Token::O_ADD_ASGN, {.assignment=true}},
    {Token::O_SUB_ASGN, {.assignment=true}},
    {Token::O_MUL_ASGN, {.assignment=true}},
    {Token::O_DIV_ASGN, {.assignment=true}},
    {Token::O_REM_ASGN, {.assignment=true}},
    {Token::O_SHL_ASGN, {.assignment=true}},
    {Token::O_SHR_ASGN, {.assignment=true}},
    {Token::O_BIT_AND_ASGN, {.assignment=true}},
    {Token::O_BIT_XOR_ASGN, {.assignment=true}},
    {Token::O_BIT_OR_ASGN, {.assignment=true}},
    {Token::O_OR, {}},
    {Token::O_AND, {}},
    {Token::O_BIT_OR, {}},
    {Token::O_BIT_XOR, {}},
    {Token::O_BIT_AND, {.unary=true}},
    {Token::O_EQ, {}},
    {Token::O_NEQ, {}},
    {Token::O_LT, {}},
    {Token::O_LTEQ, {}},
    {Token::O_GT, {}},
    {Token::O_GTEQ, {}},
    {Token::O_SHL, {}},
    {Token::O_SHR, {}},
    {Token::O_ADD, {.unary=true}},
    {Token::O_SUB, {.unary=true}},
    {Token::O_MUL, {.unary=true}},
    {Token::O_DIV, {}},
    {Token::O_REM, {}},
    {Token::O_INC, {.unary=true, .binary=false}},
    {Token::O_DEC, {.unary=true, .binary=false}},
    {Token::O_NOT, {.unary=true, .binary=false}},
    {Token::O_BIT_NOT, {.unary=true, .binary=false}},
    {Token::O_IND, {}},
    {Token::O_DOT, {}}
};

const std::unordered_map<std::string, Token::Attr> attributes = {
    {"__no_name_mangle", Token::A_NO_NAME_MANGLE}
};

const unordered_map<string, Token> keywords = {
    {"true", {.type=Token::T_BVAL, .bval=true}},
    {"false", {.type=Token::T_BVAL, .bval=false}},
    {"INF", {.type=Token::T_FNUM, .fnum=INFINITY}},
    {"NAN", {.type=Token::T_FNUM, .fnum=NAN}},
    {"null", {.type=Token::T_NULL}},
    {"and", {.type=Token::T_OPER, .op=Token::O_AND}},
    {"or", {.type=Token::T_OPER, .op=Token::O_OR}},
    {"cn", {Token::T_CN}},
    {"fnc", {Token::T_FNC}},
    {"data", {Token::T_DATA}},
    {"let", {Token::T_LET}},
    {"arr", {Token::T_ARR}},
    {"cast", {Token::T_CAST}},
    {"block", {Token::T_BLOCK}},
    {"if", {Token::T_IF}},
    {"for", {Token::T_FOR}},
    {"while", {Token::T_WHILE}},
    {"do", {Token::T_DO}},
    {"break", {Token::T_BREAK}},
    {"continue", {Token::T_CONTINUE}},
    {"ret", {Token::T_RET}},
    {"import", {Token::T_IMPORT}}
};

std::string errorString(Token tok) {
    switch (tok.type) {
    case Token::T_NUM:
        return to_string(tok.num);
    case Token::T_FNUM:
        return to_string(tok.fnum);
    case Token::T_CHAR:
        return string(1, tok.ch);
    case Token::T_BVAL:
        return tok.bval ? "true" : "false";
    case Token::T_STRING:
        return "<string>";
    case Token::T_OPER:
        return errorString(tok.op);
    default:
        return errorString(tok.type);
    }
}

// TODO optimize
string errorString(Token::Type tok) {
    switch (tok) {
    case Token::T_NUM:
        return "integer value";
    case Token::T_FNUM:
        return "float value";
    case Token::T_CHAR:
        return "char value";
    case Token::T_BVAL:
        return "boolean value";
    case Token::T_STRING:
        return "string literal";
    case Token::T_OPER:
        return "operator";
    case Token::T_ID:
        return "identifier";
    case Token::T_ATTRIBUTE:
        return "attribute";
    case Token::T_SEMICOLON:
        return ";";
    case Token::T_COLON:
        return ":";
    case Token::T_ELLIPSIS:
        return "...";
    case Token::T_BRACE_L_REG:
        return "(";
    case Token::T_BRACE_R_REG:
        return ")";
    case Token::T_BRACE_L_CUR:
        return "{";
    case Token::T_BRACE_R_CUR:
        return "}";
    default:
        // do nothing
        break;
    }

    for (const auto &it : keywords) {
        if (tok == it.second.type) return it.first;
    }

    return "<forbidden>";
}

std::string errorString(Token::Oper op) {
    switch (op) {
    case Token::O_ASGN: return "=";
    case Token::O_ADD_ASGN: return "+=";
    case Token::O_SUB_ASGN: return "-=";
    case Token::O_MUL_ASGN: return "*=";
    case Token::O_DIV_ASGN: return "/=";
    case Token::O_REM_ASGN: return "%=";
    case Token::O_SHL_ASGN: return "<<=";
    case Token::O_SHR_ASGN: return ">>=";
    case Token::O_BIT_AND_ASGN: return "&=";
    case Token::O_BIT_XOR_ASGN: return "^=";
    case Token::O_BIT_OR_ASGN: return "|=";
    case Token::O_OR: return "or";
    case Token::O_AND: return "and";
    case Token::O_BIT_OR: return "|";
    case Token::O_BIT_XOR: return "^";
    case Token::O_BIT_AND: return "&";
    case Token::O_EQ: return "==";
    case Token::O_NEQ: return "!=";
    case Token::O_LT: return "<";
    case Token::O_LTEQ: return "<=";
    case Token::O_GT: return ">";
    case Token::O_GTEQ: return ">=";
    case Token::O_SHL: return "<<";
    case Token::O_SHR: return ">>";
    case Token::O_ADD: return "+";
    case Token::O_SUB: return "-";
    case Token::O_MUL: return "*";
    case Token::O_DIV: return "/";
    case Token::O_REM: return "%";
    case Token::O_INC: return "++";
    case Token::O_DEC: return "--";
    case Token::O_NOT: return "!";
    case Token::O_BIT_NOT: return "~";
    case Token::O_IND: return "[]";
    case Token::O_DOT: return ".";
    default: return "<unknown>";
    }
}

std::string errorString(Token::Attr attr) {
    for (const auto &it : attributes) {
        if (attr == it.second) return it.first;
    }

    return "<unknown>";
}