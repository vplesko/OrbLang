#include "AST.h"
using namespace std;

UntypedExprAst::UntypedExprAst(CodeLoc loc, bool bb) : ExprAst(loc) {
    val.type = UntypedVal::T_BOOL;
    val.val_b = bb;
}

IndExprAst::IndExprAst(CodeLoc loc, unique_ptr<ExprAst> base, unique_ptr<ExprAst> ind)
    : ExprAst(loc), base(move(base)), ind(move(ind)) {
}

UnExprAst::UnExprAst(CodeLoc loc, unique_ptr<ExprAst> e, Token::Oper o)
    : ExprAst(loc), expr(move(e)), op(o) {
}

BinExprAst::BinExprAst(
    CodeLoc loc,
    unique_ptr<ExprAst> _lhs, 
    unique_ptr<ExprAst>  _rhs, 
    Token::Oper _op)
    : ExprAst(loc), lhs(move(_lhs)), rhs(move(_rhs)), op(_op) {
}

CastExprAst::CastExprAst(CodeLoc loc, unique_ptr<TypeAst> ty, unique_ptr<ExprAst> val)
    : ExprAst(loc), t(move(ty)), v(move(val)) {
}

ArrayExprAst::ArrayExprAst(CodeLoc loc, unique_ptr<TypeAst> arrTy, vector<unique_ptr<ExprAst>> vals)
    : ExprAst(loc), arrTy(move(arrTy)), vals(move(vals)) {
}

IfAst::IfAst(CodeLoc loc,
    unique_ptr<StmntAst> init, unique_ptr<ExprAst> cond, 
    unique_ptr<StmntAst> thenBody, unique_ptr<StmntAst> elseBody)
    : StmntAst(loc), init(move(init)), cond(move(cond)), thenBody(move(thenBody)), elseBody(move(elseBody)) {
}

ForAst::ForAst(CodeLoc loc,
    unique_ptr<StmntAst> init, unique_ptr<ExprAst> cond, unique_ptr<ExprAst> iter, std::unique_ptr<StmntAst> body)
    : StmntAst(loc), init(move(init)), cond(move(cond)), iter(move(iter)), body(move(body)) {
}

WhileAst::WhileAst(CodeLoc loc, unique_ptr<ExprAst> cond, unique_ptr<StmntAst> body)
    : StmntAst(loc), cond(move(cond)), body(move(body)) {
}

DoWhileAst::DoWhileAst(CodeLoc loc, unique_ptr<StmntAst> body, unique_ptr<ExprAst> cond)
    : StmntAst(loc), body(move(body)), cond(move(cond)) {
}

SwitchAst::Case::Case(std::vector<std::unique_ptr<ExprAst>> comparisons, std::unique_ptr<BlockAst> body)
    : comparisons(move(comparisons)), body(move(body)) {
}

SwitchAst::SwitchAst(CodeLoc loc, unique_ptr<ExprAst> value, vector<Case> cases)
    : StmntAst(loc), value(move(value)), cases(move(cases)) {
}

optional<size_t> SwitchAst::getDefault() const {
    for (size_t i = 0; i < cases.size(); ++i) {
        if (cases[i].isDefault()) return i;
    }

    return nullopt;
}

DeclAst::DeclAst(CodeLoc loc, unique_ptr<TypeAst> type) : StmntAst(loc), varType(move(type)) {
}

void DeclAst::add(pair<unique_ptr<VarExprAst>, unique_ptr<ExprAst>> decl) {
    decls.push_back(move(decl));
}

FuncAst::FuncAst(CodeLoc loc, unique_ptr<FuncProtoAst> proto, unique_ptr<BlockAst> body)
    : BaseAst(loc), proto(move(proto)), body(move(body)) {
}
