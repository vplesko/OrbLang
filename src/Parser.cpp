#include "Parser.h"
#include <iostream>
#include <sstream>
#include <cstdint>
#include <unordered_set>
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

// If the next token matches the type, eats it and returns true.
// Otherwise, files an error and returns false.
bool Parser::matchOrError(Token::Type type) {
    if (!match(type)) {
        msgs->errorUnexpectedTokenType(loc(), type, peek());
        return false;
    }
    return true;
}

CodeLoc Parser::loc() const {
    return lex->loc();
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
        Token tok = next();
        ret = make_unique<VarExprAst>(codeLoc, tok.nameId);
    } else {
        msgs->errorNotPrim(codeLoc);
        return nullptr;
    }

    return ret;
}

unique_ptr<ExprAst> Parser::expr() {
    unique_ptr<ExprAst> retExpr;

    if (peek().type != Token::T_BRACE_L_REG) {
        retExpr = prim();
    } else {
        next(); // eat left brace

        if (peek().type == Token::T_OPER) {
            CodeLoc codeLocOp = loc();
            Token tokOp = next();

            unique_ptr<ExprAst> lhsExpr = expr();
            if (lhsExpr == nullptr) return nullptr;

            if (!operInfos.at(tokOp.op).binary) {
                // if not binary, must be unary
                retExpr = make_unique<UnExprAst>(codeLocOp, move(lhsExpr), tokOp.op);
            } else {
                unique_ptr<ExprAst> rhsExpr;
                if (!operInfos.at(tokOp.op).unary || peek().type != Token::T_BRACE_R_REG) {
                    rhsExpr = expr();
                    if (rhsExpr == nullptr) return nullptr;
                }

                if (rhsExpr != nullptr) {
                    retExpr = make_unique<BinExprAst>(codeLocOp, move(lhsExpr), move(rhsExpr), tokOp.op);
                } else {
                    retExpr = make_unique<UnExprAst>(codeLocOp, move(lhsExpr), tokOp.op);
                }
            }
        } else if (peek().type == Token::T_ID) {
            CodeLoc codeLocCall = loc();
            Token tokFncId = next();
            
            unique_ptr<CallExprAst> call = make_unique<CallExprAst>(codeLocCall, tokFncId.nameId);

            while (peek().type != Token::T_BRACE_R_REG) {
                unique_ptr<ExprAst> arg = expr();
                if (arg == nullptr) return nullptr;

                call->addArg(move(arg));
            }

            retExpr = move(call);
        } else if (peek().type == Token::T_ARR) {
            CodeLoc codeLocArr = loc();
            next();

            unique_ptr<TypeAst> arrTy = type();
            if (arrTy == nullptr) return nullptr;

            vector<unique_ptr<ExprAst>> vals;

            while (peek().type != Token::T_BRACE_R_REG) {
                unique_ptr<ExprAst> ex = expr();
                if (ex == nullptr) return nullptr;

                vals.push_back(move(ex));
            }

            retExpr = make_unique<ArrayExprAst>(arrTy->loc(), move(arrTy), move(vals));
        } else if (peek().type == Token::T_CAST) {
            CodeLoc codeLocCast = loc();
            next();

            unique_ptr<TypeAst> ty = type();
            if (ty == nullptr) return nullptr;

            unique_ptr<ExprAst> e = expr();
            if (e == nullptr) return nullptr;

            retExpr = make_unique<CastExprAst>(ty->loc(), move(ty), move(e));
        } else if (peek().type == Token::T_BRACE_L_SQR) {
            CodeLoc codeLocInd = loc();
            next();

            if (!matchOrError(Token::T_BRACE_R_SQR))
                return nullptr;
            
            unique_ptr<ExprAst> baseExpr = expr();
            if (baseExpr == nullptr) return nullptr;

            unique_ptr<ExprAst> indExpr = expr();
            if (indExpr == nullptr) return nullptr;

            retExpr = make_unique<IndExprAst>(codeLocInd, move(baseExpr), move(indExpr));
        } else if (peek().type == Token::T_DOT) {
            CodeLoc codeLocDot = loc();
            next();
            
            unique_ptr<ExprAst> baseExpr = expr();
            if (baseExpr == nullptr) return nullptr;

            CodeLoc codeLocMember = loc();
            Token member = next();
            if (member.type != Token::T_ID) {
                msgs->errorUnexpectedTokenType(codeLocMember, Token::T_ID, member);
                return nullptr;
            }
            unique_ptr<VarExprAst> varExpr = make_unique<VarExprAst>(codeLocMember, member.nameId);

            retExpr = make_unique<DotExprAst>(codeLocDot, move(baseExpr), move(varExpr));
        } else {
            msgs->errorUnexpectedTokenType(loc(),
                {Token::T_OPER, Token::T_ID, Token::T_ARR, Token::T_CAST, Token::T_BRACE_L_SQR, Token::T_DOT},
                peek());
            return nullptr;
        }

        if (!matchOrError(Token::T_BRACE_R_REG)) {
            return nullptr;
        }
    }

    return retExpr;
}

