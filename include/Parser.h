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

    std::unique_ptr<ExprAst> prim();
    std::unique_ptr<ExprAst> expr();
    std::unique_ptr<TypeAst> type();
    std::unique_ptr<DeclAst> decl(std::unique_ptr<TypeAst> ty);
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
    std::unique_ptr<DataAst> data();
    std::unique_ptr<ImportAst> import();

public:
    Parser(NamePool *namePool, SymbolTable *symbolTable, CompileMessages *msgs);

    void setLexer(Lexer *lex_) { lex = lex_; }
    Lexer* getLexer() const { return lex; }

    std::unique_ptr<BaseAst> parseNode();

    bool isOver() const { return peek().type == Token::T_END; }
};