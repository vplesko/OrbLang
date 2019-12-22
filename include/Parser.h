#pragma once

#include <memory>
#include <queue>
#include "Lexer.h"
#include "AST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class Parser {
    NamePool *namePool;
    SymbolTable *symbolTable;
    Lexer *lex;

    bool panic;

    // needed to due ambiguity between decl and expr in some starting tokens
    std::queue<Token> tokRewind;

    Token peek() const;
    Token next();
    bool match(Token::Type type);
    bool mismatch(Token::Type type);
    template<typename T> bool broken(const T &x);

    std::unique_ptr<ExprAST> call();
    std::unique_ptr<ExprAST> prim();
    std::unique_ptr<ExprAST> expr(std::unique_ptr<ExprAST> lhs, OperPrec min_prec);
    std::unique_ptr<ExprAST> expr();
    std::unique_ptr<TypeAST> type();
    std::unique_ptr<DeclAST> decl();
    std::unique_ptr<StmntAST> simple();
    std::unique_ptr<StmntAST> if_stmnt();
    std::unique_ptr<StmntAST> for_stmnt();
    std::unique_ptr<StmntAST> while_stmnt();
    std::unique_ptr<StmntAST> do_while_stmnt();
    std::unique_ptr<StmntAST> ret();
    std::unique_ptr<StmntAST> stmnt();
    std::unique_ptr<BlockAST> block();
    std::unique_ptr<BaseAST> func();

public:
    Parser(NamePool *namePool, SymbolTable *symbolTable, Lexer *lexer);

    std::unique_ptr<BaseAST> parseNode();

    bool isOver() const { return peek().type == Token::T_END; }
    bool isPanic() const { return panic; }
};

// panics if pointer x is null; returns panic
template<typename T>
bool Parser::broken(const T &x) {
    if (x == nullptr) panic = true;
    return panic;
}