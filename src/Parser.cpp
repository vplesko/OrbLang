#include "Parser.h"
#include <iostream>
#include <sstream>
#include <cstdint>
#include "AST.h"
using namespace std;

Parser::Parser(NamePool *namePool, SymbolTable *symbolTable, CompileMessages *msgs) 
    : namePool(namePool), symbolTable(symbolTable), msgs(msgs), lex(nullptr) {
}

const Token& Parser::peek() const {
    return lex->peek();
}

Token Parser::next() {
    return lex->next();
}

// If the next token matches the type, eats it and returns true.
// Otherwise, returns false.
bool Parser::match(Token::Type type) {
    if (peek().type != type) return false;
    next();
    return true;
}

CodeLoc Parser::loc() const {
    return lex->loc();
}

unique_ptr<ArrayExprAst> Parser::array_list(unique_ptr<TypeAst> arrTy) {
    vector<unique_ptr<ExprAst>> vals;

    if (!match(Token::T_BRACE_L_CUR)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    if (peek().type == Token::T_BRACE_R_CUR) {
        // NOTE empty arrays not allowed
        msgs->errorUnknown(loc());
        return nullptr;
    }

    while (true) {
        unique_ptr<ExprAst> ex = expr();
        if (ex == nullptr) return nullptr;

        vals.push_back(move(ex));

        CodeLoc codeLocNext = loc();
        Token tok = next();
        if (tok.type != Token::T_COMMA) {
            if (tok.type == Token::T_BRACE_R_CUR) {
                break;
            } else {
                msgs->errorUnknown(codeLocNext);
                return nullptr;
            }
        }
    }

    return make_unique<ArrayExprAst>(arrTy->loc(), move(arrTy), move(vals));
}

unique_ptr<ExprAst> Parser::prim() {
    CodeLoc codeLoc = loc();
    unique_ptr<ExprAst> ret;

    if (peek().type == Token::T_NUM) {
        Token tok = next();

        UntypedVal val;
        val.type = UntypedVal::T_SINT;
        val.val_si = tok.num;

        ret = make_unique<UntypedExprAst>(codeLoc, val);
    } else if (peek().type == Token::T_FNUM) {
        Token tok = next();

        UntypedVal val;
        val.type = UntypedVal::T_FLOAT;
        val.val_f = tok.fnum;

        ret = make_unique<UntypedExprAst>(codeLoc, val);
    } else if (peek().type == Token::T_CHAR) {
        Token tok = next();

        UntypedVal val;
        val.type = UntypedVal::T_CHAR;
        val.val_c = tok.ch;

        ret = make_unique<UntypedExprAst>(codeLoc, val);
    } else if (peek().type == Token::T_BVAL) {
        Token tok = next();

        ret = make_unique<UntypedExprAst>(codeLoc, tok.bval);
    } else if (peek().type == Token::T_STRING) {
        stringstream ss;
        while (peek().type == Token::T_STRING) {
            ss << next().str;
        }

        UntypedVal val;
        val.type = UntypedVal::T_STRING;
        val.val_str = ss.str();

        ret = make_unique<UntypedExprAst>(codeLoc, move(val));
    } else if (peek().type == Token::T_NULL) {
        next();

        UntypedVal val;
        val.type = UntypedVal::T_NULL;

        ret = make_unique<UntypedExprAst>(codeLoc, val);
    } else if (peek().type == Token::T_ID) {
        if (symbolTable->getTypeTable()->isType(peek().nameId)) {
            unique_ptr<TypeAst> t = type();
            if (t == nullptr) return nullptr;

            if (peek().type == Token::T_BRACE_L_REG) {
                next();

                unique_ptr<ExprAst> e = expr();
                if (e == nullptr) return nullptr;
                if (!match(Token::T_BRACE_R_REG)) {
                    msgs->errorUnknown(loc());
                    return nullptr;
                }

                ret = make_unique<CastExprAst>(codeLoc, move(t), move(e));
            } else if (peek().type == Token::T_BRACE_L_CUR) {
                unique_ptr<ArrayExprAst> e = array_list(move(t));
                if (e == nullptr) return nullptr;

                ret = move(e);
            } else {
                msgs->errorUnknown(codeLoc);
                return nullptr;
            }
        } else {
            Token tok = next();

            if (peek().type == Token::T_BRACE_L_REG) {
                next();

                unique_ptr<CallExprAst> call = make_unique<CallExprAst>(codeLoc, tok.nameId);

                bool first = true;
                while (true) {
                    if (peek().type == Token::T_BRACE_R_REG) {
                        next();
                        break;
                    }

                    if (!first && !match(Token::T_COMMA)) {
                        msgs->errorUnknown(loc());
                        return nullptr;
                    }

                    unique_ptr<ExprAst> arg = expr();
                    if (arg == nullptr) return nullptr;

                    call->addArg(move(arg));

                    first = false;
                }

                ret = move(call);
            } else {
                ret = make_unique<VarExprAst>(codeLoc, tok.nameId);
            }
        }
    } else if (peek().type == Token::T_OPER) {
        Token tok = next();

        if (!operInfos.at(tok.op).unary) {
            msgs->errorUnknown(codeLoc);
            return nullptr;
        }

        unique_ptr<ExprAst> e = prim();
        if (e == nullptr) return nullptr;

        ret = make_unique<UnExprAst>(codeLoc, move(e), tok.op);
    } else if (peek().type == Token::T_BRACE_L_REG) {
        next();

        unique_ptr<ExprAst> e = expr();
        if (e == nullptr) return nullptr;

        if (!match(Token::T_BRACE_R_REG)) {
            msgs->errorUnknown(loc());
            return nullptr;
        }

        ret = move(e);
    } else {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    while (peek().type == Token::T_BRACE_L_SQR) {
        CodeLoc codeLocInd = loc();
        next();

        unique_ptr<ExprAst> ind = expr();
        if (ind == nullptr) return nullptr;
        if (!match(Token::T_BRACE_R_SQR)) {
            msgs->errorUnknown(loc());
            return nullptr;
        }

        ret = make_unique<IndExprAst>(codeLocInd, move(ret), move(ind));
    }

    return ret;
}

unique_ptr<ExprAst> Parser::expr(unique_ptr<ExprAst> lhs, OperPrec min_prec) {
    Token lookOp = peek();
    while (lookOp.type == Token::T_OPER && operInfos.at(lookOp.op).prec >= min_prec) {
        CodeLoc codeLocOp = loc();
        Token op = next();

        if (!operInfos.at(op.op).binary) {
            msgs->errorUnknown(codeLocOp);
            return nullptr;
        }
        
        unique_ptr<ExprAst> rhs = prim();
        if (rhs == nullptr) return nullptr;

        lookOp = peek();
        while (lookOp.type == Token::T_OPER && 
            (operInfos.at(lookOp.op).prec > operInfos.at(op.op).prec ||
            (operInfos.at(lookOp.op).prec == operInfos.at(op.op).prec && !operInfos.at(lookOp.op).l_assoc))) {
            rhs = expr(move(rhs), operInfos.at(lookOp.op).prec);
            lookOp = peek();
        }

        lhs = make_unique<BinExprAst>(codeLocOp, move(lhs), move(rhs), op.op);
    }
    return lhs;
}

unique_ptr<ExprAst> Parser::expr() {
    unique_ptr<ExprAst> e = prim();
    if (e == nullptr) return nullptr;

    e = expr(move(e), minOperPrec);
    if (e == nullptr) return nullptr;

    if (peek().type == Token::T_QUESTION) {
        CodeLoc codeLoc = loc();

        next();

        unique_ptr<ExprAst> t = expr();
        if (t == nullptr) return nullptr;

        if (!match(Token::T_COLON)) {
            msgs->errorUnknown(loc());
            return nullptr;
        }

        unique_ptr<ExprAst> f = expr();
        if (f == nullptr) return nullptr;

        if (e->type() == AST_BinExpr && operInfos.at(((BinExprAst*)e.get())->getOp()).assignment) {
            // assignment has the same precedence as ternary cond, but they're right-to-left assoc
            // beware, move labyrinth ahead
            BinExprAst *binE = (BinExprAst*)e.get();
            binE->setR(make_unique<TernCondExprAst>(codeLoc, move(binE->resetR()), move(t), move(f)));
            return e;
        } else {
            return make_unique<TernCondExprAst>(codeLoc, move(e), move(t), move(f));
        }
    } else {
        return e;
    }
}

std::unique_ptr<TypeAst> Parser::type() {
    CodeLoc codeLoc = loc();

    if (peek().type != Token::T_ID) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    Token typeTok = next();
    if (!symbolTable->getTypeTable()->isType(typeTok.nameId)) {
        msgs->errorUnknown(codeLoc);
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
                if (!match(Token::T_BRACE_R_SQR)) {
                    msgs->errorUnknown(loc());
                    return nullptr;
                }

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

    return make_unique<TypeAst>(codeLoc, typeId);
}

unique_ptr<DeclAst> Parser::decl() {
    CodeLoc codeLoc = loc();
    
    unique_ptr<TypeAst> ty = type();
    if (ty == nullptr) return nullptr;

    unique_ptr<DeclAst> ret = make_unique<DeclAst>(codeLoc, ty->clone());

    while (true) {
        CodeLoc codeLocId = loc();
        Token id = next();
        if (id.type != Token::T_ID) {
            msgs->errorUnknown(codeLocId);
            return nullptr;
        }

        unique_ptr<ExprAst> init;
        Token look = peek();
        if ((look.type == Token::T_OPER && look.op == Token::O_ASGN) ||
            look.type == Token::T_BRACE_L_REG) {
            look = next();

            init = expr();
            if (init == nullptr) return nullptr;

            if (look.type == Token::T_BRACE_L_REG && !match(Token::T_BRACE_R_REG)) {
                msgs->errorUnknown(loc());
                return nullptr;
            }
        } else if (look.type == Token::T_BRACE_L_CUR) {
            init = array_list(ty->clone());
            if (init == nullptr) return nullptr;
        }
        ret->add(make_pair(id.nameId, move(init)));

        CodeLoc codeLocNext = loc();
        look = next();
        
        if (look.type == Token::T_SEMICOLON) {
            return ret;
        } else if (look.type != Token::T_COMMA) {
            msgs->errorUnknown(codeLocNext);
            return nullptr;
        }
    }
}

std::unique_ptr<StmntAst> Parser::simple() {
    if (peek().type == Token::T_SEMICOLON) {
        CodeLoc codeLoc = loc();
        next();
        return make_unique<EmptyStmntAst>(codeLoc);
    } else if (peek().type == Token::T_ID && symbolTable->getTypeTable()->isType(peek().nameId)) {
        // if a statement begins with a type, it has to be a declaration
        // to begin an expression with a cast, put it inside brackets
        return decl();
    } else {
        return expr();
    }
}

std::unique_ptr<StmntAst> Parser::if_stmnt() {
    CodeLoc codeLoc = loc();
    
    if (!match(Token::T_IF)) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }
    if (!match(Token::T_BRACE_L_REG)) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    unique_ptr<StmntAst> init;
    unique_ptr<ExprAst> cond;

    init = simple();
    if (init == nullptr) return nullptr;

    if (init->type() == AST_Decl || init->type() == AST_EmptyExpr) {
        // semicolon was eaten, parse condition
        cond = expr();
        if (cond == nullptr) return nullptr;
    } else {
        if (peek().type == Token::T_SEMICOLON) {
            // init was expression, eat semicolon and parse condition
            next();

            cond = expr();
            if (cond == nullptr) return nullptr;
        } else {
            // there was no init
            cond = unique_ptr<ExprAst>((ExprAst*) init.release()); // modern C++ is modern
        }
    }

    if (!match(Token::T_BRACE_R_REG)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    unique_ptr<StmntAst> thenBody = stmnt();
    if (thenBody == nullptr) return nullptr;

    unique_ptr<StmntAst> elseBody;
    if (peek().type == Token::T_ELSE) {
        next();

        elseBody = stmnt();
        if (elseBody == nullptr) return nullptr;
    }

    return make_unique<IfAst>(codeLoc, move(init), move(cond), move(thenBody), move(elseBody));
}

std::unique_ptr<StmntAst> Parser::for_stmnt() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_FOR)) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }
    if (!match(Token::T_BRACE_L_REG)) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    unique_ptr<StmntAst> init = simple();
    if (init->type() != AST_Decl && init->type() != AST_EmptyExpr) {
        // init was expression, need to eat semicolon
        if (!match(Token::T_SEMICOLON)) {
            msgs->errorUnknown(loc());
            return nullptr;
        }
    }

    unique_ptr<ExprAst> cond;
    if (peek().type != Token::T_SEMICOLON) {
        cond = expr();
        if (cond == nullptr) return nullptr;
    }
    
    if (!match(Token::T_SEMICOLON)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    unique_ptr<ExprAst> iter;
    if (peek().type != Token::T_BRACE_R_REG) {
        iter = expr();
        if (iter == nullptr) return nullptr;
    }
    
    if (!match(Token::T_BRACE_R_REG)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    unique_ptr<StmntAst> body = stmnt();
    if (body == nullptr) return nullptr;

    return make_unique<ForAst>(codeLoc, move(init), move(cond), move(iter), move(body));
}

std::unique_ptr<StmntAst> Parser::while_stmnt() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_WHILE)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }
    if (!match(Token::T_BRACE_L_REG)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    unique_ptr<ExprAst> cond = expr();
    if (cond == nullptr) return nullptr;

    if (!match(Token::T_BRACE_R_REG)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    unique_ptr<StmntAst> body = stmnt();
    if (body == nullptr) return nullptr;

    return make_unique<WhileAst>(codeLoc, move(cond), move(body));
}

std::unique_ptr<StmntAst> Parser::do_while_stmnt() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_DO)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    unique_ptr<StmntAst> body = stmnt();
    if (body == nullptr) return nullptr;

    if (!match(Token::T_WHILE)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }
    if (!match(Token::T_BRACE_L_REG)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }
    unique_ptr<ExprAst> cond = expr();
    if (cond == nullptr) return nullptr;
    if (!match(Token::T_BRACE_R_REG)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }
    if (!match(Token::T_SEMICOLON)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    return make_unique<DoWhileAst>(codeLoc, move(body), move(cond));
}

