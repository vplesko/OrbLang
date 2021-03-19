#include "Lexer.h"
#include <cstdlib>
#include <algorithm>
#include <sstream>
using namespace std;

const string numLitChars = "0123456789abcdefABCDEF.xXpP_";
const string idSpecialChars = "=+-*/%<>&|^!~[]._?";

bool isValidIdStart(char ch) {
    return isalnum(ch) || idSpecialChars.find(ch) != idSpecialChars.npos;
}

Lexer::Lexer(NamePool *namePool, StringPool *stringPool, CompilationMessages *msgs, const std::string &file)
    : namePool(namePool), stringPool(stringPool), msgs(msgs), in(file) {
    ln = 0;
    col = 0;
    ch = 0; // not EOF
    tok.type = Token::T_NUM; // not END
    codeLocPoint.file = stringPool->add(file);
}

bool Lexer::start() {
    if (!in.is_open()) return false;

    nextCh();
    next();
    return true;
}

char Lexer::nextCh() {
    if (over()) return ch;

    char old = ch;

    ++col;

    if (col > line.size()) {
        if (!getline(in, line)) ch = EOF;
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

    if (!getline(in, line)) {
        ch = EOF;
        return;
    }

    ++ln;
    col = 0;
    ch = col == line.size() ? '\n' : line[col];
}

void Lexer::lexNum(CodeIndex from) {
    CodeIndex l = from;
    while (numLitChars.find(peekCh()) != numLitChars.npos) nextCh();
    CodeIndex r = col-1;

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
            if (end != &*lit.end() || errno == ERANGE) tok.type = Token::T_UNKNOWN;
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
            tok.num = strtoll(lit.c_str(), &end, base);
            if (end != &*lit.end() || errno == ERANGE) tok.type = Token::T_UNKNOWN;
        }
    }

    if (isValidIdStart(peekCh())) {
        tok.type = Token::T_UNKNOWN;
    }
}

// TODO check tokens separated (eg. by whitespace)
Token Lexer::next() {
    if (tok.type == Token::T_END) return tok;

    Token old = move(tok);

    while (true) {
        char ch;
        do {
            codeLocPoint.ln = ln;
            codeLocPoint.col = col+1; // text editors are 1-indexed
            ch = nextCh();
        } while (isspace(ch));
        
        if (over()) {
            tok.type = Token::T_END;
            return old;
        }

        if (ch == '#') {
            if (peekCh() == '$') {
                nextCh();
                do {
                    do {
                        ch = nextCh();
                    } while (ch != '$' && !over());
                } while (peekCh() != '#' && !over());

                if (over()) {
                    CodeLocPoint codeLocPointEnd = codeLocPoint;
                    codeLocPointEnd.col += 1;

                    tok.type = Token::T_UNKNOWN;
                    msgs->errorUnclosedMultilineComment({codeLocPoint, codeLocPointEnd});
                    return tok; // unclosed comment error, so skip old token
                }

                nextCh(); // eat '#'
                continue;
            } else {
                skipLine();
                continue;
            }
        }
        
        if (isdigit(ch)) {
            lexNum(col-1);
        } else if (ch == '+' && isdigit(peekCh())) {
            lexNum(col);
        } else if (ch == '-' && isdigit(peekCh())) {
            lexNum(col);
            if (tok.type == Token::T_NUM) tok.num *= -1;
            else if (tok.type == Token::T_FNUM) tok.fnum *= -1.0;
        } else if (ch == ';') {
            tok = {.type=Token::T_SEMICOLON};
        } else if (ch == ':') {
            if (peekCh() == ':') {
                nextCh();
                tok = {.type=Token::T_DOUBLE_COLON};
            } else {
                tok = {.type=Token::T_COLON};
            }
        } else if (ch == '(') {
            tok = {.type=Token::T_BRACE_L_REG};
        } else if (ch == ')') {
            tok = {.type=Token::T_BRACE_R_REG};
        } else if (ch == '{') {
            tok = {.type=Token::T_BRACE_L_CUR};
        } else if (ch == '}') {
            tok = {.type=Token::T_BRACE_R_CUR};
        } else if (ch == '\'') {
            UnescapePayload unesc = unescape(line, col-1, true);

            if (unesc.success == false || unesc.unescaped.size() != 1) {
                tok.type = Token::T_UNKNOWN;
                msgs->errorUnknown({codeLocPoint, codeLocPoint});
                return tok; // unclosed char literal error, so skip old token
            } else {
                tok.type = Token::T_CHAR;
                tok.ch = unesc.unescaped[0];
            }

            col = unesc.nextIndex-1;
            ch = line[col];
            nextCh();
        } else if (ch == '\"') {
            UnescapePayload unesc = unescape(line, col-1, false);

            if (unesc.success == false) {
                tok.type = Token::T_UNKNOWN;
                msgs->errorUnknown({codeLocPoint, codeLocPoint});
                return tok; // unclosed string literal error, so skip old token
            } else {
                tok.type = Token::T_STRING;
                tok.stringId = stringPool->add(unesc.unescaped);
            }

            col = unesc.nextIndex-1;
            ch = line[col];
            nextCh();
        } else if (ch == '\\') {
            tok.type = Token::T_BACKSLASH;
        } else if (ch == ',') {
            tok.type = Token::T_COMMA;
        } else if (isalnum(ch) || idSpecialChars.find(ch) != idSpecialChars.npos) {
            int l = col-1;
            while (isValidIdStart(peekCh())) {
                ch = nextCh();
            }

            string id = line.substr(l, col-l);

            if (id == "true" || id == "false") {
                tok.type = Token::T_BVAL;
                tok.bval = id == "true";
            } else if (id == "null") {
                tok.type = Token::T_NULL;
            } else {
                tok.type = Token::T_ID;
                tok.nameId = namePool->add(id);
            }
        } else {
            tok.type = Token::T_UNKNOWN;
        }

        if (tok.type == Token::T_UNKNOWN) {
            msgs->errorBadToken({codeLocPoint, loc()});
        }

        return old;
    }
}

// Eats the next token and returns whether it matches the type.
bool Lexer::match(Token::Type type) {
    return next().type == type;
}