std::unique_ptr<TypeAst> Parser::type() {
    CodeLoc codeLocType = loc();

    bool isDescr = false;
    if (peek().type == Token::T_BRACE_L_REG) {
        next();
        isDescr = true;
    } else if (peek().type != Token::T_ID) {
        msgs->errorUnexpectedTokenType(codeLocType, {Token::T_ID, Token::T_BRACE_L_REG}, peek());
        return nullptr;
    }

    Token typeTok = next();
    optional<TypeTable::Id> baseTypeId = symbolTable->getTypeTable()->getTypeId(typeTok.nameId);

    if (!baseTypeId.has_value()) {
        msgs->errorNotTypeId(codeLocType, typeTok.nameId);
        return nullptr;
    }

    if (!isDescr) {
        return make_unique<TypeAst>(codeLocType, baseTypeId.value());
    }

    TypeTable::TypeDescr typeDescr(baseTypeId.value());

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
                CodeLoc codeLocInd = loc();
                Token ind = next();
                if (ind.type != Token::T_NUM) {
                    msgs->errorUnexpectedTokenType(codeLocInd, Token::T_NUM, ind);
                    return nullptr;
                }
                if (ind.num <= 0) {
                    msgs->errorBadArraySize(codeLocInd, ind.num);
                    return nullptr;
                }

                if (!matchOrError(Token::T_BRACE_R_SQR))
                    return nullptr;

                typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_ARR, (unsigned long) ind.num});
            }
        } else if (peek().type == Token::T_CN) {
            next();
            typeDescr.setLastCn();
        } else {
            break;
        }
    }

    TypeTable::Id typeId = symbolTable->getTypeTable()->addTypeDescr(move(typeDescr));

    if (!matchOrError(Token::T_BRACE_R_REG)) {
        return nullptr;
    }

    return make_unique<TypeAst>(codeLocType, typeId);
}

vector<InitInfo> Parser::inits() {
    vector<InitInfo> ret;

    while (true) {
        bool hasInit = false;

        if (peek().type == Token::T_BRACE_L_REG) {
            hasInit = true;
            next();
        } else if (peek().type != Token::T_ID) {
            if (ret.empty()) {
                msgs->errorUnexpectedTokenType(loc(), {Token::T_ID, Token::T_BRACE_L_REG}, peek());
                return {};
            }

            return ret;
        }

        unique_ptr<VarExprAst> name = make_unique<VarExprAst>(loc(), next().nameId);

        if (!matchOrError(Token::T_COLON)) {
            return {};
        }

        unique_ptr<TypeAst> ty = type();
        if (ty == nullptr) {
            return {};
        }

        unique_ptr<ExprAst> init;
        if (hasInit) {
            init = expr();
            if (init == nullptr || !matchOrError(Token::T_BRACE_R_REG)) {
                return {};
            }
        }

        ret.push_back({move(name), move(ty), move(init)});
    }
}

unique_ptr<DeclAst> Parser::decl() {
    CodeLoc codeLocDecl = loc();

    if (!matchOrError(Token::T_LET)) {
        return nullptr;
    }

    unique_ptr<DeclAst> ret = make_unique<DeclAst>(codeLocDecl);

    vector<InitInfo> initInfos = inits();
    if (initInfos.empty()) {
        return nullptr;
    }

    if (!matchOrError(Token::T_SEMICOLON)) {
        return nullptr;
    }

    ret->addAll(move(initInfos));

    return ret;
}

