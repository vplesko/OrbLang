#pragma once

#include <memory>
#include "Lexer.h"
#include "AST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

class Parser {
    Lexer *lex;

    std::unique_ptr<SymbolTable> symbolTable;

    llvm::LLVMContext llvmContext;
    llvm::IRBuilder<> llvmBuilder;
    std::unique_ptr<llvm::Module> llvmModule;

    bool panic;

    std::unique_ptr<ExprAST> prim();
    std::unique_ptr<ExprAST> expr(std::unique_ptr<ExprAST> lhs, OperPrec min_prec);
    std::unique_ptr<ExprAST> expr();
    std::unique_ptr<DeclAST> decl();
    std::unique_ptr<BaseAST> stmnt();

    llvm::Function *main; // TODO tmp, remove
    void codegenStart(); // TODO tmp, remove
    llvm::Value* codegen(const BaseAST *ast);
    llvm::Value* codegen(const VarExprAST *ast);
    llvm::Value* codegen(const BinExprAST *ast);
    llvm::Value* codegen(const DeclAST *ast);
    void codegenEnd(); // TODO tmp, remove

public:
    Parser(Lexer *lexer);

    void parse();
};