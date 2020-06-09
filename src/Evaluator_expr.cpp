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

NodeVal Evaluator::evaluateUntypedVal(const AstNode *ast) {
    NodeVal ret(NodeVal::Kind::kUntyVal);

    const LiteralVal &lit = ast->terminal.value().val;

    if (lit.kind == LiteralVal::Kind::kNone) {
        // should not happen
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    switch (lit.kind) {
    case LiteralVal::Kind::kBool:
        ret.untyVal.type = getPrimTypeId(TypeTable::P_BOOL);
        ret.untyVal.b = lit.val_b;
        break;
    case LiteralVal::Kind::kSint:
        {
            TypeTable::PrimIds fitting = getTypeTable()->shortestFittingPrimTypeI(lit.val_si);
            TypeTable::PrimIds chosen = max(TypeTable::P_I32, fitting);
            ret.untyVal.type = getPrimTypeId(chosen);
            if (chosen == TypeTable::P_I32) ret.untyVal.i32 = lit.val_si;
            else ret.untyVal.i64 = lit.val_si;
            break;
        }
    case LiteralVal::Kind::kChar:
        ret.untyVal.type = getPrimTypeId(TypeTable::P_C8);
        ret.untyVal.c8 = lit.val_c;
        break;
    case LiteralVal::Kind::kFloat:
        {
            TypeTable::PrimIds fitting = getTypeTable()->shortestFittingPrimTypeF(lit.val_f);
            TypeTable::PrimIds chosen = max(TypeTable::P_F32, fitting);
            ret.untyVal.type = getPrimTypeId(chosen);
            if (chosen == TypeTable::P_F32) ret.untyVal.f32 = lit.val_f;
            else ret.untyVal.f64 = lit.val_f;
            break;
        }
    case LiteralVal::Kind::kString:
        ret.untyVal.type = getTypeTable()->getTypeIdStr();
        ret.untyVal.str = lit.val_str;
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

        if (ret.untyVal.type != ty.value()) {
            if (!UntypedVal::isImplicitCastable(ret.untyVal, ty.value(), stringPool, getTypeTable())) {
                msgs->errorExprCannotPromote(ast->codeLoc, ty.value());
                return NodeVal();
            }

            if (!cast(ret.untyVal, ty.value())) {
                msgs->errorInternal(ast->codeLoc);
                return NodeVal();
            }
        }
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

bool Evaluator::isI(const UntypedVal &val) const {
    return getTypeTable()->worksAsTypeI(val.type);
}

bool Evaluator::isU(const UntypedVal &val) const {
    return getTypeTable()->worksAsTypeU(val.type);
}

bool Evaluator::isF(const UntypedVal &val) const {
    return getTypeTable()->worksAsTypeF(val.type);
}

bool Evaluator::isB(const UntypedVal &val) const {
    return getTypeTable()->worksAsTypeB(val.type);
}

bool Evaluator::isC(const UntypedVal &val) const {
    return getTypeTable()->worksAsTypeC(val.type);
}

bool Evaluator::isStr(const UntypedVal &val) const {
    return getTypeTable()->worksAsTypeStr(val.type);
}

bool Evaluator::isNull(const UntypedVal &val) const {
    return getTypeTable()->worksAsTypeAnyP(val.type);
}

// using macros makes me feel naughty
#define ASSIGN_BASED_ON_TYPE_IU(val, x, t) \
    if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_I8)) { \
        val.i8 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_I16)) { \
        val.i16 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_I32)) { \
        val.i32 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_I64)) { \
        val.i64 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_U8)) { \
        val.u8 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_U16)) { \
        val.u16 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_U32)) { \
        val.u32 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_U64)) { \
        val.u64 = x; \
    }

#define ASSIGN_BASED_ON_TYPE_F(val, x, t) \
    if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_F32)) { \
        val.f32 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_F64)) { \
        val.f64 = x; \
    }

#define ASSIGN_BASED_ON_TYPE_CB(val, x, t) \
    if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_C8)) { \
        val.c8 = x; \
    } else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_BOOL)) { \
        val.b = x != 0; \
    }

