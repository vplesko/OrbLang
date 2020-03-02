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

    Token peek() const;
    Token next();
    bool match(Token::Type type);
    bool mismatch(Token::Type type);
    template<typename T> bool broken(const T &x);

    std::unique_ptr<ExprAst> prim();
    std::unique_ptr<ExprAst> expr(std::unique_ptr<ExprAst> lhs, OperPrec min_prec);
    std::unique_ptr<ExprAst> expr();
    std::unique_ptr<TypeAst> type();
    std::unique_ptr<DeclAst> decl();
    std::unique_ptr<StmntAst> simple();
    std::unique_ptr<StmntAst> if_stmnt();
    std::unique_ptr<StmntAst> for_stmnt();
    std::unique_ptr<StmntAst> while_stmnt();
    std::unique_ptr<StmntAst> do_while_stmnt();
    std::unique_ptr<StmntAst> break_stmnt();
    std::unique_ptr<StmntAst> continue_stmnt();
    std::unique_ptr<StmntAst> switch_stmnt();
    std::unique_ptr<StmntAst> ret();
    std::unique_ptr<StmntAst> stmnt();
    std::unique_ptr<BlockAst> block();
    std::unique_ptr<BaseAst> func();

public:
    Parser(NamePool *namePool, SymbolTable *symbolTable, Lexer *lexer);

    std::unique_ptr<BaseAst> parseNode();

    bool isOver() const { return peek().type == Token::T_END; }
    bool isPanic() const { return panic; }
};

// panics if pointer x is null; returns panic
template<typename T>
bool Parser::broken(const T &x) {
    if (x == nullptr) panic = true;
    return panic;
}