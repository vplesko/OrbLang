#pragma once

#include "CompilationMessages.h"
#include "Lexer.h"
#include "NodeVal.h"
#include "TypeTable.h"

class Parser {
    StringPool *stringPool;
    Lexer *lex;
    TypeTable *typeTable;
    CompilationMessages *msgs;

    const Token& peek() const;
    Token next();
    CodeLoc loc() const;
    
    bool match(Token::Type type);
    bool matchOrError(Token::Type type);
    bool matchCloseBraceOrError(Token openBrace);

    EscapeScore parseEscapeScore();
    // Returns false on error, otherwise true.
    bool parseTypeAttr(NodeVal &node);
    // Returns false on error, otherwise true.
    bool parseAttrs(NodeVal &node);

    NodeVal parseNoSemicolon();
    NodeVal parseTerm();

public:
    Parser(StringPool *stringPool, TypeTable *typeTable, CompilationMessages *msgs);

    void setLexer(Lexer *lex_) { lex = lex_; }
    Lexer* getLexer() const { return lex; }

    NodeVal parseNode();

    bool isOver() const { return peek().type == Token::T_END; }
};