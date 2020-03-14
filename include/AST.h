#pragma once

#include <memory>
#include <vector>
#include "Lexer.h"
#include "SymbolTable.h"

enum AstType {
    AST_Type,
    AST_EmptyExpr,
    AST_UntypedExpr,
    AST_VarExpr,
    AST_IndExpr,
    AST_UnExpr,
    AST_BinExpr,
    AST_TernCondExpr,
    AST_CallExpr,
    AST_CastExpr,
    AST_ArrayExpr,
    AST_Decl,
    AST_If,
    AST_For,
    AST_While,
    AST_DoWhile,
    AST_Break,
    AST_Continue,
    AST_Switch,
    AST_FuncProto,
    AST_Func,
    AST_Block,
    AST_Ret
};

class BaseAst {
public:

    virtual AstType type() const =0;

    virtual ~BaseAst() {}
};

class TypeAst : public BaseAst {
    TypeTable::Id id;

public:
    explicit TypeAst(TypeTable::Id id) : id(id) {}

    TypeTable::Id getTypeId() const { return id; }

    std::unique_ptr<TypeAst> clone() const { return std::make_unique<TypeAst>(id); }

    AstType type() const override { return AST_Type; }
};

class StmntAst : public BaseAst {
public:

    virtual ~StmntAst() {}
};

class ExprAst : public StmntAst {
public:

    virtual ~ExprAst() {}
};

class UntypedExprAst : public ExprAst {
    UntypedVal val;

public:
    explicit UntypedExprAst(UntypedVal v) : val(v) {}
    explicit UntypedExprAst(bool bb);

    AstType type() const override { return AST_UntypedExpr; }

    const UntypedVal& getVal() const { return val; }
};

class VarExprAst : public ExprAst {
    NamePool::Id nameId;

public:
    explicit VarExprAst(NamePool::Id id) : nameId(id) {}

    AstType type() const override { return AST_VarExpr; }

    NamePool::Id getNameId() const { return nameId; }
};

class IndExprAst : public ExprAst {
    std::unique_ptr<ExprAst> base, ind;

public:
    IndExprAst(std::unique_ptr<ExprAst> base, std::unique_ptr<ExprAst> ind);

    AstType type() const override { return AST_IndExpr; }

    const ExprAst* getBase() const { return base.get(); }
    const ExprAst* getInd() const { return ind.get(); }
};

class UnExprAst : public ExprAst {
    std::unique_ptr<ExprAst> expr;
    Token::Oper op;

public:
    UnExprAst(std::unique_ptr<ExprAst> e, Token::Oper o);

    AstType type() const override { return AST_UnExpr; }

    const ExprAst* getExpr() const { return expr.get(); }
    Token::Oper getOp() const { return op; }
};

class BinExprAst : public ExprAst {
    std::unique_ptr<ExprAst> lhs, rhs;
    Token::Oper op;

public:
    BinExprAst(
        std::unique_ptr<ExprAst> _lhs, 
        std::unique_ptr<ExprAst>  _rhs, 
        Token::Oper _op);

    AstType type() const override { return AST_BinExpr; }

    const ExprAst* getL() const { return lhs.get(); }
    const ExprAst* getR() const { return rhs.get(); }
    Token::Oper getOp() const { return op; }

    // needed when parsing ternary conditional oper
    std::unique_ptr<ExprAst> resetR() { return move(rhs); }
    void setR(std::unique_ptr<ExprAst> _rhs);
};

class TernCondExprAst : public ExprAst {
    std::unique_ptr<ExprAst> cond, op1, op2;

public:
    TernCondExprAst(
        std::unique_ptr<ExprAst> _cond,
        std::unique_ptr<ExprAst> _op1,
        std::unique_ptr<ExprAst> _op2);

    AstType type() const override { return AST_TernCondExpr; }

    const ExprAst* getCond() const { return cond.get(); }
    const ExprAst* getOp1() const { return op1.get(); }
    const ExprAst* getOp2() const { return op2.get(); }
};

