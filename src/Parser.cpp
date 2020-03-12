#include "Parser.h"
#include "AST.h"
#include <iostream>
#include <sstream>
#include <cstdint>
using namespace std;

Parser::Parser(NamePool *namePool, SymbolTable *symbolTable, Lexer *lexer) 
    : namePool(namePool), symbolTable(symbolTable), lex(lexer), panic(false) {
}

const Token& Parser::peek() const {
    return lex->peek();
}

Token Parser::next() {
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

unique_ptr<ArrayExprAst> Parser::array_list(unique_ptr<TypeAst> arrTy) {
    vector<unique_ptr<ExprAst>> vals;

    if (mismatch(Token::T_BRACE_L_CUR)) return nullptr;

    if (peek().type == Token::T_BRACE_R_CUR) {
        // REM empty arrays not allowed
        panic = true;
        return nullptr;
    }

    while (true) {
        unique_ptr<ExprAst> ex = expr();
        if (broken(ex)) return nullptr;

        vals.push_back(move(ex));

        Token tok = next();
        if (tok.type != Token::T_COMMA) {
            if (tok.type == Token::T_BRACE_R_CUR) {
                break;
            } else {
                panic = true;
                return nullptr;
            }
        }
    }

    return make_unique<ArrayExprAst>(move(arrTy), move(vals));
}

unique_ptr<ExprAst> Parser::prim() {
    unique_ptr<ExprAst> ret;

    if (peek().type == Token::T_NUM) {
        Token tok = next();

        UntypedVal val;
        val.type = UntypedVal::T_SINT;
        val.val_si = tok.num;

        ret = make_unique<UntypedExprAst>(val);
    } else if (peek().type == Token::T_FNUM) {
        Token tok = next();

        UntypedVal val;
        val.type = UntypedVal::T_FLOAT;
        val.val_f = tok.fnum;

        ret = make_unique<UntypedExprAst>(val);
    } else if (peek().type == Token::T_CHAR) {
        Token tok = next();

        UntypedVal val;
        val.type = UntypedVal::T_CHAR;
        val.val_c = tok.ch;

        ret = make_unique<UntypedExprAst>(val);
    } else if (peek().type == Token::T_BVAL) {
        Token tok = next();

        ret = make_unique<UntypedExprAst>(tok.bval);
    } else if (peek().type == Token::T_STRING) {
        stringstream ss;
        while (peek().type == Token::T_STRING) {
            ss << next().str;
        }

        UntypedVal val;
        val.type = UntypedVal::T_STRING;
        val.val_str = ss.str();

        ret = make_unique<UntypedExprAst>(move(val));
    } else if (peek().type == Token::T_NULL) {
        next();

        UntypedVal val;
        val.type = UntypedVal::T_NULL;

        ret = make_unique<UntypedExprAst>(val);
    } else if (peek().type == Token::T_ID) {
        if (symbolTable->getTypeTable()->isType(peek().nameId)) {
            unique_ptr<TypeAst> t = type();
            if (broken(t)) return nullptr;

            if (peek().type == Token::T_BRACE_L_REG) {
                next();

                unique_ptr<ExprAst> e = expr();
                if (broken(e)) return nullptr;
                if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

                ret = make_unique<CastExprAst>(move(t), move(e));
            } else if (peek().type == Token::T_BRACE_L_CUR) {
                unique_ptr<ArrayExprAst> e = array_list(move(t));
                if (broken(e)) return nullptr;

                ret = move(e);
            } else {
                panic = true;
                return nullptr;
            }
        } else {
            Token tok = next();

            if (peek().type == Token::T_BRACE_L_REG) {
                next();

                unique_ptr<CallExprAst> call = make_unique<CallExprAst>(tok.nameId);

                bool first = true;
                while (true) {
                    if (peek().type == Token::T_BRACE_R_REG) {
                        next();
                        break;
                    }

                    if (!first && mismatch(Token::T_COMMA)) return nullptr;

                    unique_ptr<ExprAst> arg = expr();
                    if (broken(arg)) return nullptr;

                    call->addArg(move(arg));

                    first = false;
                }

                ret = move(call);
            } else {
                ret = make_unique<VarExprAst>(tok.nameId);
            }
        }
    } else if (peek().type == Token::T_OPER) {
        Token tok = next();

        if (!operInfos.at(tok.op).unary) {
            panic = true;
            return nullptr;
        }

        unique_ptr<ExprAst> e = prim();
        if (broken(e)) return nullptr;

        ret = make_unique<UnExprAst>(move(e), tok.op);
    } else if (peek().type == Token::T_BRACE_L_REG) {
        next();

        unique_ptr<ExprAst> e = expr();
        if (broken(e)) return nullptr;

        if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

        ret = move(e);
    } else {
        panic = true;
        return nullptr;
    }

    while (peek().type == Token::T_BRACE_L_SQR) {
        next();
        unique_ptr<ExprAst> ind = expr();
        if (broken(ind)) return nullptr;
        if (mismatch(Token::T_BRACE_R_SQR)) return nullptr;
        ret = make_unique<IndExprAst>(move(ret), move(ind));
    }

    return ret;
}

unique_ptr<ExprAst> Parser::expr(unique_ptr<ExprAst> lhs, OperPrec min_prec) {
    Token lookOp = peek();
    while (lookOp.type == Token::T_OPER && operInfos.at(lookOp.op).prec >= min_prec) {
        Token op = next();

        if (!operInfos.at(op.op).binary) {
            panic = true;
            return nullptr;
        }
        
        unique_ptr<ExprAst> rhs = prim();
        if (broken(rhs)) return nullptr;

        lookOp = peek();
        while (lookOp.type == Token::T_OPER && 
            (operInfos.at(lookOp.op).prec > operInfos.at(op.op).prec ||
            (operInfos.at(lookOp.op).prec == operInfos.at(op.op).prec && !operInfos.at(lookOp.op).l_assoc))) {
            rhs = expr(move(rhs), operInfos.at(lookOp.op).prec);
            lookOp = peek();
        }

        lhs = make_unique<BinExprAst>(move(lhs), move(rhs), op.op);
    }
    return lhs;
}

unique_ptr<ExprAst> Parser::expr() {
    unique_ptr<ExprAst> e = prim();
    if (broken(e)) return nullptr;

    e = expr(move(e), minOperPrec);
    if (broken(e)) return nullptr;

    if (peek().type == Token::T_QUESTION) {
        next();

        unique_ptr<ExprAst> t = expr();
        if (broken(t)) return nullptr;

        if (mismatch(Token::T_COLON)) return nullptr;

        unique_ptr<ExprAst> f = expr();
        if (broken(f)) return nullptr;

        if (e->type() == AST_BinExpr && operInfos.at(((BinExprAst*)e.get())->getOp()).assignment) {
            // assignment has the same precedence as ternary cond, but they're right-to-left assoc
            // beware, move labyrinth ahead
            BinExprAst *binE = (BinExprAst*)e.get();
            binE->setR(make_unique<TernCondExprAst>(move(binE->resetR()), move(t), move(f)));
            return e;
        } else {
            return make_unique<TernCondExprAst>(move(e), move(t), move(f));
        }
    } else {
        return e;
    }
}

std::unique_ptr<TypeAst> Parser::type() {
    if (peek().type != Token::T_ID) {
        panic = true;
        return nullptr;
    }

    Token typeTok = next();
    if (!symbolTable->getTypeTable()->isType(typeTok.nameId)) {
        panic = true;
        return nullptr;
    }

    TypeTable::TypeDescr typeDescr(symbolTable->getTypeTable()->getTypeId(typeTok.nameId));

    while (true) {
        if (peek().type == Token::T_OPER && peek().op == Token::O_MUL) {
            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_PTR});
            next();
        } else if (peek().type == Token::T_BRACE_L_SQR) {
            next();
            if (peek().type == Token::T_BRACE_R_SQR) {
                next();
                typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_ARR_PTR});
            } else {
                Token ind = next();
                if (ind.type != Token::T_NUM || ind.num <= 0) return nullptr;
                if (mismatch(Token::T_BRACE_R_SQR)) return nullptr;

                typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_ARR, (unsigned long) ind.num});
            }
        } else if (peek().type == Token::T_CN) {
            next();
            typeDescr.setLastCn();
        } else {
            break;
        }
    }

    TypeTable::Id typeId = symbolTable->getTypeTable()->addType(move(typeDescr));

    return make_unique<TypeAst>(typeId);
}

