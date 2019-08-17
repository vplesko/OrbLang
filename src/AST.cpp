#include "AST.h"
using namespace std;

BinExprAST::BinExprAST(
    unique_ptr<ExprAST> _lhs, 
    unique_ptr<ExprAST>  _rhs, 
    Token::Oper _op) : lhs(move(_lhs)), rhs(move(_rhs)), op(_op) {
}

IfAST::IfAST(std::unique_ptr<ExprAST> cond, std::unique_ptr<StmntAST> body)
        : cond(std::move(cond)), body(std::move(body)) {
}

void DeclAST::add(std::pair<NamePool::Id, std::unique_ptr<ExprAST>> decl) {
    decls.push_back(move(decl));
}

FuncAST::FuncAST(unique_ptr<FuncProtoAST> proto, unique_ptr<BlockAST> body)
        : proto(move(proto)), body(move(body)) {
}
