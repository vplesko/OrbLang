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

    std::unique_ptr<ExprAST> prim();
    std::unique_ptr<ExprAST> expr(std::unique_ptr<ExprAST> lhs, OperPrec min_prec);
    std::unique_ptr<ExprAST> expr();
    std::unique_ptr<DeclAST> decl();
    std::unique_ptr<BaseAST> stmnt();

public:
    Parser(NamePool *namePool, SymbolTable *symbolTable, Lexer *lexer);

    void parse(std::istream &istr);
};