#pragma once

#include <memory>
#include <vector>
#include "Lexer.h"
#include "AST.h"
#include "CompileMessages.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class Parser {
    NamePool *namePool;
    SymbolTable *symbolTable;
    Lexer *lex;
    CompileMessages *msgs;

    TypeTable* getTypeTable() { return symbolTable->getTypeTable(); }
    const TypeTable* getTypeTable() const { return symbolTable->getTypeTable(); }

    const Token& peek() const;
    Token next();
    bool match(Token::Type type);
    bool matchOrError(Token::Type type);
    CodeLoc loc() const;

    std::unique_ptr<AstNode> makeEmptyTerm();

    std::unique_ptr<AstNode> parseType();
    std::unique_ptr<AstNode> parseTerm();

public:
    Parser(NamePool *namePool, SymbolTable *symbolTable, CompileMessages *msgs);

    void setLexer(Lexer *lex_) { lex = lex_; }
    Lexer* getLexer() const { return lex; }

    std::unique_ptr<AstNode> parseNode();

    bool isOver() const { return peek().type == Token::T_END; }
};