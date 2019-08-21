#pragma once

#include <memory>
#include <vector>
#include "Lexer.h"
#include "SymbolTable.h"

enum ASTType {
    AST_NullExpr,
    AST_LiteralExpr,
    AST_VarExpr,
    AST_BinExpr,
    AST_CallExpr,
    AST_Decl,
    AST_If,
    AST_For,
    AST_While,
    AST_DoWhile,
    AST_FuncProto,
    AST_Func,
    AST_Block,
    AST_Ret
};

class BaseAST {
public:

    virtual ASTType type() const =0;

    virtual void print() const;

    virtual ~BaseAST() {}
};

class StmntAST : public BaseAST {
public:

    virtual ~StmntAST() {}
};

class ExprAST : public StmntAST {
public:

    virtual ~ExprAST() {}
};

class NullExprAST : public ExprAST {
public:

    ASTType type() const { return AST_NullExpr; }
    void print() const;
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

class CallExprAST : public ExprAST {
    NamePool::Id func;
    std::vector<std::unique_ptr<ExprAST>> args;

public:
    CallExprAST(NamePool::Id funcName) : func(funcName) {}

    NamePool::Id getName() const { return func; }
    const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return args; }

    void addArg(std::unique_ptr<ExprAST> arg) { args.push_back(std::move(arg)); }

    ASTType type() const { return AST_CallExpr; }

    void print() const;
};

class DeclAST : public StmntAST {
    std::vector<std::pair<NamePool::Id, std::unique_ptr<ExprAST>>> decls;

public:

    void add(std::pair<NamePool::Id, std::unique_ptr<ExprAST>> decl);
    const std::vector<std::pair<NamePool::Id, std::unique_ptr<ExprAST>>>& getDecls() const { return decls; }

    ASTType type() const { return AST_Decl; }

    void print() const;
};

class IfAST : public StmntAST {
    std::unique_ptr<StmntAST> init;
    std::unique_ptr<ExprAST> cond;
    std::unique_ptr<StmntAST> thenBody, elseBody;

public:
    IfAST(std::unique_ptr<StmntAST> init, std::unique_ptr<ExprAST> cond, 
        std::unique_ptr<StmntAST> thenBody, std::unique_ptr<StmntAST> elseBody);
    
    const StmntAST* getInit() const { return init.get(); }
    const ExprAST* getCond() const { return cond.get(); }
    const StmntAST* getThen() const { return thenBody.get(); }
    const StmntAST* getElse() const { return elseBody.get(); }

    bool hasInit() const { return init != nullptr; }
    bool hasElse() const { return elseBody != nullptr; }

    ASTType type() const { return AST_If; }
};

class ForAST : public StmntAST {
    std::unique_ptr<StmntAST> init;
    std::unique_ptr<ExprAST> cond, iter;
    std::unique_ptr<StmntAST> body;

public:
    ForAST(std::unique_ptr<StmntAST> init, std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> iter, std::unique_ptr<StmntAST> body);

    const StmntAST* getInit() const { return init.get(); }
    const ExprAST* getCond() const { return cond.get(); }
    const ExprAST* getIter() const { return iter.get(); }
    const StmntAST* getBody() const { return body.get(); }

    bool hasCond() const { return cond != nullptr; }
    bool hasIter() const { return iter != nullptr; }

    ASTType type() const { return AST_For; }
};

class WhileAST : public StmntAST {
    std::unique_ptr<ExprAST> cond;
    std::unique_ptr<StmntAST> body;

public:
    WhileAST(std::unique_ptr<ExprAST> cond, std::unique_ptr<StmntAST> body);

    const ExprAST* getCond() const { return cond.get(); }
    const StmntAST* getBody() const { return body.get(); }

    ASTType type() const { return AST_While; }
};

class DoWhileAST : public StmntAST {
    std::unique_ptr<StmntAST> body;
    std::unique_ptr<ExprAST> cond;

public:
    DoWhileAST(std::unique_ptr<StmntAST> body, std::unique_ptr<ExprAST> cond);

    const StmntAST* getBody() const { return body.get(); }
    const ExprAST* getCond() const { return cond.get(); }

    ASTType type() const { return AST_DoWhile; }
};

class BlockAST : public StmntAST {
    std::vector<std::unique_ptr<StmntAST>> body;

public:

    void add(std::unique_ptr<StmntAST> st) { body.push_back(std::move(st)); }
    const std::vector<std::unique_ptr<StmntAST>>& getBody() const { return body; }

    ASTType type() const { return AST_Block; }

    void print() const;
};

class FuncProtoAST : public BaseAST {
    NamePool::Id name;
    // TODO args have types
    std::vector<NamePool::Id> args;
    bool ret;

public:
    FuncProtoAST(NamePool::Id name) : name(name) {}

    ASTType type() const { return AST_FuncProto; }

    NamePool::Id getName() const { return name; }

    void addArg(NamePool::Id arg) { args.push_back(arg); }
    const std::vector<NamePool::Id> getArgs() const { return args; }

    void setRetVal(bool r) { ret = r; }
    bool hasRetVal() const { return ret; }

    void print() const;
};

class FuncAST : public BaseAST {
    std::unique_ptr<FuncProtoAST> proto;
    std::unique_ptr<BlockAST> body;

public:
    FuncAST(
        std::unique_ptr<FuncProtoAST> proto,
        std::unique_ptr<BlockAST> body);

    ASTType type() const { return AST_Func; }

    const FuncProtoAST* getProto() const { return proto.get(); }
    const BlockAST* getBody() const { return body.get(); }

    void print() const;
};

class RetAST : public StmntAST {
    std::unique_ptr<ExprAST> val;

public:
    RetAST(std::unique_ptr<ExprAST> v) : val(std::move(v)) {}

    const ExprAST* getVal() const { return val.get(); }

    ASTType type() const { return AST_Ret; }

    void print() const;
};