class CallExprAst : public ExprAst {
    NamePool::Id func;
    std::vector<std::unique_ptr<ExprAst>> args;

public:
    explicit CallExprAst(NamePool::Id funcName) : func(funcName) {}

    NamePool::Id getName() const { return func; }
    const std::vector<std::unique_ptr<ExprAst>>& getArgs() const { return args; }

    void addArg(std::unique_ptr<ExprAst> arg) { args.push_back(std::move(arg)); }

    AstType type() const override { return AST_CallExpr; }
};

class CastExprAst : public ExprAst {
    std::unique_ptr<TypeAst> t;
    std::unique_ptr<ExprAst> v;

public:
    CastExprAst(std::unique_ptr<TypeAst> ty, std::unique_ptr<ExprAst> val);

    const TypeAst* getType() const { return t.get(); }
    const ExprAst* getVal() const { return v.get(); }

    AstType type() const override { return AST_CastExpr; }
};

class ArrayExprAst : public ExprAst {
    std::unique_ptr<TypeAst> arrTy;
    std::vector<std::unique_ptr<ExprAst>> vals;

public:
    ArrayExprAst(std::unique_ptr<TypeAst> arrTy, std::vector<std::unique_ptr<ExprAst>> vals);

    const TypeAst* getArrayType() const { return arrTy.get(); }
    const std::vector<std::unique_ptr<ExprAst>>& getVals() const { return vals; }

    AstType type() const override { return AST_ArrayExpr; }
};

class EmptyStmntAst : public StmntAst {
public:

    AstType type() const override { return AST_EmptyExpr; }
};

class DeclAst : public StmntAst {
    std::unique_ptr<TypeAst> varType;
    std::vector<std::pair<NamePool::Id, std::unique_ptr<ExprAst>>> decls;

public:
    explicit DeclAst(std::unique_ptr<TypeAst> type);

    const TypeAst* getType() const { return varType.get(); }

    void add(std::pair<NamePool::Id, std::unique_ptr<ExprAst>> decl);
    const std::vector<std::pair<NamePool::Id, std::unique_ptr<ExprAst>>>& getDecls() const { return decls; }

    AstType type() const override { return AST_Decl; }
};

class IfAst : public StmntAst {
    std::unique_ptr<StmntAst> init;
    std::unique_ptr<ExprAst> cond;
    std::unique_ptr<StmntAst> thenBody, elseBody;

public:
    IfAst(std::unique_ptr<StmntAst> init, std::unique_ptr<ExprAst> cond, 
        std::unique_ptr<StmntAst> thenBody, std::unique_ptr<StmntAst> elseBody);
    
    const StmntAst* getInit() const { return init.get(); }
    const ExprAst* getCond() const { return cond.get(); }
    const StmntAst* getThen() const { return thenBody.get(); }
    const StmntAst* getElse() const { return elseBody.get(); }

    bool hasInit() const { return init != nullptr; }
    bool hasElse() const { return elseBody != nullptr; }

    AstType type() const override { return AST_If; }
};

class ForAst : public StmntAst {
    std::unique_ptr<StmntAst> init;
    std::unique_ptr<ExprAst> cond, iter;
    std::unique_ptr<StmntAst> body;

public:
    ForAst(std::unique_ptr<StmntAst> init, std::unique_ptr<ExprAst> cond,
        std::unique_ptr<ExprAst> iter, std::unique_ptr<StmntAst> body);

    const StmntAst* getInit() const { return init.get(); }
    const ExprAst* getCond() const { return cond.get(); }
    const ExprAst* getIter() const { return iter.get(); }
    const StmntAst* getBody() const { return body.get(); }

    bool hasCond() const { return cond != nullptr; }
    bool hasIter() const { return iter != nullptr; }

    AstType type() const override { return AST_For; }
};

class WhileAst : public StmntAst {
    std::unique_ptr<ExprAst> cond;
    std::unique_ptr<StmntAst> body;

public:
    WhileAst(std::unique_ptr<ExprAst> cond, std::unique_ptr<StmntAst> body);

