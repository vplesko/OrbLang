#include "Parser.h"
#include <iostream>
using namespace std;

Parser::Parser(NamePool *namePool, SymbolTable *symbolTable, Lexer *lexer) : namePool(namePool), symbolTable(symbolTable), lex(lexer), panic(false) {
    codegen = std::make_unique<CodeGen>(namePool, symbolTable);
}

unique_ptr<ExprAST> Parser::prim() {
    Token tok = lex->next();
    if (tok.type == Token::T_NUM) {
        return make_unique<LiteralExprAST>(tok.num);
    } else if (tok.type == Token::T_ID) {
        return make_unique<VarExprAST>(tok.nameId);
    } else {
        panic = true;
        return nullptr;
    }
}

unique_ptr<ExprAST> Parser::expr() {
    unique_ptr<ExprAST> first = prim();
    if (panic) return nullptr;

    return expr(move(first), minOperPrec);
}

unique_ptr<ExprAST> Parser::expr(unique_ptr<ExprAST> lhs, OperPrec min_prec) {
    Token lookOp = lex->peek();
    while (lookOp.type == Token::T_OPER && operInfos.at(lookOp.op).prec >= min_prec) {
        Token op = lex->next();
        
        unique_ptr<ExprAST> rhs = prim();
        if (panic) return 0;

        lookOp = lex->peek();
        while (lookOp.type == Token::T_OPER && 
            (operInfos.at(lookOp.op).prec > operInfos.at(op.op).prec ||
            (operInfos.at(lookOp.op).prec == operInfos.at(op.op).prec && !operInfos.at(lookOp.op).l_assoc))) {
            rhs = expr(move(rhs), operInfos.at(lookOp.op).prec);
            lookOp = lex->peek();
        }

        lhs = make_unique<BinExprAST>(move(lhs), move(rhs), op.op);
    }
    return lhs;
}

unique_ptr<DeclAST> Parser::decl() {
    if (!lex->match(Token::T_VAR)) { panic = true; return nullptr; }

    unique_ptr<DeclAST> ret = make_unique<DeclAST>();

    while (true) {
        Token id = lex->next();
        if (id.type != Token::T_ID) { panic = true; return nullptr; }

        unique_ptr<ExprAST> init;
        Token look = lex->next();
        if (look.type == Token::T_OPER && look.op == Token::O_ASGN) {
            init = expr();
            if (panic || !init) return nullptr;
            look = lex->next();
        }
        ret->add(make_pair(id.nameId, move(init)));

        if (look.type == Token::T_SEMICOLON) {
            return ret;
        } else if (look.type != Token::T_COMMA) {
            panic = true;
            return nullptr;
        }
    }
}

unique_ptr<BaseAST> Parser::stmnt() {
    if (lex->peek().type == Token::T_VAR) return decl();
    
    unique_ptr<BaseAST> st = expr();
    if (panic) return nullptr;
    if (!lex->match(Token::T_SEMICOLON)) {
        panic = true;
        return nullptr;
    }
    return st;
}

void Parser::parse(std::istream &istr) {
    lex->start(istr);

    codegen->codegenStart();
    if (codegen->isPanic()) { panic = true; return; }

    while (lex->peek().type != Token::T_END) {
        unique_ptr<BaseAST> st = stmnt();
        if (panic || !st) {
            cout << "ERROR!" << endl;
            return;
        }

        st->print();
        cout << endl;

        codegen->codegen(st.get());
        if (codegen->isPanic()) { panic = true; }
        if (panic) return;
    }
    
    codegen->codegenEnd();
    if (codegen->isPanic()) { panic = true; return; }

    cout << endl;
    codegen->printout();
}