std::unique_ptr<StmntAst> Parser::simple() {
    if (peek().type == Token::T_SEMICOLON) {
        CodeLoc codeLoc = loc();
        next();
        return make_unique<EmptyStmntAst>(codeLoc);
    } else if (peek().type == Token::T_LET) {
        return decl();
    } else {
        return expr();
    }
}

std::unique_ptr<StmntAst> Parser::if_stmnt() {
    CodeLoc codeLocIf = loc();
    
    if (!matchOrError(Token::T_IF) || !matchOrError(Token::T_BRACE_L_REG))
        return nullptr;

    unique_ptr<StmntAst> init;
    unique_ptr<ExprAst> cond;

    init = simple();
    if (init == nullptr) {
        msgs->errorNotSimple(loc());
        return nullptr;
    }

    if (init->type() == AST_Decl || init->type() == AST_Empty) {
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

    if (!matchOrError(Token::T_BRACE_R_REG))
        return nullptr;

    unique_ptr<StmntAst> thenBody = stmnt();
    if (thenBody == nullptr) return nullptr;

    unique_ptr<StmntAst> elseBody;
    if (peek().type == Token::T_ELSE) {
        next();

        elseBody = stmnt();
        if (elseBody == nullptr) return nullptr;
    }

    return make_unique<IfAst>(codeLocIf, move(init), move(cond), move(thenBody), move(elseBody));
}

std::unique_ptr<StmntAst> Parser::for_stmnt() {
    CodeLoc codeLoc = loc();

    if (!matchOrError(Token::T_FOR) || !matchOrError(Token::T_BRACE_L_REG))
        return nullptr;

    unique_ptr<StmntAst> init = simple();
    if (init == nullptr) {
        msgs->errorNotSimple(loc());
        return nullptr;
    }

    if (init->type() != AST_Decl && init->type() != AST_Empty) {
        // init was expression, need to eat semicolon
        if (!matchOrError(Token::T_SEMICOLON))
            return nullptr;
    }

    unique_ptr<ExprAst> cond;
    if (peek().type != Token::T_SEMICOLON) {
        cond = expr();
        if (cond == nullptr) return nullptr;
    }
    
    if (!matchOrError(Token::T_SEMICOLON))
        return nullptr;

    unique_ptr<ExprAst> iter;
    if (peek().type != Token::T_BRACE_R_REG) {
        iter = expr();
        if (iter == nullptr) return nullptr;
    }
    
    if (!matchOrError(Token::T_BRACE_R_REG))
        return nullptr;

    unique_ptr<StmntAst> body = stmnt();
    if (body == nullptr) return nullptr;

    return make_unique<ForAst>(codeLoc, move(init), move(cond), move(iter), move(body));
}

std::unique_ptr<StmntAst> Parser::while_stmnt() {
    CodeLoc codeLocWhile = loc();

    if (!matchOrError(Token::T_WHILE) || !matchOrError(Token::T_BRACE_L_REG))
        return nullptr;

    unique_ptr<ExprAst> cond = expr();
    if (cond == nullptr) return nullptr;

    if (!matchOrError(Token::T_BRACE_R_REG))
        return nullptr;

    unique_ptr<StmntAst> body = stmnt();
    if (body == nullptr) return nullptr;

    return make_unique<WhileAst>(codeLocWhile, move(cond), move(body));
}

std::unique_ptr<StmntAst> Parser::do_while_stmnt() {
    CodeLoc codeLocDo = loc();

    if (!matchOrError(Token::T_DO))
        return nullptr;

    unique_ptr<StmntAst> body = stmnt();
    if (body == nullptr) return nullptr;

    if (!matchOrError(Token::T_WHILE) || !matchOrError(Token::T_BRACE_L_REG))
        return nullptr;
    
    unique_ptr<ExprAst> cond = expr();
    if (cond == nullptr) return nullptr;

    if (!matchOrError(Token::T_BRACE_R_REG) || !matchOrError(Token::T_SEMICOLON))
        return nullptr;

    return make_unique<DoWhileAst>(codeLocDo, move(body), move(cond));
}

std::unique_ptr<StmntAst> Parser::break_stmnt() {
    CodeLoc codeLoc = loc();

    if (!matchOrError(Token::T_BREAK) || !matchOrError(Token::T_SEMICOLON))
        return nullptr;

    return make_unique<BreakAst>(codeLoc);
}

std::unique_ptr<StmntAst> Parser::continue_stmnt() {
    CodeLoc codeLoc = loc();

    if (!matchOrError(Token::T_CONTINUE) || !matchOrError(Token::T_SEMICOLON))
        return nullptr;

    return make_unique<ContinueAst>(codeLoc);
}

std::unique_ptr<StmntAst> Parser::ret() {
    CodeLoc codeLoc = loc();

    if (!matchOrError(Token::T_RET))
        return nullptr;

    unique_ptr<ExprAst> val;
    if (peek().type != Token::T_SEMICOLON) {
        val = expr();
        if (val == nullptr) return nullptr;
    }

    if (!matchOrError(Token::T_SEMICOLON))
        return nullptr;

    return make_unique<RetAst>(codeLoc, move(val));
}

unique_ptr<StmntAst> Parser::stmnt() {
    if (peek().type == Token::T_IF) return if_stmnt();

    if (peek().type == Token::T_FOR) return for_stmnt();

    if (peek().type == Token::T_WHILE) return while_stmnt();

    if (peek().type == Token::T_DO) return do_while_stmnt();

    if (peek().type == Token::T_BREAK) return break_stmnt();

    if (peek().type == Token::T_CONTINUE) return continue_stmnt();

    if (peek().type == Token::T_RET) return ret();

    if (peek().type == Token::T_BRACE_L_CUR) return block();
    
    unique_ptr<StmntAst> st = simple();
    if (st == nullptr) return nullptr;
    if (st->type() != AST_Empty && st->type() != AST_Decl) {
        if (!matchOrError(Token::T_SEMICOLON))
            return nullptr;
    }
    
    return st;
}

unique_ptr<BlockAst> Parser::block() {
    CodeLoc codeLoc = loc();

    if (!matchOrError(Token::T_BRACE_L_CUR))
        return nullptr;

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
    CodeLoc codeLocFunc = loc();

    if (!matchOrError(Token::T_FNC))
        return nullptr;

    CodeLoc codeLocName = loc();
    Token name = next();
    if (name.type != Token::T_ID) {
        msgs->errorUnexpectedTokenType(codeLocName, Token::T_ID, name);
        return nullptr;
    }

    unique_ptr<FuncProtoAst> proto = make_unique<FuncProtoAst>(codeLocFunc, name.nameId);
    proto->setNoNameMangle(name.nameId == namePool->getMain());

    if (!matchOrError(Token::T_BRACE_L_REG))
        return nullptr;

    // args
    bool last = false;
    while (true) {
        if (peek().type == Token::T_BRACE_R_REG) {
            next();
            break;
        }
        if (last && peek().type != Token::T_BRACE_R_REG) {
            msgs->errorNotLastParam(loc());
            return nullptr;
        }

        if (peek().type == Token::T_ELLIPSIS) {
            next();
            proto->setVariadic(true);
            last = true;
        } else {
            CodeLoc codeLocArg = loc();
            Token arg = next();
            if (arg.type != Token::T_ID) {
                msgs->errorUnexpectedTokenType(codeLocArg, Token::T_ID, arg);
                return nullptr;
            }

            if (!matchOrError(Token::T_COLON)) {
                return nullptr;
            }

            unique_ptr<TypeAst> argType = type();
            if (argType == nullptr) return nullptr;

            proto->addArg(make_pair(move(argType), arg.nameId));
        }
    }

    // ret type
    if ((peek().type == Token::T_ID && symbolTable->getTypeTable()->isType(peek().nameId)) ||
        peek().type == Token::T_BRACE_L_REG) {
        unique_ptr<TypeAst> retType = type();
        if (retType == nullptr) return nullptr;
        proto->setRetType(move(retType));
    }

    if (peek().type == Token::T_ATTRIBUTE) {
        if (peek().attr == Token::A_NO_NAME_MANGLE) {
            proto->setNoNameMangle(true);
        } else {
            msgs->errorBadAttr(loc(), peek().attr);
            return nullptr;
        }
        next();
    }

    if (peek().type == Token::T_BRACE_L_CUR) {
        //body
        unique_ptr<BlockAst> body = block();
        if (body == nullptr) return nullptr;

        return make_unique<FuncAst>(codeLocFunc, move(proto), move(body));
    } else if (peek().type == Token::T_SEMICOLON) {
        next();
        return proto;
    } else {
        msgs->errorUnexpectedTokenType(loc(), {Token::T_BRACE_L_CUR, Token::T_SEMICOLON}, peek());
        return nullptr;
    }
}

unique_ptr<DataAst> Parser::data() {
    CodeLoc codeLocData = loc();

    if (!matchOrError(Token::T_DATA))
        return nullptr;
    
    if (peek().type != Token::T_ID) {
        msgs->errorUnexpectedTokenType(loc(), Token::T_ID, peek());
        return nullptr;
    }

    NamePool::Id dataName = next().nameId;

    if (!symbolTable->dataMayTakeName(dataName)) {
        msgs->errorDataNameTaken(codeLocData, dataName);
        return nullptr;
    }

    TypeTable::Id dataTypeId = symbolTable->getTypeTable()->addDataType(dataName);

    unique_ptr<DataAst> ret = make_unique<DataAst>(codeLocData, dataTypeId);

    if (peek().type == Token::T_SEMICOLON) {
        next();
        return ret;
    }

    if (!match(Token::T_BRACE_L_CUR)) {
        msgs->errorUnexpectedTokenType(loc(), {Token::T_BRACE_L_CUR, Token::T_SEMICOLON}, peek());
        return nullptr;
    }
    
    unordered_set<NamePool::Id> memberNames;

    vector<InitInfo> members = inits();
    if (members.empty()) return nullptr;

    for (const InitInfo &m : members) {
        TypeTable::Id memberTypeId = m.type->getTypeId();
        if (!getTypeTable()->isNonOpaqueType(memberTypeId)) {
            if (getTypeTable()->isDataType(memberTypeId)) {
                msgs->errorDataOpaqueInit(m.type->loc());
            } else {
                msgs->errorUndefinedType(m.type->loc());
            }
            return nullptr;
        }
    }

    for (const InitInfo &m : members) {
        if (m.init != nullptr) {
            msgs->errorDataMemberInit(m.init->loc());
            return nullptr;
        }

        NamePool::Id memberName = m.name->getNameId();
        if (memberNames.find(memberName) != memberNames.end()) {
            msgs->errorDataMemberNameDuplicate(m.name->loc(), memberName);
            return nullptr;
        }
        memberNames.insert(memberName);
    }

    for (InitInfo &m : members) {
        ret->addMember(move(m));
    }
    
    if (!matchOrError(Token::T_BRACE_R_CUR)) {
        return nullptr;
    }

    if (memberNames.empty()) {
        msgs->errorDataNoMembers(codeLocData, dataName);
        return nullptr;
    }

    return ret;
}

unique_ptr<ImportAst> Parser::import() {
    CodeLoc codeLocImport = loc();

    if (!matchOrError(Token::T_IMPORT))
        return nullptr;

    if (peek().type != Token::T_STRING) {
        msgs->errorUnexpectedTokenType(loc(), Token::T_STRING, peek());
        return nullptr;
    }

    unique_ptr<ImportAst> ret = make_unique<ImportAst>(codeLocImport, next().str);

    if (!matchOrError(Token::T_SEMICOLON))
        return nullptr;

    return ret;
}

unique_ptr<BaseAst> Parser::parseNode() {
    if (lex == nullptr) {
        return nullptr;
    }

    unique_ptr<BaseAst> next;

    if (peek().type == Token::T_IMPORT) next = import();
    else if (peek().type == Token::T_FNC) next = func();
    else if (peek().type == Token::T_DATA) next = data();
    else if (peek().type == Token::T_LET) next = decl();
    else {
        msgs->errorUnexpectedTokenType(loc(),
            {Token::T_IMPORT, Token::T_FNC, Token::T_DATA, Token::T_LET}, peek());
        return nullptr;
    }

    return next;
}