bool Evaluator::cast(UntypedVal &val, TypeTable::Id t) const {
    if (val.type == t) return true;

    if (isI(val)) {
        int64_t x = UntypedVal::getValueI(val, getTypeTable()).value();
        ASSIGN_BASED_ON_TYPE_IU(val, x, t)
        else ASSIGN_BASED_ON_TYPE_F(val, x, t)
        else ASSIGN_BASED_ON_TYPE_CB(val, x, t)
        else return false;
    } else if (isU(val)) {
        uint64_t x = UntypedVal::getValueU(val, getTypeTable()).value();
        ASSIGN_BASED_ON_TYPE_IU(val, x, t)
        else ASSIGN_BASED_ON_TYPE_F(val, x, t)
        else ASSIGN_BASED_ON_TYPE_CB(val, x, t)
        else return false;
    } else if (isF(val)) {
        double x = UntypedVal::getValueF(val, getTypeTable()).value();
        ASSIGN_BASED_ON_TYPE_IU(val, x, t)
        else ASSIGN_BASED_ON_TYPE_F(val, x, t)
        else return false;
    } else if (isC(val)) {
        char x = val.c8;
        ASSIGN_BASED_ON_TYPE_IU(val, x, t)
        else ASSIGN_BASED_ON_TYPE_CB(val, x, t)
        else return false;
    } else if (isB(val)) {
        int8_t b = val.b ? 1 : 0;
        ASSIGN_BASED_ON_TYPE_IU(val, b, t)
        else return false;
    } else if (isNull(val)) {
        if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_BOOL)) {
            val.b = false; // only null is possible
        } else {
            return false;
        }
    } else {
        return false;
    }

    val.type = t;
    return true;
}

NodeVal Evaluator::calculate(CodeLoc codeLoc, Token::Oper op, UntypedVal unty) {
    NodeVal exprRet(NodeVal::Kind::kUntyVal);
    exprRet.untyVal.type = unty.type;

    if (op == Token::O_ADD) {
        if (!isI(unty) && !isU(unty) && !isF(unty)) {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
        exprRet.untyVal = unty;
    } else if (op == Token::O_SUB) {
        if (isI(unty)) {
            int64_t x = UntypedVal::getValueI(unty, getTypeTable()).value();
            x = -x;
            ASSIGN_BASED_ON_TYPE_IU(exprRet.untyVal, x, unty.type);
        } else if (isF(unty)) {
            double x = UntypedVal::getValueF(unty, getTypeTable()).value();
            x = -x;
            ASSIGN_BASED_ON_TYPE_F(exprRet.untyVal, x, unty.type);
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else if (op == Token::O_BIT_NOT) {
        if (isI(unty)) {
            int64_t x = UntypedVal::getValueI(unty, getTypeTable()).value();
            x = ~x;
            ASSIGN_BASED_ON_TYPE_IU(exprRet.untyVal, x, unty.type);
        } else if (isU(unty)) {
            uint64_t x = UntypedVal::getValueU(unty, getTypeTable()).value();
            x = ~x;
            ASSIGN_BASED_ON_TYPE_IU(exprRet.untyVal, x, unty.type);
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else if (op == Token::O_NOT) {
        if (isB(unty)) {
            exprRet.untyVal.b = !unty.b;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else {
        if (op == Token::O_MUL && isNull(unty)) {
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

#define ASSIGN_CALC_BASED_ON_TYPE_IU(valRet, valL, valR, op) \
    if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_I8)) \
        valRet.i8 = valL.i8 op valR.i8; \
    else if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_I16)) \
        valRet.i16 = valL.i16 op valR.i16; \
    else if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_I32)) \
        valRet.i32 = valL.i32 op valR.i32; \
    else if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_I64)) \
        valRet.i64 = valL.i64 op valR.i64; \
    else if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_U8)) \
        valRet.u8 = valL.u8 op valR.u8; \
    else if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_U16)) \
        valRet.u16 = valL.u16 op valR.u16; \
    else if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_U32)) \
        valRet.u32 = valL.u32 op valR.u32; \
    else if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_U64)) \
        valRet.u64 = valL.u64 op valR.u64;

#define ASSIGN_CALC_BASED_ON_TYPE_C(valRet, valL, valR, op) \
    if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_C8)) \
        valRet.c8 = valL.c8 op valR.c8;

#define ASSIGN_CALC_BASED_ON_TYPE_F(valRet, valL, valR, op) \
    if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_F32)) \
        valRet.f32 = valL.f32 op valR.f32; \
    else if (getTypeTable()->worksAsPrimitive(valRet.type, TypeTable::P_F64)) \
        valRet.f64 = valL.f64 op valR.f64;

#define ASSIGN_COMP_BASED_ON_TYPE_IU(valRet, valL, valR, op) \
    if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_I8)) \
        valRet.b = valL.i8 op valR.i8; \
    else if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_I16)) \
        valRet.b = valL.i16 op valR.i16; \
    else if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_I32)) \
        valRet.b = valL.i32 op valR.i32; \
    else if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_I64)) \
        valRet.b = valL.i64 op valR.i64; \
    else if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_U8)) \
        valRet.b = valL.u8 op valR.u8; \
    else if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_U16)) \
        valRet.b = valL.u16 op valR.u16; \
    else if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_U32)) \
        valRet.b = valL.u32 op valR.u32; \
    else if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_U64)) \
        valRet.b = valL.u64 op valR.u64;

#define ASSIGN_COMP_BASED_ON_TYPE_C(valRet, valL, valR, op) \
    if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_C8)) \
        valRet.b = valL.c8 op valR.c8;

