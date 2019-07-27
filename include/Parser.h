#pragma once

#include <memory>
#include "Lexer.h"
#include "Codegen.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class Parser {
    NamePool *namePool;
    SymbolTable *symbolTable;
    Lexer *lex;
    std::unique_ptr<CodeGen> codegen;

    bool panic;

    bool mismatch(Token::Type t);
    template<typename T> bool broken(const T &x);

    std::unique_ptr<ExprAST> prim();
    std::unique_ptr<ExprAST> expr(std::unique_ptr<ExprAST> lhs, OperPrec min_prec);
    std::unique_ptr<ExprAST> expr();
    std::unique_ptr<DeclAST> decl();
    std::unique_ptr<StmntAST> stmnt();
    std::unique_ptr<BlockAST> block();
    std::unique_ptr<BaseAST> func();

public:
    Parser(NamePool *namePool, SymbolTable *symbolTable, Lexer *lexer);

    void parse(std::istream &istr);
};

template<typename T>
bool Parser::broken(const T &x) {
    if (x == nullptr) panic = true;
    return panic;
}