std::unique_ptr<StmntAst> Parser::break_stmnt() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_BREAK) || !match(Token::T_SEMICOLON)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    return make_unique<BreakAst>(codeLoc);
}

std::unique_ptr<StmntAst> Parser::continue_stmnt() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_CONTINUE) || !match(Token::T_SEMICOLON)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    return make_unique<ContinueAst>(codeLoc);
}

std::unique_ptr<StmntAst> Parser::switch_stmnt() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_SWITCH) || !match(Token::T_BRACE_L_REG)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    unique_ptr<ExprAst> value = expr();
    if (value == nullptr) return nullptr;

    if (!match(Token::T_BRACE_R_REG) || !match(Token::T_BRACE_L_CUR)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    vector<SwitchAst::Case> cases;

    bool parsedDefault = false;
    while (peek().type != Token::T_BRACE_R_CUR) {
        CodeLoc codeLocCase = loc();
        Token::Type case_ = next().type;

        vector<unique_ptr<ExprAst>> comparisons;

        if (case_ == Token::T_CASE) {
            while (true) {
                unique_ptr<ExprAst> comp = expr();
                if (comp == nullptr) return nullptr;

                comparisons.push_back(move(comp));

                Token::Type ty = peek().type;
                if (ty == Token::T_COMMA) {
                    next();
                } else if (ty == Token::T_COLON) {
                    break;
                } else {
                    msgs->errorUnknown(loc());
                    return nullptr;
                }
            }

            if (comparisons.empty()) {
                msgs->errorUnknown(codeLocCase);
                return nullptr;
            }
        } else if (case_ == Token::T_ELSE) {
            if (parsedDefault) {
                msgs->errorUnknown(codeLocCase);
                return nullptr;
            }
            parsedDefault = true;
        } else {
            msgs->errorUnknown(codeLocCase);
            return nullptr;
        }

        if (!match(Token::T_COLON)) {
            msgs->errorUnknown(loc());
            return nullptr;
        }

        CodeLoc codeLocBlock = loc();
        unique_ptr<BlockAst> body = make_unique<BlockAst>(codeLocBlock);
        while (peek().type != Token::T_CASE && peek().type != Token::T_ELSE && peek().type != Token::T_BRACE_R_CUR) {
            unique_ptr<StmntAst> st = stmnt();
            if (st == nullptr) return nullptr;

            body->add(move(st));
        }

        // i like to move it, move it
        SwitchAst::Case caseSection(move(comparisons), move(body));
        cases.push_back(move(caseSection));
    }
    next();

    if (cases.empty()) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    return make_unique<SwitchAst>(codeLoc, move(value), move(cases));
}

