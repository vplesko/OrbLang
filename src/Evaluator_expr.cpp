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

NodeVal Evaluator::evaluateKnownVal(const AstNode *ast) {
    NodeVal ret(NodeVal::Kind::kKnownVal);

    const LiteralVal &lit = ast->terminal.value().val;

    if (lit.kind == LiteralVal::Kind::kNone) {
        // should not happen
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    switch (lit.kind) {
    case LiteralVal::Kind::kBool:
        ret.knownVal.type = getPrimTypeId(TypeTable::P_BOOL);
        ret.knownVal.b = lit.val_b;
        break;
    case LiteralVal::Kind::kSint:
        {
            TypeTable::PrimIds fitting = getTypeTable()->shortestFittingPrimTypeI(lit.val_si);
            TypeTable::PrimIds chosen = max(TypeTable::P_I32, fitting);
            ret.knownVal.type = getPrimTypeId(chosen);
            if (chosen == TypeTable::P_I32) ret.knownVal.i32 = lit.val_si;
            else ret.knownVal.i64 = lit.val_si;
            break;
        }
    case LiteralVal::Kind::kChar:
        ret.knownVal.type = getPrimTypeId(TypeTable::P_C8);
        ret.knownVal.c8 = lit.val_c;
        break;
    case LiteralVal::Kind::kFloat:
        {
            TypeTable::PrimIds fitting = getTypeTable()->shortestFittingPrimTypeF(lit.val_f);
            TypeTable::PrimIds chosen = max(TypeTable::P_F32, fitting);
            ret.knownVal.type = getPrimTypeId(chosen);
            if (chosen == TypeTable::P_F32) ret.knownVal.f32 = lit.val_f;
            else ret.knownVal.f64 = lit.val_f;
            break;
        }
    case LiteralVal::Kind::kString:
        ret.knownVal.type = getTypeTable()->getTypeIdStr();
        ret.knownVal.str = lit.val_str;
        break;
    case LiteralVal::Kind::kNull:
        ret.knownVal.type = getPrimTypeId(TypeTable::P_PTR);
        break;
    default:
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    if (ast->hasType()) {
        optional<TypeTable::Id> ty = getType(ast->type.value().get(), true);
        if (!ty.has_value()) return NodeVal();

        if (ret.knownVal.type != ty.value()) {
            if (!KnownVal::isImplicitCastable(ret.knownVal, ty.value(), stringPool, getTypeTable())) {
                msgs->errorExprCannotPromote(ast->codeLoc, ty.value());
                return NodeVal();
            }

            if (!cast(ret.knownVal, ty.value())) {
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
    if (isGotoIssued() || !codegen->checkIsKnown(nodeVal->codeLoc, exprPay, true)) return NodeVal();

    return calculateOperUnary(ast->codeLoc, op, exprPay.knownVal);
}

NodeVal Evaluator::evaluateOper(CodeLoc codeLoc, Token::Oper op, const NodeVal &lhs, const NodeVal &rhs) {
    NodeVal exprPayL, exprPayR, exprPayRet;

    exprPayL = lhs;
    if (!codegen->checkIsKnown(codeLoc, exprPayL, true)) return NodeVal();

    exprPayR = rhs;
    if (!codegen->checkIsKnown(codeLoc, exprPayR, true)) return NodeVal();

    return calculateOper(codeLoc, op, exprPayL.knownVal, exprPayR.knownVal);
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
        if (isGotoIssued()) return NodeVal();

        for (size_t i = 2; i < ast->children.size(); ++i) {
            const AstNode *nodeRhs = ast->children[i].get();
            NodeVal rhsVal = evaluateNode(nodeRhs);
            if (isGotoIssued()) return NodeVal();

            lhsVal = evaluateOper(nodeRhs->codeLoc, first.oper, lhsVal, rhsVal);
            if (lhsVal.isInvalid()) return NodeVal();
        }

        return lhsVal;
    } else {
        const AstNode *nodeRhs = ast->children.back().get();
        NodeVal rhsVal = evaluateNode(nodeRhs);
        if (isGotoIssued()) return NodeVal();

        for (size_t i = ast->children.size()-2;; --i) {
            const AstNode *nodeLhs = ast->children[i].get();
            NodeVal lhsVal = evaluateNode(nodeLhs);
            if (isGotoIssued()) return NodeVal();

            rhsVal = evaluateOper(nodeLhs->codeLoc, first.oper, lhsVal, rhsVal);
            if (rhsVal.isInvalid()) return NodeVal();

            if (i == 1) break;
        }

        return rhsVal;
    }
}

NodeVal Evaluator::evaluateCast(const AstNode *ast) {
    if (!codegen->checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeType = ast->children[1].get();
    const AstNode *nodeVal = ast->children[2].get();

    optional<TypeTable::Id> valTypeId = getType(nodeType, true);
    if (!valTypeId.has_value()) return NodeVal();

    NodeVal exprVal = evaluateNode(nodeVal);
    if (isGotoIssued() || exprVal.isInvalid()) return NodeVal();

    return calculateCast(ast->codeLoc, exprVal.knownVal, valTypeId.value());
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

bool Evaluator::cast(KnownVal &val, TypeTable::Id t) const {
    if (val.type == t) return true;

    if (KnownVal::isI(val, getTypeTable())) {
        int64_t x = KnownVal::getValueI(val, getTypeTable()).value();
        ASSIGN_BASED_ON_TYPE_IU(val, x, t)
        else ASSIGN_BASED_ON_TYPE_F(val, x, t)
        else ASSIGN_BASED_ON_TYPE_CB(val, x, t)
        else return false;
    } else if (KnownVal::isU(val, getTypeTable())) {
        uint64_t x = KnownVal::getValueU(val, getTypeTable()).value();
        ASSIGN_BASED_ON_TYPE_IU(val, x, t)
        else ASSIGN_BASED_ON_TYPE_F(val, x, t)
        else ASSIGN_BASED_ON_TYPE_CB(val, x, t)
        else return false;
    } else if (KnownVal::isF(val, getTypeTable())) {
        double x = KnownVal::getValueF(val, getTypeTable()).value();
        ASSIGN_BASED_ON_TYPE_IU(val, x, t)
        else ASSIGN_BASED_ON_TYPE_F(val, x, t)
        else return false;
    } else if (KnownVal::isC(val, getTypeTable())) {
        char x = val.c8;
        ASSIGN_BASED_ON_TYPE_IU(val, x, t)
        else ASSIGN_BASED_ON_TYPE_CB(val, x, t)
        else return false;
    } else if (KnownVal::isB(val, getTypeTable())) {
        int8_t b = val.b ? 1 : 0;
        ASSIGN_BASED_ON_TYPE_IU(val, b, t)
        else return false;
    } else if (KnownVal::isNull(val, getTypeTable())) {
        ASSIGN_BASED_ON_TYPE_IU(val, 0, t)
        else if (getTypeTable()->worksAsPrimitive(t, TypeTable::P_BOOL)) {
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

NodeVal Evaluator::calculateOperUnary(CodeLoc codeLoc, Token::Oper op, KnownVal known) {
    NodeVal exprRet(NodeVal::Kind::kKnownVal);
    exprRet.knownVal.type = known.type;

    if (op == Token::O_ADD) {
        if (!KnownVal::isI(known, getTypeTable()) &&
            !KnownVal::isU(known, getTypeTable()) &&
            !KnownVal::isF(known, getTypeTable())) {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
        exprRet.knownVal = known;
    } else if (op == Token::O_SUB) {
        if (KnownVal::isI(known, getTypeTable())) {
            int64_t x = KnownVal::getValueI(known, getTypeTable()).value();
            x = -x;
            ASSIGN_BASED_ON_TYPE_IU(exprRet.knownVal, x, known.type);
        } else if (KnownVal::isF(known, getTypeTable())) {
            double x = KnownVal::getValueF(known, getTypeTable()).value();
            x = -x;
            ASSIGN_BASED_ON_TYPE_F(exprRet.knownVal, x, known.type);
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else if (op == Token::O_BIT_NOT) {
        if (KnownVal::isI(known, getTypeTable())) {
            int64_t x = KnownVal::getValueI(known, getTypeTable()).value();
            x = ~x;
            ASSIGN_BASED_ON_TYPE_IU(exprRet.knownVal, x, known.type);
        } else if (KnownVal::isU(known, getTypeTable())) {
            uint64_t x = KnownVal::getValueU(known, getTypeTable()).value();
            x = ~x;
            ASSIGN_BASED_ON_TYPE_IU(exprRet.knownVal, x, known.type);
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else if (op == Token::O_NOT) {
        if (KnownVal::isB(known, getTypeTable())) {
            exprRet.knownVal.b = !known.b;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return NodeVal();
        }
    } else {
        if (op == Token::O_MUL && KnownVal::isNull(known, getTypeTable())) {
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
NodeVal Evaluator::calculateOper(CodeLoc codeLoc, Token::Oper op, KnownVal knownL, KnownVal knownR) {
    if (knownL.type != knownR.type) {
        if (getTypeTable()->isImplicitCastable(knownR.type, knownL.type)) {
            if (!cast(knownR, knownL.type)) {
                msgs->errorInternal(codeLoc);
                return NodeVal();
            }
        } else if (getTypeTable()->isImplicitCastable(knownL.type, knownR.type)) {
            if (!cast(knownL, knownR.type)) {
                msgs->errorInternal(codeLoc);
                return NodeVal();
            }
        } else {
            msgs->errorExprCannotImplicitCastEither(codeLoc, knownL.type, knownR.type);
            return NodeVal();
        }
    }

    KnownVal knownRet(knownL.type);

    if (KnownVal::isB(knownRet, getTypeTable())) {
        switch (op) {
        case Token::O_EQ:
            knownRet.b = knownL.b == knownR.b;
            break;
        case Token::O_NEQ:
            knownRet.b = knownL.b != knownR.b;
            break;
        default:
            msgs->errorExprKnownBinBadOp(codeLoc, op);
            return NodeVal();
        }
    } else if (KnownVal::isNull(knownRet, getTypeTable())) {
        // both are null
        switch (op) {
        case Token::O_EQ:
            knownRet.type = getPrimTypeId(TypeTable::P_BOOL);
            knownRet.b = true;
            break;
        case Token::O_NEQ:
            knownRet.type = getPrimTypeId(TypeTable::P_BOOL);
            knownRet.b = false;
            break;
        default:
            msgs->errorExprKnownBinBadOp(codeLoc, op);
            return NodeVal();
        }
    } else {
        bool isTypeI = KnownVal::isI(knownRet, getTypeTable());
        bool isTypeU = KnownVal::isU(knownRet, getTypeTable());
        bool isTypeF = KnownVal::isF(knownRet, getTypeTable());
        bool isTypeC = KnownVal::isC(knownRet, getTypeTable());

        if (!isTypeI && !isTypeU && !isTypeC && !isTypeF) {
            // NOTE cannot == nor != on two string literals
            if (KnownVal::isStr(knownRet, getTypeTable())) {
                msgs->errorExprCompareStringLits(codeLoc);
            } else {
                msgs->errorExprKnownBinBadOp(codeLoc, op);
            }
            return NodeVal();
        } else {
            switch (op) {
                case Token::O_ADD:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, +)
                    else ASSIGN_CALC_BASED_ON_TYPE_F(knownRet, knownL, knownR, +)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_SUB:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, -)
                    else ASSIGN_CALC_BASED_ON_TYPE_F(knownRet, knownL, knownR, -)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_SHL:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, <<)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_SHR:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, >>)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_BIT_AND:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, &)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_BIT_XOR:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, ^)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_BIT_OR:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, |)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_MUL:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, *)
                    else ASSIGN_CALC_BASED_ON_TYPE_F(knownRet, knownL, knownR, *)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_DIV:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, /)
                    else ASSIGN_CALC_BASED_ON_TYPE_F(knownRet, knownL, knownR, /)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_REM:
                    ASSIGN_CALC_BASED_ON_TYPE_IU(knownRet, knownL, knownR, %)
                    else if (getTypeTable()->worksAsPrimitive(knownRet.type, TypeTable::P_F32)) {
                        knownRet.f32 = fmod(knownL.f32, knownR.f32);
                    } else if (getTypeTable()->worksAsPrimitive(knownRet.type, TypeTable::P_F64)) {
                        knownRet.f32 = fmod(knownL.f64, knownR.f64);
                    } else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_EQ:
                    knownRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(knownRet, knownL, knownR, ==)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(knownRet, knownL, knownR, ==)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(knownRet, knownL, knownR, ==)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_NEQ:
                    knownRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(knownRet, knownL, knownR, !=)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(knownRet, knownL, knownR, !=)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(knownRet, knownL, knownR, !=)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_LT:
                    knownRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(knownRet, knownL, knownR, <)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(knownRet, knownL, knownR, <)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(knownRet, knownL, knownR, <)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_LTEQ:
                    knownRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(knownRet, knownL, knownR, <=)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(knownRet, knownL, knownR, <=)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(knownRet, knownL, knownR, <=)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_GT:
                    knownRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(knownRet, knownL, knownR, >)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(knownRet, knownL, knownR, >)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(knownRet, knownL, knownR, >)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                case Token::O_GTEQ:
                    knownRet.type = getPrimTypeId(TypeTable::P_BOOL);
                    ASSIGN_COMP_BASED_ON_TYPE_IU(knownRet, knownL, knownR, >=)
                    else ASSIGN_COMP_BASED_ON_TYPE_F(knownRet, knownL, knownR, >=)
                    else ASSIGN_COMP_BASED_ON_TYPE_C(knownRet, knownL, knownR, >=)
                    else {
                        msgs->errorExprKnownBinBadOp(codeLoc, op);
                    }
                    break;
                default:
                    msgs->errorExprKnownBinBadOp(codeLoc, op);
                    return NodeVal();
            }
        }
    }

    NodeVal ret(NodeVal::Kind::kKnownVal);
    ret.knownVal = knownRet;
    return ret;
}

NodeVal Evaluator::calculateCast(CodeLoc codeLoc, KnownVal known, TypeTable::Id type) {
    if (!cast(known, type)) {
        msgs->errorExprCannotCast(codeLoc, known.type, type);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kKnownVal);
    ret.knownVal = known;
    return ret;
}