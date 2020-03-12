#include "AST.h"
using namespace std;

UntypedExprAst::UntypedExprAst(bool bb) {
    val.type = UntypedVal::T_BOOL;
    val.val_b = bb;
}

IndExprAst::IndExprAst(unique_ptr<ExprAst> base, unique_ptr<ExprAst> ind) : base(move(base)), ind(move(ind)) {
}

UnExprAst::UnExprAst(unique_ptr<ExprAst> e, Token::Oper o) : expr(move(e)), op(o) {
}

BinExprAst::BinExprAst(
    unique_ptr<ExprAst> _lhs, 
    unique_ptr<ExprAst>  _rhs, 
    Token::Oper _op) : lhs(move(_lhs)), rhs(move(_rhs)), op(_op) {
}

void BinExprAst::setR(std::unique_ptr<ExprAst> _rhs) {
    rhs = move(_rhs);
}

TernCondExprAst::TernCondExprAst(
    unique_ptr<ExprAst> _cond,
    unique_ptr<ExprAst> _op1,
    unique_ptr<ExprAst> _op2) : cond(move(_cond)), op1(move(_op1)), op2(move(_op2)) {
}

CastExprAst::CastExprAst(unique_ptr<TypeAst> ty, unique_ptr<ExprAst> val) : t(move(ty)), v(move(val)) {
}

ArrayExprAst::ArrayExprAst(unique_ptr<TypeAst> arrTy, vector<unique_ptr<ExprAst>> vals)
    : arrTy(move(arrTy)), vals(move(vals)) {
}

IfAst::IfAst(unique_ptr<StmntAst> init, unique_ptr<ExprAst> cond, 
        unique_ptr<StmntAst> thenBody, unique_ptr<StmntAst> elseBody)
        : init(move(init)), cond(move(cond)), thenBody(move(thenBody)), elseBody(move(elseBody)) {
}

ForAst::ForAst(unique_ptr<StmntAst> init, unique_ptr<ExprAst> cond, unique_ptr<ExprAst> iter, std::unique_ptr<StmntAst> body)
    : init(move(init)), cond(move(cond)), iter(move(iter)), body(move(body)) {
}

WhileAst::WhileAst(unique_ptr<ExprAst> cond, unique_ptr<StmntAst> body)
    : cond(move(cond)), body(move(body)) {
}

DoWhileAst::DoWhileAst(unique_ptr<StmntAst> body, unique_ptr<ExprAst> cond)
    : body(move(body)), cond(move(cond)) {
}

SwitchAst::Case::Case(std::vector<std::unique_ptr<ExprAst>> comparisons, std::unique_ptr<BlockAst> body)
    : comparisons(move(comparisons)), body(move(body)) {
}

SwitchAst::SwitchAst(unique_ptr<ExprAst> value, vector<Case> cases)
    : value(move(value)), cases(move(cases)) {
}

pair<bool, size_t> SwitchAst::getDefault() const {
    for (size_t i = 0; i < cases.size(); ++i) {
        if (cases[i].isDefault()) return {true, i};
    }

    return {false, 0};
}

DeclAst::DeclAst(unique_ptr<TypeAst> type) : varType(move(type)) {
}

void DeclAst::add(pair<NamePool::Id, unique_ptr<ExprAst>> decl) {
    decls.push_back(move(decl));
}

FuncAst::FuncAst(unique_ptr<FuncProtoAst> proto, unique_ptr<BlockAst> body)
        : proto(move(proto)), body(move(body)) {
}
