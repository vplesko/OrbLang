#pragma once

#include "CompilationMessages.h"
#include "Lexer.h"
#include "NodeVal.h"

class Parser {
    StringPool *stringPool;
    Lexer *lex;
    CompilationMessages *msgs;

    const Token& peek() const;
    Token next();
    CodeLoc loc() const;
    
    bool match(Token::Type type);
    bool matchOrError(Token::Type type);
    bool matchCloseBraceOrError(Token openBrace);

    NodeVal makeEmpty() const;
    NodeVal makeEmpty(CodeLoc codeLoc) const;
    // Returns false on error, otherwise true.
    bool parseTypeAttr(NodeVal &node);

    NodeVal parseType();
    NodeVal parseTerm();

public:
    Parser(StringPool *stringPool, CompilationMessages *msgs);

    void setLexer(Lexer *lex_) { lex = lex_; }
    Lexer* getLexer() const { return lex; }

    NodeVal parseNode();

    bool isOver() const { return peek().type == Token::T_END; }
};