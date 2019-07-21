#include "Parser.h"
#include <iostream>
using namespace std;

Parser::Parser(Lexer *lexer) : lex(lexer), llvmBuilder(llvmContext), panic(false) {
    symbolTable = std::make_unique<SymbolTable>();
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("test"), llvmContext);
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

        // TODO can't have multiple vars of same name
        if (!lex->match(Token::T_ASGN)) { panic = true; return nullptr; }

        unique_ptr<ExprAST> init = expr();
        if (panic) return nullptr;

        ret->add(make_pair(id.nameId, move(init)));

        Token look = lex->next();
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

void Parser::parse() {
    while (lex->peek().type != Token::T_END) {
        unique_ptr<BaseAST> st = stmnt();
        if (panic || !st) {
            cout << "ERROR!" << endl;
            return;
        }

        st->print();
        cout << endl;

        codegen(st.get())->print(llvm::outs());
        cout << endl;
        if (panic) return;
    }

    llvmModule->print(llvm::outs(), nullptr);
}