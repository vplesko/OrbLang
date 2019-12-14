#include "Lexer.h"
#include <cstdlib>
#include <algorithm>
#include <cmath>
using namespace std;

const string numLitChars = "0123456789abcdefABCDEF.xXeEpP_";

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
        
        if ((ch >= '0' && ch <= '9') || ch == '.') {
            int l = col-1;
            while (numLitChars.find(peekCh()) != numLitChars.npos) nextCh();
            int r = col-1;

            size_t dotIndex = line.find(".", l);
            if (dotIndex != line.npos && dotIndex <= r) {
                tok.type = Token::T_FNUM;

                string lit = line.substr(l, r-l+1);
                if (lit.size() >= 3 && lit[0] == '0' && lit[1] == '_' && (lit[2] == 'x' || lit[2] == 'X')) {
                    tok.type = Token::T_UNKNOWN;
                } else {
                    lit.erase(remove(lit.begin(), lit.end(), '_'), lit.end());
                    
                    char *end;
                    tok.fnum = strtod(lit.c_str(), &end);
                    if (end != lit.end().base() || errno == ERANGE) tok.type = Token::T_UNKNOWN;
                }
            } else {
                tok.type = Token::T_NUM;
                int base = 10;
                if (r-l+1 > 2 && line[l] == '0' && (line[l+1] == 'x' || line[l+1] == 'X')) {
                    base = 16;
                    l += 2;
                } else if (r-l+1 > 2 && line[l] == '0' && line[l+1] == 'b') {
                    base = 2;
                    l += 2;
                } else if (r-l+1 > 1 && line[l] == '0') {
                    base = 8;
                    l += 1;
                }

                string lit = line.substr(l, r-l+1);
                lit.erase(remove(lit.begin(), lit.end(), '_'), lit.end());
                if (lit.empty()) {
                    // 0_, 0__... are allowed and equal to 0
                    if (base == 8) {
                        tok.num = 0;
                    } else {
                        tok.type = Token::T_UNKNOWN;
                    }
                } else {
                    char *end;
                    tok.num = strtol(lit.c_str(), &end, base);
                    if (end != lit.end().base() || errno == ERANGE) tok.type = Token::T_UNKNOWN;
                }
            }
        } else if (ch == '+') {
            if (peekCh() == '+') {
                nextCh();
                tok = {Token::T_OPER, Token::O_INC};
            } else if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_ADD_ASGN};
            } else {
                tok = {Token::T_OPER, Token::O_ADD};
            }
        } else if (ch == '-') {
            if (peekCh() == '-') {
                nextCh();
                tok = {Token::T_OPER, Token::O_DEC};
            } else if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_SUB_ASGN};
            } else {
                tok = {Token::T_OPER, Token::O_SUB};
            }
        } else if (ch == '*') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_MUL_ASGN};
            } else {
                tok = {Token::T_OPER, Token::O_MUL};
            }
        } else if (ch == '/') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_DIV_ASGN};
            } else {
                tok = {Token::T_OPER, Token::O_DIV};
            }
        } else if (ch == '%') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_REM_ASGN};
            } else {
                tok = {Token::T_OPER, Token::O_REM};
            }
        } else if (ch == '&') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_BIT_AND_ASGN};
            } else if (peekCh() == '&') {
                nextCh();
                tok = {Token::T_OPER, Token::O_AND};
            } else {
                tok = {Token::T_OPER, Token::O_BIT_AND};
            }
        } else if (ch == '^') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_BIT_XOR_ASGN};
            } else {
                tok = {Token::T_OPER, Token::O_BIT_XOR};
            }
        } else if (ch == '|') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_BIT_OR_ASGN};
            } else if (peekCh() == '|') {
                nextCh();
                tok = {Token::T_OPER, Token::O_OR};
            } else {
                tok = {Token::T_OPER, Token::O_BIT_OR};
            }
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
                if (peekCh() == '=') {
                    nextCh();
                    tok = {Token::T_OPER, Token::O_SHL_ASGN};
                } else {
                    tok = {Token::T_OPER, Token::O_SHL};
                }
            } else {
                tok = {Token::T_OPER, Token::O_LT};
            }
        } else if (ch == '>') {
            if (peekCh() == '=') {
                nextCh();
                tok = {Token::T_OPER, Token::O_GTEQ};
            } else if (peekCh() == '>') {
                nextCh();
                if (peekCh() == '=') {
                    nextCh();
                    tok = {Token::T_OPER, Token::O_SHR_ASGN};
                } else {
                    tok = {Token::T_OPER, Token::O_SHR};
                }
            } else {
                tok = {Token::T_OPER, Token::O_GT};
            }
        } else if (ch == ',') {
            tok = {Token::T_COMMA};
        } else if (ch == ';') {
            tok = {Token::T_SEMICOLON};
        } else if (ch == '?') {
            tok = {Token::T_QUESTION};
        } else if (ch == ':') {
            tok = {Token::T_COLON};
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
            int firstAlnum = ch == '_' ? -1 : 0;
            while (isalnum(peekCh()) || peekCh() == '_') {
                if (isalnum(peekCh()) && firstAlnum < 0) firstAlnum = col-l;
                nextCh();
            }

            if (firstAlnum < 0 || firstAlnum >= 2) {
                // all _ or starting with __ is not allowed
                tok.type = Token::T_UNKNOWN;
            } else {            
                string id = line.substr(l, col-l);

                auto loc = keywords.find(id);
                if (loc != keywords.end()) {
                    tok.type = loc->second;

                    if (id.compare(keywordInf) == 0) {
                        tok.fnum = INFINITY;
                    } else if (id.compare(keywordNan) == 0) {
                        tok.fnum = NAN;
                    }
                } else {
                    tok.type = Token::T_ID;
                    tok.nameId = namePool->add(id);
                }
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