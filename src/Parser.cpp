#include "Parser.h"
#include "AST.h"
#include <iostream>
#include <cstdint>
using namespace std;

Parser::Parser(NamePool *namePool, SymbolTable *symbolTable, Lexer *lexer) 
    : namePool(namePool), symbolTable(symbolTable), lex(lexer), panic(false) {
}

Token Parser::peek() const {
    if (!tokRewind.empty()) return tokRewind.front();
    return lex->peek();
}

Token Parser::next() {
    if (!tokRewind.empty()) {
        Token tok = tokRewind.front();
        tokRewind.pop();
        return tok;
    }
    return lex->next();
}

// Eats the next token and returns whether its type matches the given type.
bool Parser::match(Token::Type type) {
    return next().type == type;
}

// Eats the next token and panics if it doesn't match the expected type.
bool Parser::mismatch(Token::Type type) {
    if (!match(type)) panic = true;
    return panic;
}

std::unique_ptr<ExprAST> Parser::call() {
    Token tok = next();
    if (tok.type != Token::T_ID) {
        panic = true;
        return nullptr;
    }

    unique_ptr<CallExprAST> call = make_unique<CallExprAST>(tok.nameId);

    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    bool first = true;
    while (true) {
        if (peek().type == Token::T_BRACE_R_REG) {
            next();
            break;
        }

        if (!first && mismatch(Token::T_COMMA)) return nullptr;

        unique_ptr<ExprAST> arg = expr();
        if (broken(arg)) return nullptr;

        call->addArg(move(arg));

        first = false;
    }

    return call;
}

unique_ptr<ExprAST> Parser::prim() {
    // remember, string literal is lvalue
    Token tok = next();
    if (tok.type == Token::T_NUM) {
        LiteralVal val;
        val.type = LiteralVal::T_SINT;
        val.val_si = tok.num;

        return make_unique<LiteralExprAST>(val);
    } else if (tok.type == Token::T_FNUM) {
        LiteralVal val;
        val.type = LiteralVal::T_FLOAT;
        val.val_f = tok.fnum;

        return make_unique<LiteralExprAST>(val);
    } else if (tok.type == Token::T_BVAL) {
        return make_unique<LiteralExprAST>(tok.bval);
    } else if (tok.type == Token::T_ID) {
        if (symbolTable->getTypeTable()->isType(tok.nameId)) {
            tokRewind.push(tok);
            unique_ptr<TypeAST> t = type();
            if (broken(t)) return nullptr;

            if (mismatch(Token::T_BRACE_L_REG)) return nullptr;
            unique_ptr<ExprAST> e = expr();
            if (broken(e)) return nullptr;
            if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

            return make_unique<CastExprAST>(move(t), move(e));
        } else if (peek().type == Token::T_BRACE_L_REG) {
            tokRewind.push(tok);
            return call();
        } else {
            return make_unique<VarExprAST>(tok.nameId);
        }
    } else if (tok.type == Token::T_OPER) {
        if (!operInfos.at(tok.op).unary) {
            panic = true;
            return nullptr;
        }

        unique_ptr<ExprAST> e = prim();
        if (broken(e)) return nullptr;

        return make_unique<UnExprAST>(move(e), tok.op);
    } else if (tok.type == Token::T_BRACE_L_REG) {
        unique_ptr<ExprAST> e = expr();
        if (broken(e)) return nullptr;
        if (mismatch(Token::T_BRACE_R_REG)) return nullptr;
        return e;
    } else {
        panic = true;
        return nullptr;
    }
}

unique_ptr<ExprAST> Parser::expr(unique_ptr<ExprAST> lhs, OperPrec min_prec) {
    Token lookOp = peek();
    while (lookOp.type == Token::T_OPER && operInfos.at(lookOp.op).prec >= min_prec) {
        Token op = next();

        if (!operInfos.at(op.op).binary) {
            panic = true;
            return nullptr;
        }
        
        unique_ptr<ExprAST> rhs = prim();
        if (broken(rhs)) return nullptr;

        lookOp = peek();
        while (lookOp.type == Token::T_OPER && 
            (operInfos.at(lookOp.op).prec > operInfos.at(op.op).prec ||
            (operInfos.at(lookOp.op).prec == operInfos.at(op.op).prec && !operInfos.at(lookOp.op).l_assoc))) {
            rhs = expr(move(rhs), operInfos.at(lookOp.op).prec);
            lookOp = peek();
        }

        lhs = make_unique<BinExprAST>(move(lhs), move(rhs), op.op);
    }
    return lhs;
}