unique_ptr<DeclAst> Parser::decl() {
    unique_ptr<TypeAst> ty = type();
    if (broken(ty)) return nullptr;

    unique_ptr<DeclAst> ret = make_unique<DeclAst>(ty->clone());

    while (true) {
        Token id = next();
        if (id.type != Token::T_ID) {
            panic = true;
            return nullptr;
        }

        unique_ptr<ExprAst> init;
        Token look = peek();
        if ((look.type == Token::T_OPER && look.op == Token::O_ASGN) ||
            look.type == Token::T_BRACE_L_REG) {
            look = next();

            init = expr();
            if (broken(init)) return nullptr;

            if (look.type == Token::T_BRACE_L_REG && mismatch(Token::T_BRACE_R_REG)) return nullptr;
        } else if (look.type == Token::T_BRACE_L_CUR) {
            init = array_list(ty->clone());
            if (broken(init)) return nullptr;
        }
        ret->add(make_pair(id.nameId, move(init)));

        look = next();

        if (look.type == Token::T_SEMICOLON) {
            return ret;
        } else if (look.type != Token::T_COMMA) {
            panic = true;
            return nullptr;
        }
    }
}

std::unique_ptr<StmntAst> Parser::simple() {
    if (peek().type == Token::T_SEMICOLON) {
        next();
        return make_unique<EmptyStmntAst>();
    } else if (peek().type == Token::T_ID && symbolTable->getTypeTable()->isType(peek().nameId)) {
        // if a statement begins with a type, it has to be a declaration
        // to begin an expression with a cast, put it inside brackets
        return decl();
    } else {
        return expr();
    }
}

