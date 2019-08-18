#include "AST.h"
using namespace std;

BinExprAST::BinExprAST(
    unique_ptr<ExprAST> _lhs, 
    unique_ptr<ExprAST>  _rhs, 
    Token::Oper _op) : lhs(move(_lhs)), rhs(move(_rhs)), op(_op) {
}

IfAST::IfAST(unique_ptr<StmntAST> init, unique_ptr<ExprAST> cond, 
        unique_ptr<StmntAST> thenBody, unique_ptr<StmntAST> elseBody)
        : init(move(init)), cond(move(cond)), thenBody(move(thenBody)), elseBody(move(elseBody)) {
}

WhileAST::WhileAST(unique_ptr<ExprAST> cond, unique_ptr<StmntAST> body)
    : cond(move(cond)), body(move(body)) {
}

DoWhileAST::DoWhileAST(unique_ptr<StmntAST> body, unique_ptr<ExprAST> cond)
    : body(move(body)), cond(move(cond)) {
}

void DeclAST::add(pair<NamePool::Id, unique_ptr<ExprAST>> decl) {
    decls.push_back(move(decl));
}

FuncAST::FuncAST(unique_ptr<FuncProtoAST> proto, unique_ptr<BlockAST> body)
        : proto(move(proto)), body(move(body)) {
}
