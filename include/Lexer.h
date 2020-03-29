#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>
#include "CodeLoc.h"
#include "SymbolTable.h"
#include "Token.h"

class Lexer {
    NamePool *namePool;
    std::ifstream in;
    std::string line;
    CodeIndex ln, col;
    char ch;
    Token tok;
    CodeLoc codeLoc;

    bool over() const { return ch == EOF; }

    char peekCh() const { return ch; }
    char nextCh();
    void skipLine();

public:
    Lexer(NamePool *namePool, const std::string &file);

    bool start();

    const Token& peek() const { return tok; }
    Token next();
    bool match(Token::Type type);

    // Returns the location of the start of the token that would be returned by next().
    CodeLoc loc() const { return codeLoc; }
};