unique_ptr<ExprAST> Parser::expr() {
    unique_ptr<ExprAST> e = prim();
    if (broken(e)) return nullptr;

    e = expr(move(e), minOperPrec);
    if (broken(e)) return nullptr;

    if (peek().type == Token::T_QUESTION) {
        next();

        unique_ptr<ExprAST> t = expr();
        if (broken(t)) return nullptr;

        if (mismatch(Token::T_COLON)) return nullptr;

        unique_ptr<ExprAST> f = expr();
        if (broken(f)) return nullptr;

        if (e->type() == AST_BinExpr && operInfos.at(((BinExprAST*)e.get())->getOp()).assignment) {
            // assignment has the same precedence as ternary cond, but they're right-to-left assoc
            // beware, move labyrinth ahead
            BinExprAST *binE = (BinExprAST*)e.get();
            binE->setR(make_unique<TernCondExprAST>(move(binE->resetR()), move(t), move(f)));
            return e;
        } else {
            return make_unique<TernCondExprAST>(move(e), move(t), move(f));
        }
    } else {
        return e;
    }
}

std::unique_ptr<TypeAST> Parser::type() {
    if (peek().type != Token::T_ID) {
        panic = true;
        return nullptr;
    }

    Token type = next();

    if (!symbolTable->getTypeTable()->isType(type.nameId)) {
        panic = true;
        return nullptr;
    }

    TypeTable::Id typeId = symbolTable->getTypeTable()->getTypeId(type.nameId);
    return make_unique<TypeAST>(typeId);
}

