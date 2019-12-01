#include "Lexer.h"
#include <cstdlib>
using namespace std;

const OperPrec minOperPrec = -1000;
const unordered_map<Token::Oper, OperInfo> operInfos = {
    {Token::O_ASGN, {1, false}},
    {Token::O_EQ, {2}},
    {Token::O_NEQ, {2}},
    {Token::O_BIT_OR, {3}},
    {Token::O_BIT_XOR, {4}},
    {Token::O_BIT_AND, {5}},
    {Token::O_LT, {6}},
    {Token::O_LTEQ, {6}},
    {Token::O_GT, {6}},
    {Token::O_GTEQ, {6}},
    {Token::O_SHL, {7}},
    {Token::O_SHR, {7}},
    {Token::O_ADD, {8, true, true, true}},
    {Token::O_SUB, {8, true, true, true}},
    {Token::O_MUL, {9}},
    {Token::O_DIV, {9}},
    {Token::O_REM, {9}},
    {Token::O_INC, {10, false, true, false}},
    {Token::O_DEC, {10, false, true, false}},
    {Token::O_NOT, {10, false, true, false}},
    {Token::O_BIT_NOT, {10, false, true, false}}
};

const unordered_map<string, Token::Type> keywords = {
    {"true", {Token::T_TRUE}},
    {"false", {Token::T_FALSE}},
    {"fnc", {Token::T_FNC}},
    {"if", {Token::T_IF}},
    {"else", {Token::T_ELSE}},
    {"for", {Token::T_FOR}},
    {"while", {Token::T_WHILE}},
    {"do", {Token::T_DO}},
    {"ret", {Token::T_RET}}
};

Lexer::Lexer(NamePool *namePool) : namePool(namePool) {
}

void Lexer::start(std::istream &istr) {
    in = &istr;
    ln = 0;
    col = 0;
    ch = 0; // not EOF
    tok.type = Token::T_NUM; // not END

    nextCh();
    next();
}

char Lexer::nextCh() {
    if (over()) return ch;

    char old = ch;

    ++col;

    if (col > line.size()) {
        if (!getline(*in, line)) ch = EOF;
        else ++ln;
        col = 0;
    }

    if (!over()) {
        ch = col == line.size() ? '\n' : line[col];
    }

    return old;
}

void Lexer::skipLine() {
    if (over()) return;

    if (!getline(*in, line)) {
        ch = EOF;
        return;
    }

    ++ln;
    col = 0;
    ch = col == line.size() ? '\n' : line[col];
}

Token Lexer::next() {
    if (tok.type == Token::T_END) return tok;

    Token old = tok;

    while (true) {
        char ch;
        do {
            ch = nextCh();
        } while (isspace(ch));
        
        if (over()) {
            tok.type = Token::T_END;
            return old;
        }

        if (ch == '/' && peekCh() == '/') {
            skipLine();
            continue;
        }
        if (ch == '/' && peekCh() == '*') {
            nextCh();
            do {
                do {
                    ch = nextCh();
                } while (ch != '*' && !over());
            } while (peekCh() != '/' && !over());

            if (over()) {
                tok.type = Token::T_UNKNOWN;
                return tok; // unclosed comment error, so skip old token
            }

            nextCh(); // eat '/'
            continue;
        }
        
        if (ch >= '0' && ch <= '9') {
            tok.type = Token::T_NUM;
            int l = col-1;
            while (peekCh() >= '0' && peekCh() <= '9') nextCh();
            
            char *end = nullptr;
            tok.num = strtol(line.substr(l, col-l).c_str(), &end, 10);
            if (*end) tok.type = Token::T_UNKNOWN;
        } else if (ch == '+') {
            if (peekCh() == '+') {
                nextCh();
                tok = {Token::T_OPER, Token::O_INC};
            } else {
                tok = {Token::T_OPER, Token::O_ADD};
            }
        } else if (ch == '-') {
            if (peekCh() == '-') {
                nextCh();
                tok = {Token::T_OPER, Token::O_DEC};
            } else {
                tok = {Token::T_OPER, Token::O_SUB};
            }
        } else if (ch == '*') {
            tok = {Token::T_OPER, Token::O_MUL};
        } else if (ch == '/') {
            tok = {Token::T_OPER, Token::O_DIV};
        } else if (ch == '%') {
            tok = {Token::T_OPER, Token::O_REM};
        } else if (ch == '&') {
            tok = {Token::T_OPER, Token::O_BIT_AND};
        } else if (ch == '^') {
            tok = {Token::T_OPER, Token::O_BIT_XOR};
        } else if (ch == '|') {
            tok = {Token::T_OPER, Token::O_BIT_OR};
        } else if (ch == '=') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_EQ};
            } else {
                tok = {Token::T_OPER, Token::O_ASGN};
            }
        } else if (ch == '!') {
            if (peekCh() != '=') {
                tok = {Token::T_OPER, Token::O_NOT};
            } else {
                nextCh();
                tok = {Token::T_OPER, Token::O_NEQ};
            }
        } else if (ch == '~') {
            tok = {Token::T_OPER, Token::O_BIT_NOT};
        } else if (ch == '<') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_LTEQ};
            } else if (peekCh() == '<') {
                nextCh();
                tok = {Token::T_OPER, Token::O_SHL};
            } else {
                tok = {Token::T_OPER, Token::O_LT};
            }
        } else if (ch == '>') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_GTEQ};
            } else if (peekCh() == '>') {
                nextCh();
                tok = {Token::T_OPER, Token::O_SHR};
            } else {
                tok = {Token::T_OPER, Token::O_GT};
            }
        } else if (ch == ',') {
            tok = {Token::T_COMMA};
        } else if (ch == ';') {
            tok = {Token::T_SEMICOLON};
        } else if (ch == '(') {
            tok = {Token::T_BRACE_L_REG};
        } else if (ch == ')') {
            tok = {Token::T_BRACE_R_REG};
        } else if (ch == '{') {
            tok = {Token::T_BRACE_L_CUR};
        } else if (ch == '}') {
            tok = {Token::T_BRACE_R_CUR};
        } else if (isalpha(ch) || ch == '_') {
            int l = col-1;
            while (isalnum(peekCh()) || peekCh() == '_') nextCh();
            string id = line.substr(l, col-l);

            auto loc = keywords.find(id);
            if (loc != keywords.end()) {
                tok.type = loc->second;
            } else {
                tok.type = Token::T_ID;
                tok.nameId = namePool->add(id);
            }
        } else {
            tok.type = Token::T_UNKNOWN;
        }

        return old;
    }
}

// Eats the next token and returns whether it matches the type.
bool Lexer::match(Token::Type type) {
    return next().type == type;
}