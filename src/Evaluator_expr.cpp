#include "Evaluator.h"
#include "Codegen.h"
using namespace std;

NodeVal Evaluator::evaluateExpr(const AstNode *ast, const NodeVal &first) {
    if (first.isOper()) {
        if (ast->children.size() == 2) {
            return evaluateOperUnary(ast, first);
        } else {
            return evaluateOper(ast, first);
        }
    } else {
        msgs->errorEvaluationNotSupported(ast->children[0]->codeLoc);
        return NodeVal();
    }
}

// TODO! implicit is i64 if i32 doesn't fit
NodeVal Evaluator::evaluateUntypedVal(const AstNode *ast) {
    NodeVal ret(NodeVal::Kind::kUntyVal);

    ret.untyVal.val = ast->terminal.value().val;
    if (ret.untyVal.val.kind == LiteralVal::Kind::kNone) {
        // should not happen
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    switch (ret.untyVal.val.kind) {
    case LiteralVal::Kind::kBool:
        ret.untyVal.type = getPrimTypeId(TypeTable::P_BOOL);
        break;
    case LiteralVal::Kind::kSint:
        ret.untyVal.type = getPrimTypeId(TypeTable::P_I32);
        break;
    case LiteralVal::Kind::kChar:
        ret.untyVal.type = getPrimTypeId(TypeTable::P_C8);
        break;
    case LiteralVal::Kind::kFloat:
        ret.untyVal.type = getPrimTypeId(TypeTable::P_F32);
        break;
    case LiteralVal::Kind::kString:
        ret.untyVal.type = getTypeTable()->getTypeIdStr();
        break;
    case LiteralVal::Kind::kNull:
        ret.untyVal.type = getPrimTypeId(TypeTable::P_PTR);
        break;
    default:
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    if (ast->hasType()) {
        optional<TypeTable::Id> ty = getType(ast->type.value().get(), true);
        if (!ty.has_value()) return NodeVal();

        if (!isImplicitCastable(ret.untyVal, ty.value(), stringPool, getTypeTable())) {
            msgs->errorExprCannotPromote(ast->codeLoc, ty.value());
            return NodeVal();
        }

        ret.untyVal.type = ty.value();
    }

    return ret;
}

NodeVal Evaluator::evaluateOperUnary(const AstNode *ast, const NodeVal &first) {
    if (!codegen->checkExactlyChildren(ast, 2, true)) {
        return NodeVal();
    }

    const AstNode *nodeVal = ast->children[1].get();

    Token::Oper op = first.oper;

    NodeVal exprPay = evaluateNode(nodeVal);
    if (!codegen->checkIsUntyped(nodeVal->codeLoc, exprPay, true)) return NodeVal();

    return calculate(ast->codeLoc, op, exprPay.untyVal);
}

NodeVal Evaluator::evaluateOper(CodeLoc codeLoc, Token::Oper op, const NodeVal &lhs, const NodeVal &rhs) {
    NodeVal exprPayL, exprPayR, exprPayRet;

    exprPayL = lhs;
    if (!codegen->checkIsUntyped(codeLoc, exprPayL, true)) return NodeVal();

    exprPayR = rhs;
    if (!codegen->checkIsUntyped(codeLoc, exprPayR, true)) return NodeVal();

    return calculate(codeLoc, op, exprPayL.untyVal, exprPayR.untyVal);
}

NodeVal Evaluator::evaluateOper(const AstNode *ast, const NodeVal &first) {
    OperInfo opInfo = operInfos.at(first.oper);

    if (opInfo.variadic ?
        !codegen->checkAtLeastChildren(ast, 3, true) :
        !codegen->checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    if (opInfo.l_assoc) {
        const AstNode *nodeLhs = ast->children[1].get();
        NodeVal lhsVal = evaluateNode(nodeLhs);

        for (size_t i = 2; i < ast->children.size(); ++i) {
            const AstNode *nodeRhs = ast->children[i].get();

            lhsVal = evaluateOper(nodeRhs->codeLoc, first.oper, lhsVal, evaluateNode(nodeRhs));
            if (lhsVal.isInvalid()) return NodeVal();
        }

        return lhsVal;
    } else {
        const AstNode *nodeRhs = ast->children.back().get();
        NodeVal rhsVal = evaluateNode(nodeRhs);

        for (size_t i = ast->children.size()-2;; --i) {
            const AstNode *nodeLhs = ast->children[i].get();

            rhsVal = evaluateOper(nodeLhs->codeLoc, first.oper, evaluateNode(nodeLhs), rhsVal);
            if (rhsVal.isInvalid()) return NodeVal();

            if (i == 1) break;
        }

        return rhsVal;
    }
}

NodeVal Evaluator::calculate(CodeLoc codeLoc, Token::Oper op, UntypedVal unty) {
    NodeVal exprRet(NodeVal::Kind::kUntyVal);
    exprRet.untyVal.type = unty.type;
    exprRet.untyVal.val.kind = unty.val.kind;
    if (op == Token::O_ADD) {
        if (unty.val.kind != LiteralVal::Kind::kSint && unty.val.kind != LiteralVal::Kind::kFloat) {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
        exprRet.untyVal = unty;
    } else if (op == Token::O_SUB) {
        if (unty.val.kind == LiteralVal::Kind::kSint) {
            exprRet.untyVal.val.val_si = -unty.val.val_si;
        } else if (unty.val.kind == LiteralVal::Kind::kFloat) {
            exprRet.untyVal.val.val_f = -unty.val.val_f;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else if (op == Token::O_BIT_NOT) {
        if (unty.val.kind == LiteralVal::Kind::kSint) {
            exprRet.untyVal.val.val_si = ~unty.val.val_si;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else if (op == Token::O_NOT) {
        if (unty.val.kind == LiteralVal::Kind::kBool) {
            exprRet.untyVal.val.val_b = !unty.val.val_b;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else {
        if (op == Token::O_MUL && unty.val.kind == LiteralVal::Kind::kNull) {
            msgs->errorExprUnOnNull(codeLoc, op);
        } else if (op == Token::O_BIT_AND) {
            msgs->errorExprAddressOfNoRef(codeLoc);
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
        }
        return NodeVal();
    }
    return exprRet;
}

// TODO warn on over/underflow
NodeVal Evaluator::calculate(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR) {
    if (untyL.val.kind != untyR.val.kind) {
        msgs->errorExprUntyMismatch(codeLoc);
        return NodeVal();
    }
    if (untyL.val.kind == LiteralVal::Kind::kNone) {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    UntypedVal untyValRet(untyL.type);
    untyValRet.val.kind = untyL.val.kind;

    if (untyValRet.val.kind == LiteralVal::Kind::kBool) {
        switch (op) {
        case Token::O_EQ:
            untyValRet.val.val_b = untyL.val.val_b == untyR.val.val_b;
            break;
        case Token::O_NEQ:
            untyValRet.val.val_b = untyL.val.val_b != untyR.val.val_b;
            break;
        // AND and OR handled with the non-untyVal cases
        default:
            msgs->errorExprUntyBinBadOp(codeLoc, op);
            return NodeVal();
        }
    } else if (untyValRet.val.kind == LiteralVal::Kind::kNull) {
        // both are null
        switch (op) {
        case Token::O_EQ:
            untyValRet.type = getPrimTypeId(TypeTable::P_BOOL);
            untyValRet.val.kind = LiteralVal::Kind::kBool;
            untyValRet.val.val_b = true;
            break;
        case Token::O_NEQ:
            untyValRet.type = getPrimTypeId(TypeTable::P_BOOL);
            untyValRet.val.kind = LiteralVal::Kind::kBool;
            untyValRet.val.val_b = false;
            break;
        default:
            msgs->errorExprUntyBinBadOp(codeLoc, op);
            return NodeVal();
        }
    } else {
        bool isTypeI = untyValRet.val.kind == LiteralVal::Kind::kSint;
        bool isTypeC = untyValRet.val.kind == LiteralVal::Kind::kChar;
        bool isTypeF = untyValRet.val.kind == LiteralVal::Kind::kFloat;

        if (!isTypeI && !isTypeC && !isTypeF) {
            // NOTE cannot == nor != on two string literals
            if (untyValRet.val.kind == LiteralVal::Kind::kString) {
                msgs->errorExprCompareStringLits(codeLoc);
            } else {
                msgs->errorExprUntyBinBadOp(codeLoc, op);
            }
            return NodeVal();
        } else {
            switch (op) {
                case Token::O_ADD:
                    if (isTypeF)
                        untyValRet.val.val_f = untyL.val.val_f+untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_si = untyL.val.val_si+untyR.val.val_si;
                    break;
                case Token::O_SUB:
                    if (isTypeF)
                        untyValRet.val.val_f = untyL.val.val_f-untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_si = untyL.val.val_si-untyR.val.val_si;
                    break;
                case Token::O_SHL:
                    if (isTypeI) {
                        untyValRet.val.val_si = untyL.val.val_si<<untyR.val.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_SHR:
                    if (isTypeI) {
                        untyValRet.val.val_si = untyL.val.val_si>>untyR.val.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_BIT_AND:
                    if (isTypeI) {
                        untyValRet.val.val_si = untyL.val.val_si&untyR.val.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_BIT_XOR:
                    if (isTypeI) {
                        untyValRet.val.val_si = untyL.val.val_si^untyR.val.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_BIT_OR:
                    if (isTypeI) {
                        untyValRet.val.val_si = untyL.val.val_si|untyR.val.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_MUL:
                    if (isTypeF)
                        untyValRet.val.val_f = untyL.val.val_f*untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_si = untyL.val.val_si*untyR.val.val_si;
                    break;
                case Token::O_DIV:
                    if (isTypeF)
                        untyValRet.val.val_f = untyL.val.val_f/untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_si = untyL.val.val_si/untyR.val.val_si;
                    break;
                case Token::O_REM:
                    if (isTypeF)
                        untyValRet.val.val_f = fmod(untyL.val.val_f, untyR.val.val_f);
                    else if (isTypeI)
                        untyValRet.val.val_si = untyL.val.val_si%untyR.val.val_si;
                    break;
                case Token::O_EQ:
                    untyValRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    untyValRet.val.kind = LiteralVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val.val_b = untyL.val.val_f == untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_b = untyL.val.val_si == untyR.val.val_si;
                    else if (isTypeC)
                        untyValRet.val.val_b = untyL.val.val_c == untyR.val.val_c;
                    break;
                case Token::O_NEQ:
                    untyValRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    untyValRet.val.kind = LiteralVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val.val_b = untyL.val.val_f != untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_b = untyL.val.val_si != untyR.val.val_si;
                    else if (isTypeC)
                        untyValRet.val.val_b = untyL.val.val_c != untyR.val.val_c;
                    break;
                case Token::O_LT:
                    untyValRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    untyValRet.val.kind = LiteralVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val.val_b = untyL.val.val_f < untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_b = untyL.val.val_si < untyR.val.val_si;
                    else if (isTypeC)
                        untyValRet.val.val_b = untyL.val.val_c < untyR.val.val_c;
                    break;
                case Token::O_LTEQ:
                    untyValRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    untyValRet.val.kind = LiteralVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val.val_b = untyL.val.val_f <= untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_b = untyL.val.val_si <= untyR.val.val_si;
                    else if (isTypeC)
                        untyValRet.val.val_b = untyL.val.val_c <= untyR.val.val_c;
                    break;
                case Token::O_GT:
                    untyValRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    untyValRet.val.kind = LiteralVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val.val_b = untyL.val.val_f > untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_b = untyL.val.val_si > untyR.val.val_si;
                    else if (isTypeC)
                        untyValRet.val.val_b = untyL.val.val_c > untyR.val.val_c;
                    break;
                case Token::O_GTEQ:
                    untyValRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    untyValRet.val.kind = LiteralVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val.val_b = untyL.val.val_f >= untyR.val.val_f;
                    else if (isTypeI)
                        untyValRet.val.val_b = untyL.val.val_si >= untyR.val.val_si;
                    else if (isTypeC)
                        untyValRet.val.val_b = untyL.val.val_c >= untyR.val.val_c;
                    break;
                default:
                    msgs->errorExprUntyBinBadOp(codeLoc, op);
                    return NodeVal();
            }
        }
    }

    NodeVal ret(NodeVal::Kind::kUntyVal);
    ret.untyVal = untyValRet;
    return ret;
}