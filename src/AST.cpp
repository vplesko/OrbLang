#include "AST.h"
using namespace std;

void LiteralExprAST::print() const {
    cout << "Literal(" << val << ")";
}

BinExprAST::BinExprAST(
    unique_ptr<ExprAST> _lhs, 
    unique_ptr<ExprAST>  _rhs, 
    Token::Oper _op) : lhs(move(_lhs)), rhs(move(_rhs)), op(_op) {
}

void BinExprAST::print() const {
    cout << "Bin(";
    lhs->print();
    cout << " ";
    switch (op) {
    case Token::O_ASGN: cout << "="; break;
    case Token::O_ADD: cout << "+"; break;
    case Token::O_SUB: cout << "-"; break;
    case Token::O_MUL: cout << "*"; break;
    case Token::O_DIV: cout << "/"; break;
    default: break;
    }
    cout << " ";
    rhs->print();
    cout << ")";
}

void VarExprAST::print() const {
    cout << "Var([" << nameId << "])";
}

DeclAST::DeclAST() {
}

void DeclAST::add(std::pair<NamePool::Id, std::unique_ptr<ExprAST>> decl) {
    decls.push_back(move(decl));
}

void DeclAST::print() const {
    cout << "Decl(";
    for (size_t i = 0; i < decls.size(); ++i) {
        if (i > 0) cout << ", ";
        cout << "[" << decls[i].first << "]";
        if (decls[i].second) {
            cout << ":= ";
            decls[i].second->print();
        }
    }
    cout << ")";
}

void BlockAST::print() const {
    cout << "*** BLOCK START" << endl;
    for (const auto &it : body) {
        it->print();
        cout << endl;
    }
    cout << "*** BLOCK END";
}

void FuncProtoAST::print() const {
    cout << "FuncProto([" << name << "] := [";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cout << ", ";
        cout << args[i];
    }
    cout << "])";
}