std::unique_ptr<StmntAst> Parser::if_stmnt() {
    if (mismatch(Token::T_IF)) return nullptr;
    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    unique_ptr<StmntAst> init;
    unique_ptr<ExprAst> cond;

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
            cond = unique_ptr<ExprAst>((ExprAst*) init.release()); // modern C++ is modern
        }
    }

    if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

    unique_ptr<StmntAst> thenBody = stmnt();
    if (broken(thenBody)) return nullptr;

    unique_ptr<StmntAst> elseBody;
    if (peek().type == Token::T_ELSE) {
        next();

        elseBody = stmnt();
        if (broken(elseBody)) return nullptr;
    }

    return make_unique<IfAst>(move(init), move(cond), move(thenBody), move(elseBody));
}

std::unique_ptr<StmntAst> Parser::for_stmnt() {
    if (mismatch(Token::T_FOR)) return nullptr;
    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    unique_ptr<StmntAst> init = simple();
    if (init->type() != AST_Decl && init->type() != AST_EmptyExpr) {
        // init was expression, need to eat semicolon
        if (mismatch(Token::T_SEMICOLON)) return nullptr;
    }

    unique_ptr<ExprAst> cond;
    if (peek().type != Token::T_SEMICOLON) {
        cond = expr();
        if (broken(cond)) return nullptr;
    }
    
    if (mismatch(Token::T_SEMICOLON)) return nullptr;

    unique_ptr<ExprAst> iter;
    if (peek().type != Token::T_BRACE_R_REG) {
        iter = expr();
        if (broken(iter)) return nullptr;
    }
    
    if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

    unique_ptr<StmntAst> body = stmnt();
    if (broken(body)) return nullptr;

    return make_unique<ForAst>(move(init), move(cond), move(iter), move(body));
}

std::unique_ptr<StmntAst> Parser::while_stmnt() {
    if (mismatch(Token::T_WHILE)) return nullptr;
    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    unique_ptr<ExprAst> cond = expr();
    if (broken(cond)) return nullptr;

    if (mismatch(Token::T_BRACE_R_REG)) return nullptr;

    unique_ptr<StmntAst> body = stmnt();
    if (broken(body)) return nullptr;

    return make_unique<WhileAst>(move(cond), move(body));
}

std::unique_ptr<StmntAst> Parser::do_while_stmnt() {
    if (mismatch(Token::T_DO)) return nullptr;

    unique_ptr<StmntAst> body = stmnt();
    if (broken(body)) return nullptr;

    if (mismatch(Token::T_WHILE)) return nullptr;
    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;
    unique_ptr<ExprAst> cond = expr();
    if (broken(cond)) return nullptr;
    if (mismatch(Token::T_BRACE_R_REG)) return nullptr;
    if (mismatch(Token::T_SEMICOLON)) return nullptr;

    return make_unique<DoWhileAst>(move(body), move(cond));
}

std::unique_ptr<StmntAst> Parser::break_stmnt() {
    if (mismatch(Token::T_BREAK) || mismatch(Token::T_SEMICOLON)) return nullptr;

    return make_unique<BreakAst>();
}

std::unique_ptr<StmntAst> Parser::continue_stmnt() {
    if (mismatch(Token::T_CONTINUE) || mismatch(Token::T_SEMICOLON)) return nullptr;

    return make_unique<ContinueAst>();
}

