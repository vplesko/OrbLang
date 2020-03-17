#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <fstream>
#include "SymbolTable.h"
#include "Token.h"

class Lexer {
    NamePool *namePool;
    std::ifstream in;
    std::string line;
    int ln, col;
    char ch;
    Token tok;

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
};