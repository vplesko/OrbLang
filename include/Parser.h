#pragma once

#include "CompileMessages.h"
#include "Lexer.h"
#include "NodeVal.h"

class Parser {
    StringPool *stringPool;
    Lexer *lex;
    CompileMessages *msgs;

    const Token& peek() const;
    Token next();
    CodeLoc loc() const;
    
    bool match(Token::Type type);
    bool matchOrError(Token::Type type);
    bool matchCloseBraceOrError(Token openBrace);

    NodeVal makeEmpty();
    // Returns false on error, otherwise true.
    bool parseTypeAnnot(NodeVal &node);

    NodeVal parseType();
    NodeVal parseTerm();

public:
    Parser(StringPool *stringPool, CompileMessages *msgs);

    void setLexer(Lexer *lex_) { lex = lex_; }
    Lexer* getLexer() const { return lex; }

    NodeVal parseNode();

    bool isOver() const { return peek().type == Token::T_END; }
};