unique_ptr<DeclAST> Parser::decl() {
    unique_ptr<TypeAST> ty = type();
    if (broken(ty)) return nullptr;

    unique_ptr<DeclAST> ret = make_unique<DeclAST>(move(ty));

    while (true) {
        Token id = next();
        if (id.type != Token::T_ID) {
            panic = true;
            return nullptr;
        }

        unique_ptr<ExprAST> init;
        Token look = next();
        if (look.type == Token::T_OPER && look.op == Token::O_ASGN) {
            init = expr();
            if (broken(init)) return nullptr;
            look = next();
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

std::unique_ptr<StmntAST> Parser::simple() {
    if (peek().type == Token::T_SEMICOLON) {
        next();
        return make_unique<EmptyExprAST>();
    }

    if (peek().type == Token::T_ID && symbolTable->getTypeTable()->isType(peek().nameId)) {
        tokRewind.push(lex->next());
        if (lex->peek().type == Token::T_BRACE_L_REG) {
            // expression starting with a cast (or ctor)
            return expr();
        } else {
            return decl();
        }
    }

    return expr();
}

std::unique_ptr<StmntAST> Parser::if_stmnt() {
    if (mismatch(Token::T_IF)) return nullptr;
    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    unique_ptr<StmntAST> init;
    unique_ptr<ExprAST> cond;

    init = simple();
    if (broken(init)) return nullptr;

    if (init->type() == AST_Decl || init->type() == AST_EmptyExpr) {
        // semicolon was eaten, parse condition
        cond = expr();
        if (broken(cond)) return nullptr;
    } else {
        if (peek().type == Token::T_SEMICOLON) {
            // init was expression, eat semicolon and parse condition
            next();

            cond = expr();
            if (broken(cond)) return nullptr;
        } else {
            // there was no init
            cond = unique_ptr<ExprAST>((ExprAST*) init.release()); // modern C++ is modern
        }
    }

    if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

    unique_ptr<StmntAST> thenBody = stmnt();
    if (broken(thenBody)) return nullptr;

    unique_ptr<StmntAST> elseBody;
    if (peek().type == Token::T_ELSE) {
        next();

        elseBody = stmnt();
        if (broken(elseBody)) return nullptr;
    }

    return make_unique<IfAST>(move(init), move(cond), move(thenBody), move(elseBody));
}

std::unique_ptr<StmntAST> Parser::for_stmnt() {
    if (mismatch(Token::T_FOR)) return nullptr;
    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    unique_ptr<StmntAST> init = simple();
    if (init->type() != AST_Decl && init->type() != AST_EmptyExpr) {
        // init was expression, need to eat semicolon
        if (mismatch(Token::T_SEMICOLON)) return nullptr;
    }

    unique_ptr<ExprAST> cond;
    if (peek().type != Token::T_SEMICOLON) {
        cond = expr();
        if (broken(cond)) return nullptr;
    }
    
    if (mismatch(Token::T_SEMICOLON)) return nullptr;

    unique_ptr<ExprAST> iter;
    if (peek().type != Token::T_BRACE_R_REG) {
        iter = expr();
        if (broken(iter)) return nullptr;
    }
    
    if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

    unique_ptr<StmntAST> body = stmnt();
    if (broken(body)) return nullptr;

    return make_unique<ForAST>(move(init), move(cond), move(iter), move(body));
}

std::unique_ptr<StmntAST> Parser::while_stmnt() {
    if (mismatch(Token::T_WHILE)) return nullptr;
    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    unique_ptr<ExprAST> cond = expr();
    if (broken(cond)) return nullptr;

    if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

    unique_ptr<StmntAST> body = stmnt();
    if (broken(body)) return nullptr;

    return make_unique<WhileAST>(move(cond), move(body));
}

std::unique_ptr<StmntAST> Parser::do_while_stmnt() {
    if (mismatch(Token::T_DO)) return nullptr;

    unique_ptr<StmntAST> body = stmnt();
    if (broken(body)) return nullptr;

    if (mismatch(Token::T_WHILE)) return nullptr;
    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;
    unique_ptr<ExprAST> cond = expr();
    if (broken(cond)) return nullptr;
    if (mismatch(Token::T_BRACE_R_REG)) return nullptr;
    if (mismatch(Token::T_SEMICOLON)) return nullptr;

    return make_unique<DoWhileAST>(move(body), move(cond));
}

std::unique_ptr<StmntAST> Parser::break_stmnt() {
    if (mismatch(Token::T_BREAK) || mismatch(Token::T_SEMICOLON)) return nullptr;

    return make_unique<BreakAST>();
}

std::unique_ptr<StmntAST> Parser::continue_stmnt() {
    if (mismatch(Token::T_CONTINUE) || mismatch(Token::T_SEMICOLON)) return nullptr;

    return make_unique<ContinueAST>();
}

std::unique_ptr<StmntAST> Parser::switch_stmnt() {
    if (mismatch(Token::T_SWITCH) || mismatch(Token::T_BRACE_L_REG)) return nullptr;

    unique_ptr<ExprAST> value = expr();
    if (broken(value)) return nullptr;

    if (mismatch(Token::T_BRACE_R_REG) || mismatch(Token::T_BRACE_L_CUR)) return nullptr;

    vector<SwitchAST::Case> cases;

    bool parsedDefault = false;
    while (peek().type != Token::T_BRACE_R_CUR) {
        Token::Type case_ = next().type;

        vector<unique_ptr<ExprAST>> comparisons;

        if (case_ == Token::T_CASE) {
            while (true) {
                unique_ptr<ExprAST> comp = expr();
                if (broken(comp)) return nullptr;

                comparisons.push_back(move(comp));

                Token::Type ty = peek().type;
                if (ty == Token::T_COMMA) {
                    next();
                } else if (ty == Token::T_COLON) {
                    break;
                } else {
                    panic = true;
                    return nullptr;
                }
            }

            if (comparisons.empty()) {
                panic = true;
                return nullptr;
            }
        } else if (case_ == Token::T_DEFAULT) {
            if (parsedDefault) {
                panic = true;
                return nullptr;
            }
            parsedDefault = true;
        } else {
            panic = true;
            return nullptr;
        }

        if (mismatch(Token::T_COLON)) return nullptr;

        unique_ptr<BlockAST> body = make_unique<BlockAST>();
        while (peek().type != Token::T_CASE && peek().type != Token::T_DEFAULT && peek().type != Token::T_BRACE_R_CUR) {
            unique_ptr<StmntAST> st = stmnt();
            if (broken(st)) return nullptr;

            body->add(move(st));
        }

        // i like to move it, move it
        SwitchAST::Case caseSection(move(comparisons), move(body));
        cases.push_back(move(caseSection));
    }
    next();

    if (cases.empty()) {
        panic = true;
        return nullptr;
    }

    return make_unique<SwitchAST>(move(value), move(cases));
}

std::unique_ptr<StmntAST> Parser::ret() {
    if (mismatch(Token::T_RET)) return nullptr;

    unique_ptr<ExprAST> val;
    if (peek().type != Token::T_SEMICOLON) {
        val = expr();
        if (broken(val)) return nullptr;
    }

    if (mismatch(Token::T_SEMICOLON)) return nullptr;

    return make_unique<RetAST>(move(val));
}

unique_ptr<StmntAST> Parser::stmnt() {
    if (peek().type == Token::T_IF) return if_stmnt();

    if (peek().type == Token::T_FOR) return for_stmnt();

    if (peek().type == Token::T_WHILE) return while_stmnt();

    if (peek().type == Token::T_DO) return do_while_stmnt();

    if (peek().type == Token::T_BREAK) return break_stmnt();

    if (peek().type == Token::T_CONTINUE) return continue_stmnt();

    if (peek().type == Token::T_SWITCH) return switch_stmnt();

    if (peek().type == Token::T_RET) return ret();

    if (peek().type == Token::T_BRACE_L_CUR) return block();
    
    unique_ptr<StmntAST> st = simple();
    if (broken(st)) return nullptr;
    if (st->type() != AST_EmptyExpr && st->type() != AST_Decl) {
        if (mismatch(Token::T_SEMICOLON)) return nullptr;
    }
    
    return st;
}

unique_ptr<BlockAST> Parser::block() {
    if (mismatch(Token::T_BRACE_L_CUR)) return nullptr;

    unique_ptr<BlockAST> ret = make_unique<BlockAST>();

    while (peek().type != Token::T_BRACE_R_CUR) {
        unique_ptr<StmntAST> st = stmnt();
        if (broken(st)) return nullptr;

        ret->add(move(st));
    }
    next();

    return ret;
}

unique_ptr<BaseAST> Parser::func() {
    if (mismatch(Token::T_FNC)) return nullptr;

    Token name = next();
    if (name.type != Token::T_ID) {
        panic = true;
        return nullptr;
    }

    unique_ptr<FuncProtoAST> proto = make_unique<FuncProtoAST>(name.nameId);

    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    // args
    bool first = true;
    while (true) {
        if (peek().type == Token::T_BRACE_R_REG) {
            next();
            break;
        }

        if (!first && mismatch(Token::T_COMMA)) return nullptr;

        unique_ptr<TypeAST> argType = type();
        if (broken(argType)) return nullptr;

        Token arg = next();
        if (arg.type != Token::T_ID) {
            panic = true;
            return nullptr;
        }

        proto->addArg(make_pair(move(argType), arg.nameId));

        first = false;
    }

    // ret type
    Token look = peek();
    if (look.type != Token::T_BRACE_L_CUR && look.type != Token::T_SEMICOLON) {
        unique_ptr<TypeAST> retType = type();
        if (broken(retType)) return nullptr;
        proto->setRetType(move(retType));

        look = peek();
    }

    if (look.type == Token::T_BRACE_L_CUR) {
        //body
        unique_ptr<BlockAST> body = block();
        if (broken(body)) return nullptr;

        return make_unique<FuncAST>(move(proto), move(body));
    } else if (look.type == Token::T_SEMICOLON) {
        next();
        return proto;
    } else {
        panic = true;
        return nullptr;
    }
}

unique_ptr<BaseAST> Parser::parseNode() {
    unique_ptr<BaseAST> next;

    if (peek().type == Token::T_FNC) next = func();
    else next = decl();
        
    if (panic || !next) {
        panic = true;
        return nullptr;
    }

    return next;
}