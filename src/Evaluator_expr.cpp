#include "Evaluator.h"
#include "Codegen.h"
using namespace std;

NodeVal Evaluator::evaluateUntypedVal(const AstNode *ast) {
    UntypedVal val = ast->terminal.value().val;

    if (val.kind == UntypedVal::Kind::kNone) {
        // should not happen
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kUntyVal);
    ret.untyVal = val;
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
    exprRet.untyVal.kind = unty.kind;
    if (op == Token::O_ADD) {
        if (unty.kind != UntypedVal::Kind::kSint && unty.kind != UntypedVal::Kind::kFloat) {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
        exprRet.untyVal = unty;
    } else if (op == Token::O_SUB) {
        if (unty.kind == UntypedVal::Kind::kSint) {
            exprRet.untyVal.val_si = -unty.val_si;
        } else if (unty.kind == UntypedVal::Kind::kFloat) {
            exprRet.untyVal.val_f = -unty.val_f;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else if (op == Token::O_BIT_NOT) {
        if (unty.kind == UntypedVal::Kind::kSint) {
            exprRet.untyVal.val_si = ~unty.val_si;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else if (op == Token::O_NOT) {
        if (unty.kind == UntypedVal::Kind::kBool) {
            exprRet.untyVal.val_b = !unty.val_b;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else {
        if (op == Token::O_MUL && unty.kind == UntypedVal::Kind::kNull) {
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

NodeVal Evaluator::calculate(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR) {
    if (untyL.kind != untyR.kind) {
        msgs->errorExprUntyMismatch(codeLoc);
        return NodeVal();
    }
    if (untyL.kind == UntypedVal::Kind::kNone) {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    UntypedVal untyValRet;
    untyValRet.kind = untyL.kind;

    if (untyValRet.kind == UntypedVal::Kind::kBool) {
        switch (op) {
        case Token::O_EQ:
            untyValRet.val_b = untyL.val_b == untyR.val_b;
            break;
        case Token::O_NEQ:
            untyValRet.val_b = untyL.val_b != untyR.val_b;
            break;
        // AND and OR handled with the non-untyVal cases
        default:
            msgs->errorExprUntyBinBadOp(codeLoc, op);
            return NodeVal();
        }
    } else if (untyValRet.kind == UntypedVal::Kind::kNull) {
        // both are null
        switch (op) {
        case Token::O_EQ:
            untyValRet.kind = UntypedVal::Kind::kBool;
            untyValRet.val_b = true;
            break;
        case Token::O_NEQ:
            untyValRet.kind = UntypedVal::Kind::kBool;
            untyValRet.val_b = false;
            break;
        default:
            msgs->errorExprUntyBinBadOp(codeLoc, op);
            return NodeVal();
        }
    } else {
        bool isTypeI = untyValRet.kind == UntypedVal::Kind::kSint;
        bool isTypeC = untyValRet.kind == UntypedVal::Kind::kChar;
        bool isTypeF = untyValRet.kind == UntypedVal::Kind::kFloat;

        if (!isTypeI && !isTypeC && !isTypeF) {
            // NOTE cannot == nor != on two string literals
            if (untyValRet.kind == UntypedVal::Kind::kString) {
                msgs->errorExprCompareStringLits(codeLoc);
            } else {
                msgs->errorExprUntyBinBadOp(codeLoc, op);
            }
            return NodeVal();
        } else {
            switch (op) {
                case Token::O_ADD:
                    if (isTypeF)
                        untyValRet.val_f = untyL.val_f+untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_si = untyL.val_si+untyR.val_si;
                    break;
                case Token::O_SUB:
                    if (isTypeF)
                        untyValRet.val_f = untyL.val_f-untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_si = untyL.val_si-untyR.val_si;
                    break;
                case Token::O_SHL:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si<<untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_SHR:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si>>untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_BIT_AND:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si&untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_BIT_XOR:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si^untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_BIT_OR:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si|untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return NodeVal();
                    }
                    break;
                case Token::O_MUL:
                    if (isTypeF)
                        untyValRet.val_f = untyL.val_f*untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_si = untyL.val_si*untyR.val_si;
                    break;
                case Token::O_DIV:
                    if (isTypeF)
                        untyValRet.val_f = untyL.val_f/untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_si = untyL.val_si/untyR.val_si;
                    break;
                case Token::O_REM:
                    if (isTypeF)
                        untyValRet.val_f = fmod(untyL.val_f, untyR.val_f);
                    else if (isTypeI)
                        untyValRet.val_si = untyL.val_si%untyR.val_si;
                    break;
                case Token::O_EQ:
                    untyValRet.kind = UntypedVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f == untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si == untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c == untyR.val_c;
                    break;
                case Token::O_NEQ:
                    untyValRet.kind = UntypedVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f != untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si != untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c != untyR.val_c;
                    break;
                case Token::O_LT:
                    untyValRet.kind = UntypedVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f < untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si < untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c < untyR.val_c;
                    break;
                case Token::O_LTEQ:
                    untyValRet.kind = UntypedVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f <= untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si <= untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c <= untyR.val_c;
                    break;
                case Token::O_GT:
                    untyValRet.kind = UntypedVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f > untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si > untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c > untyR.val_c;
                    break;
                case Token::O_GTEQ:
                    untyValRet.kind = UntypedVal::Kind::kBool;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f >= untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si >= untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c >= untyR.val_c;
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