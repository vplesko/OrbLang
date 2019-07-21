#pragma once

#include <memory>
#include <vector>
#include "Lexer.h"
#include "SymbolTable.h"

enum ASTType {
    AST_LiteralExpr,
    AST_VarExpr,
    AST_BinExpr,
    AST_Decl
};

class BaseAST {
public:

    virtual ASTType type() const =0;

    virtual void print() const =0;

    virtual ~BaseAST() {}
};

class ExprAST : public BaseAST {
public:

    virtual ~ExprAST() {}
};

class LiteralExprAST : public ExprAST {
    int val;

public:
    LiteralExprAST(int v) : val(v) {}

    ASTType type() const { return AST_LiteralExpr; }

    int getVal() const { return val; }

    void print() const;
};

class VarExprAST : public ExprAST {
    NamePool::Id nameId;

public:
    VarExprAST(NamePool::Id id) : nameId(id) {}

    ASTType type() const { return AST_VarExpr; }

    NamePool::Id getNameId() const { return nameId; }

    void print() const;
};

class BinExprAST : public ExprAST {
    std::unique_ptr<ExprAST> lhs, rhs;
    Token::Oper op;

public:
    BinExprAST(
        std::unique_ptr<ExprAST> _lhs, 
        std::unique_ptr<ExprAST>  _rhs, 
        Token::Oper _op);

    ASTType type() const { return AST_BinExpr; }

    const ExprAST* getL() const { return lhs.get(); }
    const ExprAST* getR() const { return rhs.get(); }
    Token::Oper getOp() const { return op; }

    void print() const;
};

class DeclAST : public BaseAST {
    std::vector<std::pair<NamePool::Id, std::unique_ptr<ExprAST>>> decls;

public:
    DeclAST();

    void add(std::pair<NamePool::Id, std::unique_ptr<ExprAST>> decl);

    ASTType type() const { return AST_Decl; }

    void print() const;
};