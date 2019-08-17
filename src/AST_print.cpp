#include "AST.h"
using namespace std;

// TODO: get rid of print?

void BaseAST::print() const {
    cout << "(Unimplemented print)";
}

void NullExprAST::print() const {
    cout << "Epsilon";
}

void LiteralExprAST::print() const {
    cout << "Literal(" << val << ")";
}

void VarExprAST::print() const {
    cout << "Var([" << nameId << "])";
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
    case Token::O_EQ: cout << "=="; break;
    case Token::O_NEQ: cout << "!="; break;
    case Token::O_LT: cout << "<"; break;
    case Token::O_LTEQ: cout << "<="; break;
    case Token::O_GT: cout << ">"; break;
    case Token::O_GTEQ: cout << ">="; break;
    default: cout << "??"; break;
    }
    cout << " ";
    rhs->print();
    cout << ")";
}

void CallExprAST::print() const {
    cout << "Call([" << func << "]";
    if (!args.empty()) {
        cout << " : ";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) cout << ", ";
            args[i]->print();
        }
    }
    cout << ")";
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
    cout << "]";
    if (ret) cout << " -> var";
    cout << ")";
}

void FuncAST::print() const {
    cout << "***** FUNC START" << endl;
    proto->print();
    cout << endl;
    body->print();
    cout << endl << "***** FUNC END";
}

void RetAST::print() const {
    cout << "Ret(";
    if (val) val->print();
    cout << ")";
}
