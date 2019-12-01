#pragma once

#include <memory>
#include <vector>
#include "Lexer.h"
#include "SymbolTable.h"

enum ASTType {
    AST_Type,
    AST_NullExpr,
    AST_LiteralExpr,
    AST_VarExpr,
    AST_UnExpr,
    AST_BinExpr,
    AST_TernCondExpr,
    AST_CallExpr,
    AST_CastExpr,
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

    virtual ~BaseAST() {}
};

class TypeAST : public BaseAST {
    TypeTable::Id id;

public:
    explicit TypeAST(TypeTable::Id id) : id(id) {}

    TypeTable::Id getTypeId() const { return id; }

    ASTType type() const { return AST_Type; }
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
};

class LiteralExprAST : public ExprAST {
    const TypeTable::Id typeId;
    union {
        const int val;
        const bool b;
    };

public:
    LiteralExprAST(TypeTable::Id t, int v) : typeId(t), val(v) {}
    explicit LiteralExprAST(bool bb) : typeId(TypeTable::P_BOOL), b(bb) {}

    ASTType type() const { return AST_LiteralExpr; }

    TypeTable::Id getType() const { return typeId; }
    int getValI() const { return val; }
    unsigned getValU() const { return (unsigned) val; }
    bool getValB() const { return b; }
};

class VarExprAST : public ExprAST {
    NamePool::Id nameId;

public:
    explicit VarExprAST(NamePool::Id id) : nameId(id) {}

    ASTType type() const { return AST_VarExpr; }

    NamePool::Id getNameId() const { return nameId; }
};

class UnExprAST : public ExprAST {
    std::unique_ptr<ExprAST> expr;
    Token::Oper op;

public:
    UnExprAST(std::unique_ptr<ExprAST> e, Token::Oper o);

    ASTType type() const { return AST_UnExpr; }

    const ExprAST* getExpr() const { return expr.get(); }
    Token::Oper getOp() const { return op; }
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
};

class TernCondExprAST : public ExprAST {
    std::unique_ptr<ExprAST> cond, op1, op2;

public:
    TernCondExprAST(
        std::unique_ptr<ExprAST> _cond,
        std::unique_ptr<ExprAST> _op1,
        std::unique_ptr<ExprAST> _op2);

    ASTType type() const { return AST_TernCondExpr; }

    const ExprAST* getCond() const { return cond.get(); }
    const ExprAST* getOp1() const { return op1.get(); }
    const ExprAST* getOp2() const { return op2.get(); }
};

class CallExprAST : public ExprAST {
    NamePool::Id func;
    std::vector<std::unique_ptr<ExprAST>> args;

public:
    explicit CallExprAST(NamePool::Id funcName) : func(funcName) {}

    NamePool::Id getName() const { return func; }
    const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return args; }

    void addArg(std::unique_ptr<ExprAST> arg) { args.push_back(std::move(arg)); }

    ASTType type() const { return AST_CallExpr; }
};

class CastExprAST : public ExprAST {
    std::unique_ptr<TypeAST> t;
    std::unique_ptr<ExprAST> v;

public:
    CastExprAST(std::unique_ptr<TypeAST> ty, std::unique_ptr<ExprAST> val);

    const TypeAST* getType() const { return t.get(); }
    const ExprAST* getVal() const { return v.get(); }

    ASTType type() const { return AST_CastExpr; }
};

class DeclAST : public StmntAST {
    std::unique_ptr<TypeAST> varType;
    std::vector<std::pair<NamePool::Id, std::unique_ptr<ExprAST>>> decls;

public:
    explicit DeclAST(std::unique_ptr<TypeAST> type);

    const TypeAST* getType() const { return varType.get(); }

    void add(std::pair<NamePool::Id, std::unique_ptr<ExprAST>> decl);
    const std::vector<std::pair<NamePool::Id, std::unique_ptr<ExprAST>>>& getDecls() const { return decls; }

    ASTType type() const { return AST_Decl; }
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
};

class FuncProtoAST : public BaseAST {
    NamePool::Id name;
    std::vector<std::pair<std::unique_ptr<TypeAST>, NamePool::Id>> args;
    std::unique_ptr<TypeAST> retType;

public:
    explicit FuncProtoAST(NamePool::Id name) : name(name) {}

    ASTType type() const { return AST_FuncProto; }

    NamePool::Id getName() const { return name; }

    void addArg(std::pair<std::unique_ptr<TypeAST>, NamePool::Id> arg) { args.push_back(std::move(arg)); }

    std::size_t getArgCnt() const { return args.size(); }
    const TypeAST* getArgType(std::size_t index) const { return args[index].first.get(); }
    NamePool::Id getArgName(std::size_t index) const { return args[index].second; }

    void setRetType(std::unique_ptr<TypeAST> t) { retType = std::move(t); }
    const TypeAST* getRetType() const { return retType.get(); }
    bool hasRetVal() const { return retType != nullptr; }
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
};

class RetAST : public StmntAST {
    std::unique_ptr<ExprAST> val;

public:
    explicit RetAST(std::unique_ptr<ExprAST> v) : val(std::move(v)) {}

    const ExprAST* getVal() const { return val.get(); }

    ASTType type() const { return AST_Ret; }
};