std::unique_ptr<StmntAst> Parser::switch_stmnt() {
    if (mismatch(Token::T_SWITCH) || mismatch(Token::T_BRACE_L_REG)) return nullptr;

    unique_ptr<ExprAst> value = expr();
    if (broken(value)) return nullptr;

    if (mismatch(Token::T_BRACE_R_REG) || mismatch(Token::T_BRACE_L_CUR)) return nullptr;

    vector<SwitchAst::Case> cases;

    bool parsedDefault = false;
    while (peek().type != Token::T_BRACE_R_CUR) {
        Token::Type case_ = next().type;

        vector<unique_ptr<ExprAst>> comparisons;

        if (case_ == Token::T_CASE) {
            while (true) {
                unique_ptr<ExprAst> comp = expr();
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
        } else if (case_ == Token::T_ELSE) {
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

        unique_ptr<BlockAst> body = make_unique<BlockAst>();
        while (peek().type != Token::T_CASE && peek().type != Token::T_ELSE && peek().type != Token::T_BRACE_R_CUR) {
            unique_ptr<StmntAst> st = stmnt();
            if (broken(st)) return nullptr;

            body->add(move(st));
        }

        // i like to move it, move it
        SwitchAst::Case caseSection(move(comparisons), move(body));
        cases.push_back(move(caseSection));
    }
    next();

    if (cases.empty()) {
        panic = true;
        return nullptr;
    }

    return make_unique<SwitchAst>(move(value), move(cases));
}

std::unique_ptr<StmntAst> Parser::ret() {
    if (mismatch(Token::T_RET)) return nullptr;

    unique_ptr<ExprAst> val;
    if (peek().type != Token::T_SEMICOLON) {
        val = expr();
        if (broken(val)) return nullptr;
    }

    if (mismatch(Token::T_SEMICOLON)) return nullptr;

    return make_unique<RetAst>(move(val));
}

unique_ptr<StmntAst> Parser::stmnt() {
    if (peek().type == Token::T_IF) return if_stmnt();

    if (peek().type == Token::T_FOR) return for_stmnt();

    if (peek().type == Token::T_WHILE) return while_stmnt();

    if (peek().type == Token::T_DO) return do_while_stmnt();

    if (peek().type == Token::T_BREAK) return break_stmnt();

    if (peek().type == Token::T_CONTINUE) return continue_stmnt();

    if (peek().type == Token::T_SWITCH) return switch_stmnt();

    if (peek().type == Token::T_RET) return ret();

    if (peek().type == Token::T_BRACE_L_CUR) return block();
    
    unique_ptr<StmntAst> st = simple();
    if (broken(st)) return nullptr;
    if (st->type() != AST_EmptyExpr && st->type() != AST_Decl) {
        if (mismatch(Token::T_SEMICOLON)) return nullptr;
    }
    
    return st;
}

unique_ptr<BlockAst> Parser::block() {
    if (mismatch(Token::T_BRACE_L_CUR)) return nullptr;

    unique_ptr<BlockAst> ret = make_unique<BlockAst>();

    while (peek().type != Token::T_BRACE_R_CUR) {
        unique_ptr<StmntAst> st = stmnt();
        if (broken(st)) return nullptr;

        ret->add(move(st));
    }
    next();

    return ret;
}

unique_ptr<BaseAst> Parser::func() {
    if (mismatch(Token::T_FNC)) return nullptr;

    Token name = next();
    if (name.type != Token::T_ID) {
        panic = true;
        return nullptr;
    }

    unique_ptr<FuncProtoAst> proto = make_unique<FuncProtoAst>(name.nameId);

    if (mismatch(Token::T_BRACE_L_REG)) return nullptr;

    // args
    bool first = true;
    while (true) {
        if (peek().type == Token::T_BRACE_R_REG) {
            next();
            break;
        }

        if (!first && mismatch(Token::T_COMMA)) return nullptr;

        unique_ptr<TypeAst> argType = type();
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
        unique_ptr<TypeAst> retType = type();
        if (broken(retType)) return nullptr;
        proto->setRetType(move(retType));

        look = peek();
    }

    if (look.type == Token::T_BRACE_L_CUR) {
        //body
        unique_ptr<BlockAst> body = block();
        if (broken(body)) return nullptr;

        return make_unique<FuncAst>(move(proto), move(body));
    } else if (look.type == Token::T_SEMICOLON) {
        next();
        return proto;
    } else {
        panic = true;
        return nullptr;
    }
}

unique_ptr<BaseAst> Parser::parseNode() {
    unique_ptr<BaseAst> next;

    if (peek().type == Token::T_FNC) next = func();
    else next = decl();

    if (panic || !next) {
        panic = true;
        return nullptr;
    }

    return next;
}