std::unique_ptr<StmntAst> Parser::ret() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_RET)) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    unique_ptr<ExprAst> val;
    if (peek().type != Token::T_SEMICOLON) {
        val = expr();
        if (val == nullptr) return nullptr;
    }

    if (!match(Token::T_SEMICOLON)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    return make_unique<RetAst>(codeLoc, move(val));
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
    if (st == nullptr) return nullptr;
    if (st->type() != AST_EmptyExpr && st->type() != AST_Decl) {
        if (!match(Token::T_SEMICOLON)) {
            msgs->errorUnknown(loc());
            return nullptr;
        }
    }
    
    return st;
}

unique_ptr<BlockAst> Parser::block() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_BRACE_L_CUR)) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    unique_ptr<BlockAst> ret = make_unique<BlockAst>(codeLoc);

    while (peek().type != Token::T_BRACE_R_CUR) {
        unique_ptr<StmntAst> st = stmnt();
        if (st == nullptr) return nullptr;

        ret->add(move(st));
    }
    next();

    return ret;
}

unique_ptr<BaseAst> Parser::func() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_FNC)) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    CodeLoc codeLocName = loc();
    Token name = next();
    if (name.type != Token::T_ID) {
        msgs->errorUnknown(codeLocName);
        return nullptr;
    }

    unique_ptr<FuncProtoAst> proto = make_unique<FuncProtoAst>(codeLoc, name.nameId);
    proto->setNoNameMangle(name.nameId == namePool->getMain());

    if (!match(Token::T_BRACE_L_REG)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    // args
    bool first = true, last = false;
    while (true) {
        if (peek().type == Token::T_BRACE_R_REG) {
            next();
            break;
        }
        if (last && peek().type != Token::T_BRACE_R_REG) {
            msgs->errorUnknown(loc());
            return nullptr;
        }

        if (!first && !match(Token::T_COMMA)) {
            msgs->errorUnknown(loc());
            return nullptr;
        }

        if (peek().type == Token::T_ELLIPSIS) {
            next();
            proto->setVariadic(true);
            last = true;
        } else {
            unique_ptr<TypeAst> argType = type();
            if (argType == nullptr) return nullptr;

            CodeLoc codeLocArg = loc();
            Token arg = next();
            if (arg.type != Token::T_ID) {
                msgs->errorUnknown(codeLocArg);
                return nullptr;
            }

            proto->addArg(make_pair(move(argType), arg.nameId));
        }

        first = false;
    }

    // ret type
    if (peek().type == Token::T_ID && symbolTable->getTypeTable()->isType(peek().nameId)) {
        unique_ptr<TypeAst> retType = type();
        if (retType == nullptr) return nullptr;
        proto->setRetType(move(retType));
    }

    if (peek().type == Token::T_ATTRIBUTE) {
        if (namePool->get(peek().nameId) == "__no_name_mangle") {
            proto->setNoNameMangle(true);
        } else {
            msgs->errorUnknown(loc());
            return nullptr;
        }
        next();
    }

    if (peek().type == Token::T_BRACE_L_CUR) {
        //body
        unique_ptr<BlockAst> body = block();
        if (body == nullptr) return nullptr;

        return make_unique<FuncAst>(codeLoc, move(proto), move(body));
    } else if (peek().type == Token::T_SEMICOLON) {
        next();
        return proto;
    } else {
        msgs->errorUnknown(loc());
        return nullptr;
    }
}

unique_ptr<ImportAst> Parser::import() {
    CodeLoc codeLoc = loc();

    if (!match(Token::T_IMPORT)) {
        msgs->errorUnknown(codeLoc);
        return nullptr;
    }

    if (peek().type != Token::T_STRING) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    unique_ptr<ImportAst> ret = make_unique<ImportAst>(codeLoc, next().str);

    if (!match(Token::T_SEMICOLON)) {
        msgs->errorUnknown(loc());
        return nullptr;
    }

    return ret;
}

unique_ptr<BaseAst> Parser::parseNode() {
    if (lex == nullptr) {
        return nullptr;
    }

    unique_ptr<BaseAst> next;

    if (peek().type == Token::T_IMPORT) next = import();
    else if (peek().type == Token::T_FNC) next = func();
    else next = decl();

    return next;
}