#define ASSIGN_COMP_BASED_ON_TYPE_F(valRet, valL, valR, op) \
    if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_F32)) \
        valRet.b = valL.f32 op valR.f32; \
    else if (getTypeTable()->worksAsPrimitive(valL.type, TypeTable::P_F64)) \
        valRet.b = valL.f64 op valR.f64;

// TODO warn on over/underflow
NodeVal Evaluator::calculate(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR) {
    if (untyL.type != untyR.type) {
        if (getTypeTable()->isImplicitCastable(untyR.type, untyL.type)) {
            if (!cast(untyR, untyL.type)) {
                msgs->errorInternal(codeLoc);
                return NodeVal();
            }
        } else if (getTypeTable()->isImplicitCastable(untyL.type, untyR.type)) {
            if (!cast(untyL, untyR.type)) {
                msgs->errorInternal(codeLoc);
                return NodeVal();
            }
        } else {
            msgs->errorExprCannotImplicitCastEither(codeLoc, untyL.type, untyR.type);
            return NodeVal();
        }
    }

    UntypedVal untyRet(untyL.type);

    if (isB(untyRet)) {
        switch (op) {
        case Token::O_EQ:
            untyRet.b = untyL.b == untyR.b;
            break;
        case Token::O_NEQ:
            untyRet.b = untyL.b != untyR.b;
            break;
        default:
            msgs->errorExprUntyBinBadOp(codeLoc, op);
            return NodeVal();
        }
    } else if (isNull(untyRet)) {
        // both are null
        switch (op) {
        case Token::O_EQ:
            untyRet.type = getPrimTypeId(TypeTable::P_BOOL);
            untyRet.b = true;
            break;
        case Token::O_NEQ:
            untyRet.type = getPrimTypeId(TypeTable::P_BOOL);
            untyRet.b = false;
            break;
        default:
            msgs->errorExprUntyBinBadOp(codeLoc, op);
            return NodeVal();
        }
    } else {
        bool isTypeI = isI(untyRet);
        bool isTypeU = isU(untyRet);
        bool isTypeF = isF(untyRet);
        bool isTypeC = isC(untyRet);

        if (!isTypeI && !isTypeU && !isTypeC && !isTypeF) {
            // NOTE cannot == nor != on two string literals
            if (isStr(untyRet)) {
                msgs->errorExprCompareStringLits(codeLoc);
            } else {
                msgs->errorExprUntyBinBadOp(codeLoc, op);
            }
            return NodeVal();
        } else {
            switch (op) {
                case Token::O_ADD:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, +)
                    else ASSIGN_CALC_BASED_ON_TYPE_F(untyRet, untyL, untyR, +)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_SUB:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, -)
                    else ASSIGN_CALC_BASED_ON_TYPE_F(untyRet, untyL, untyR, -)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_SHL:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, <<)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_SHR:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, >>)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_BIT_AND:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, &)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_BIT_XOR:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, ^)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_BIT_OR:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, |)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_MUL:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, *)
                    else ASSIGN_CALC_BASED_ON_TYPE_F(untyRet, untyL, untyR, *)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_DIV:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, /)
                    else ASSIGN_CALC_BASED_ON_TYPE_F(untyRet, untyL, untyR, /)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_REM:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(untyRet, untyL, untyR, %)
                    else if (getTypeTable()->worksAsPrimitive(untyRet.type, TypeTable::P_F32)) {
                        untyRet.f32 = fmod(untyL.f32, untyR.f32);
                    } else if (getTypeTable()->worksAsPrimitive(untyRet.type, TypeTable::P_F64)) {
                        untyRet.f32 = fmod(untyL.f64, untyR.f64);
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_EQ:
                    untyRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(untyRet, untyL, untyR, ==)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(untyRet, untyL, untyR, ==)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(untyRet, untyL, untyR, ==)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_NEQ:
                    untyRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(untyRet, untyL, untyR, !=)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(untyRet, untyL, untyR, !=)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(untyRet, untyL, untyR, !=)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_LT:
                    untyRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(untyRet, untyL, untyR, <)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(untyRet, untyL, untyR, <)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(untyRet, untyL, untyR, <)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_LTEQ:
                    untyRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(untyRet, untyL, untyR, <=)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(untyRet, untyL, untyR, <=)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(untyRet, untyL, untyR, <=)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_GT:
                    untyRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(untyRet, untyL, untyR, >)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(untyRet, untyL, untyR, >)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(untyRet, untyL, untyR, >)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_GTEQ:
                    untyRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(untyRet, untyL, untyR, >=)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(untyRet, untyL, untyR, >=)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(untyRet, untyL, untyR, >=)
                    else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                    }
                    break;
                default:
                    msgs->errorExprUntyBinBadOp(codeLoc, op);
                    return NodeVal();
            }
        }
    }

    NodeVal ret(NodeVal::Kind::kUntyVal);
    ret.untyVal = untyRet;
    return ret;
}