    const ExprAst* getCond() const { return cond.get(); }
    const StmntAst* getBody() const { return body.get(); }

    AstType type() const override { return AST_While; }
};

class DoWhileAst : public StmntAst {
    std::unique_ptr<StmntAst> body;
    std::unique_ptr<ExprAst> cond;

public:
    DoWhileAst(std::unique_ptr<StmntAst> body, std::unique_ptr<ExprAst> cond);

    const StmntAst* getBody() const { return body.get(); }
    const ExprAst* getCond() const { return cond.get(); }

    AstType type() const override { return AST_DoWhile; }
};

class BreakAst : public StmntAst {
public:

    AstType type() const override { return AST_Break; }
};

class ContinueAst : public StmntAst {
public:

    AstType type() const override { return AST_Continue; }
};

class BlockAst : public StmntAst {
    std::vector<std::unique_ptr<StmntAst>> body;

public:

    void add(std::unique_ptr<StmntAst> st) { body.push_back(std::move(st)); }
    const std::vector<std::unique_ptr<StmntAst>>& getBody() const { return body; }

    AstType type() const override { return AST_Block; }
};

class SwitchAst : public StmntAst {
public:
    struct Case {
        std::vector<std::unique_ptr<ExprAst>> comparisons;
        std::unique_ptr<BlockAst> body;

        Case(std::vector<std::unique_ptr<ExprAst>> comparisons, std::unique_ptr<BlockAst> body);

        bool isDefault() const { return comparisons.empty(); }
    };

private:
    std::unique_ptr<ExprAst> value;
    std::vector<Case> cases;

public:
    SwitchAst(std::unique_ptr<ExprAst> value, std::vector<Case> cases);
    
    const ExprAst* getValue() const { return value.get(); }
    const std::vector<Case>& getCases() const { return cases; }
    std::pair<bool, std::size_t> getDefault() const;

    AstType type() const override { return AST_Switch; }
};

class FuncProtoAst : public BaseAst {
    NamePool::Id name;
    std::vector<std::pair<std::unique_ptr<TypeAst>, NamePool::Id>> args;
    std::unique_ptr<TypeAst> retType;
    bool variadic;
    bool noNameMangle;

public:
    explicit FuncProtoAst(NamePool::Id name) : name(name), variadic(false), noNameMangle(false) {}

    AstType type() const override { return AST_FuncProto; }

    NamePool::Id getName() const { return name; }

    void addArg(std::pair<std::unique_ptr<TypeAst>, NamePool::Id> arg) { args.push_back(std::move(arg)); }

    std::size_t getArgCnt() const { return args.size(); }
    const TypeAst* getArgType(std::size_t index) const { return args[index].first.get(); }
    NamePool::Id getArgName(std::size_t index) const { return args[index].second; }

    void setRetType(std::unique_ptr<TypeAst> t) { retType = std::move(t); }
    const TypeAst* getRetType() const { return retType.get(); }
    bool hasRetVal() const { return retType != nullptr; }

    bool isVariadic() const { return variadic; }
    void setVariadic(bool b) { variadic = b; }

    bool isNoNameMangle() const { return noNameMangle; }
    void setNoNameMangle(bool b) { noNameMangle = b; }
};

class FuncAst : public BaseAst {
    std::unique_ptr<FuncProtoAst> proto;
    std::unique_ptr<BlockAst> body;

public:
    FuncAst(
        std::unique_ptr<FuncProtoAst> proto,
        std::unique_ptr<BlockAst> body);

    AstType type() const override { return AST_Func; }

    const FuncProtoAst* getProto() const { return proto.get(); }
    const BlockAst* getBody() const { return body.get(); }
};

class RetAst : public StmntAst {
    std::unique_ptr<ExprAst> val;

public:
    explicit RetAst(std::unique_ptr<ExprAst> v) : val(std::move(v)) {}

    const ExprAst* getVal() const { return val.get(); }

    AstType type() const override { return AST_Ret; }
};