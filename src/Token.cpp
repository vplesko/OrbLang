#include "Token.h"
#include <cmath>
using namespace std;

const OperPrec minOperPrec = -1000;
const unordered_map<Token::Oper, OperInfo> operInfos = {
    {Token::O_ASGN, {1, .assignment=true}},
    {Token::O_ADD_ASGN, {1, .assignment=true}},
    {Token::O_SUB_ASGN, {1, .assignment=true}},
    {Token::O_MUL_ASGN, {1, .assignment=true}},
    {Token::O_DIV_ASGN, {1, .assignment=true}},
    {Token::O_REM_ASGN, {1, .assignment=true}},
    {Token::O_SHL_ASGN, {1, .assignment=true}},
    {Token::O_SHR_ASGN, {1, .assignment=true}},
    {Token::O_BIT_AND_ASGN, {1, .assignment=true}},
    {Token::O_BIT_XOR_ASGN, {1, .assignment=true}},
    {Token::O_BIT_OR_ASGN, {1, .assignment=true}},
    {Token::O_OR, {2}},
    {Token::O_AND, {3}},
    {Token::O_BIT_OR, {4}},
    {Token::O_BIT_XOR, {5}},
    {Token::O_BIT_AND, {6, .unary=true}},
    {Token::O_EQ, {7}},
    {Token::O_NEQ, {7}},
    {Token::O_LT, {8}},
    {Token::O_LTEQ, {8}},
    {Token::O_GT, {8}},
    {Token::O_GTEQ, {8}},
    {Token::O_SHL, {9}},
    {Token::O_SHR, {9}},
    {Token::O_ADD, {10, .unary=true}},
    {Token::O_SUB, {10, .unary=true}},
    {Token::O_MUL, {11, .unary=true}},
    {Token::O_DIV, {11}},
    {Token::O_REM, {11}},
    {Token::O_INC, {12, .l_assoc=false, .unary=true, .binary=false}},
    {Token::O_DEC, {12, .l_assoc=false, .unary=true, .binary=false}},
    {Token::O_NOT, {12, .l_assoc=false, .unary=true, .binary=false}},
    {Token::O_BIT_NOT, {12, .l_assoc=false, .unary=true, .binary=false}}
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
    {"if", {Token::T_IF}},
    {"else", {Token::T_ELSE}},
    {"for", {Token::T_FOR}},
    {"while", {Token::T_WHILE}},
    {"do", {Token::T_DO}},
    {"break", {Token::T_BREAK}},
    {"continue", {Token::T_CONTINUE}},
    {"switch", {Token::T_SWITCH}},
    {"case", {Token::T_CASE}},
    {"ret", {Token::T_RET}}
};
