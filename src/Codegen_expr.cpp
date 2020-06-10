#include "Codegen.h"
#include "Evaluator.h"
using namespace std;

NodeVal Codegen::codegenExpr(const AstNode *ast, const NodeVal &first) {
    if (first.isFuncId()) {
        return codegenCall(ast, first);
    } else if (first.isOper()) {
        if (first.oper == Token::O_DOT) {
            return codegenOperDot(ast);
        } else if (first.oper == Token::O_IND) {
            return codegenOperInd(ast);
        } else if (ast->children.size() == 2) {
            return codegenOperUnary(ast, first);
        } else {
            return codegenOper(ast, first);
        }
    } else {
        return codegenTuple(ast, first);
    }
}

NodeVal Codegen::codegenVar(const AstNode *ast) {
    NamePool::Id name = ast->terminal.value().id;

    optional<SymbolTable::VarPayload> var = symbolTable->getVar(name);
    if (!var.has_value()) {
        msgs->errorVarNotFound(ast->codeLoc, name);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kLlvmVal);
    ret.llvmVal.type = var.value().type;
    ret.llvmVal.val = llvmBuilder.CreateLoad(var.value().val, namePool->get(name));
    ret.llvmVal.ref = var.value().val;
    return ret;
}

NodeVal Codegen::codegenOperInd(CodeLoc codeLoc, const NodeVal &base, const NodeVal &ind) {
    if (!checkValueUnbroken(codeLoc, base, true)) return NodeVal();

    NodeVal leftVal = base;

    if (leftVal.isKnownVal()) {
        if (!promoteKnownVal(leftVal)) {
            msgs->errorInternal(codeLoc);
            return NodeVal();
        }
        
        if (!getTypeTable()->worksAsTypeStr(leftVal.llvmVal.type)) {
            msgs->errorExprIndexOnBadType(codeLoc);
            return NodeVal();
        }
    }

    if (!checkValueUnbroken(codeLoc, ind, true)) return NodeVal();

    NodeVal rightVal = ind;

    if (rightVal.isKnownVal()) {
        if (evaluator->isI(rightVal.knownVal)) {
            int64_t ind = KnownVal::getValueI(rightVal.knownVal, getTypeTable()).value();
            if (getTypeTable()->worksAsTypeArr(leftVal.llvmVal.type)) {
                size_t len = getTypeTable()->extractLenOfArr(leftVal.llvmVal.type).value();
                if (ind < 0 || ind >= len) {
                    msgs->warnExprIndexOutOfBounds(codeLoc);
                }
            }
        } else if (evaluator->isU(rightVal.knownVal)) {
            uint64_t ind = KnownVal::getValueU(rightVal.knownVal, getTypeTable()).value();
            if (getTypeTable()->worksAsTypeArr(leftVal.llvmVal.type)) {
                size_t len = getTypeTable()->extractLenOfArr(leftVal.llvmVal.type).value();
                if (ind >= len) {
                    msgs->warnExprIndexOutOfBounds(codeLoc);
                }
            }
        } else {
            msgs->errorExprIndexNotIntegral(codeLoc);
            return NodeVal();
        }

        if (!promoteKnownVal(rightVal)) {
            msgs->errorInternal(codeLoc);
            return NodeVal();
        }
    }

    if (!getTypeTable()->worksAsTypeI(rightVal.llvmVal.type) && !getTypeTable()->worksAsTypeU(rightVal.llvmVal.type)) {
        msgs->errorExprIndexNotIntegral(codeLoc);
        return NodeVal();
    }

    optional<TypeTable::Id> typeId = symbolTable->getTypeTable()->addTypeIndexOf(leftVal.llvmVal.type);
    if (!typeId) {
        msgs->errorExprIndexOnBadType(codeLoc, leftVal.llvmVal.type);
        return NodeVal();
    }

    NodeVal retPay(NodeVal::Kind::kLlvmVal);
    retPay.llvmVal.type = typeId.value();

    if (symbolTable->getTypeTable()->worksAsTypeArrP(leftVal.llvmVal.type)) {
        retPay.llvmVal.ref = llvmBuilder.CreateGEP(leftVal.llvmVal.val, rightVal.llvmVal.val);
        retPay.llvmVal.val = llvmBuilder.CreateLoad(retPay.llvmVal.ref, "index_tmp");
    } else if (symbolTable->getTypeTable()->worksAsTypeArr(leftVal.llvmVal.type)) {
        if (leftVal.llvmVal.ref != nullptr) {
            retPay.llvmVal.ref = llvmBuilder.CreateGEP(leftVal.llvmVal.ref,
                {llvm::ConstantInt::get(getLlvmType(rightVal.llvmVal.type), 0), rightVal.llvmVal.val});
            retPay.llvmVal.val = llvmBuilder.CreateLoad(retPay.llvmVal.ref, "index_tmp");
        } else {
            // llvm's extractvalue requires compile-time known indices
            llvm::Value *tmp = createAlloca(getLlvmType(leftVal.llvmVal.type), "tmp");
            llvmBuilder.CreateStore(leftVal.llvmVal.val, tmp);
            tmp = llvmBuilder.CreateGEP(tmp,
                {llvm::ConstantInt::get(getLlvmType(rightVal.llvmVal.type), 0), rightVal.llvmVal.val});
            retPay.llvmVal.val = llvmBuilder.CreateLoad(tmp, "index_tmp");
        }
    } else {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    return retPay;
}

NodeVal Codegen::codegenOperInd(const AstNode *ast) {
    if (!checkAtLeastChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeBase = ast->children[1].get();
    NodeVal baseVal = codegenNode(nodeBase);

    for (size_t i = 2; i < ast->children.size(); ++i) {
        const AstNode *nodeInd = ast->children[i].get();

        baseVal = codegenOperInd(nodeInd->codeLoc, baseVal, codegenNode(nodeInd));
        if (baseVal.isInvalid()) return NodeVal();
    }

    return baseVal;
}

NodeVal Codegen::codegenOperDot(CodeLoc codeLoc, const NodeVal &base, const NodeVal &memb) {
    if (!checkValueUnbroken(codeLoc, base, false) || base.isKnownVal()) {
        msgs->errorExprDotInvalidBase(codeLoc);
        return NodeVal();
    }

    if (!checkValueUnbroken(codeLoc, memb, true)) return NodeVal();
    if (!checkIsKnown(codeLoc, memb, true)) return NodeVal();

    NodeVal leftVal = base;

    if (getTypeTable()->worksAsTypeP(leftVal.llvmVal.type)) {
        optional<TypeTable::Id> derefType = symbolTable->getTypeTable()->addTypeDerefOf(leftVal.llvmVal.type);
        if (!derefType.has_value()) {
            msgs->errorInternal(codeLoc);
            return NodeVal();
        }
        leftVal.llvmVal.type = derefType.value();
        leftVal.llvmVal.ref = leftVal.llvmVal.val;
        leftVal.llvmVal.val = llvmBuilder.CreateLoad(leftVal.llvmVal.val, "deref_tmp");
    }
    
    optional<const TypeTable::Tuple*> tupleOpt = getTypeTable()->extractTuple(leftVal.llvmVal.type);
    if (!tupleOpt.has_value()) {
        msgs->errorExprDotInvalidBase(codeLoc);
        return NodeVal();
    }
    const TypeTable::Tuple &tuple = *(tupleOpt.value());

    optional<size_t> membIndOpt = KnownVal::getValueNonNeg(memb.knownVal, getTypeTable());
    if (!membIndOpt.has_value() || membIndOpt.value() >= tuple.members.size()) {
        msgs->errorMemberIndex(codeLoc);
        return NodeVal();
    }
    size_t memberInd = membIndOpt.value();

    NodeVal exprRet(NodeVal::Kind::kLlvmVal);

    if (leftVal.llvmVal.ref != nullptr) {
        exprRet.llvmVal.ref = llvmBuilder.CreateStructGEP(leftVal.llvmVal.ref, memberInd);
        exprRet.llvmVal.val = llvmBuilder.CreateLoad(exprRet.llvmVal.ref, "dot_tmp");
    } else {
        llvm::Value *tmp = createAlloca(getLlvmType(leftVal.llvmVal.type), "tmp");
        llvmBuilder.CreateStore(leftVal.llvmVal.val, tmp);
        tmp = llvmBuilder.CreateStructGEP(tmp, memberInd);
        exprRet.llvmVal.val = llvmBuilder.CreateLoad(tmp, "dot_tmp");
    }

    exprRet.llvmVal.type = tuple.members[memberInd];
    if (getTypeTable()->isDirectCn(leftVal.llvmVal.type)) {
        exprRet.llvmVal.type = getTypeTable()->addTypeCnOf(exprRet.llvmVal.type);
    }

    return exprRet;
}

NodeVal Codegen::codegenOperDot(const AstNode *ast) {
    if (!checkAtLeastChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeBase = ast->children[1].get();
    NodeVal baseVal = codegenNode(nodeBase);

    for (size_t i = 2; i < ast->children.size(); ++i) {
        const AstNode *nodeMemb = ast->children[i].get();

        baseVal = codegenOperDot(nodeMemb->codeLoc, baseVal, evaluator->evaluateNode(nodeMemb));
        if (baseVal.isInvalid()) return NodeVal();
    }

    return baseVal;
}

NodeVal Codegen::codegenOperUnary(const AstNode *ast, const NodeVal &first) {
    if (!checkExactlyChildren(ast, 2, true)) {
        return NodeVal();
    }

    const AstNode *nodeVal = ast->children[1].get();

    Token::Oper op = first.oper;

    NodeVal exprPay = codegenNode(nodeVal);
    if (!checkValueUnbroken(nodeVal->codeLoc, exprPay, true)) return NodeVal();

    if (exprPay.isKnownVal()) return evaluator->calculateOperUnary(ast->codeLoc, op, exprPay.knownVal);

    NodeVal exprRet(NodeVal::Kind::kLlvmVal);
    exprRet.llvmVal.type = exprPay.llvmVal.type;
    if (op == Token::O_ADD) {
        if (!(getTypeTable()->worksAsTypeI(exprPay.llvmVal.type) ||
            getTypeTable()->worksAsTypeU(exprPay.llvmVal.type) ||
            getTypeTable()->worksAsTypeF(exprPay.llvmVal.type))) {
            msgs->errorExprUnBadType(ast->codeLoc, op, exprPay.llvmVal.type);
            return NodeVal();
        }
        exprRet.llvmVal.val = exprPay.llvmVal.val;
    } else if (op == Token::O_SUB) {
        if (getTypeTable()->worksAsTypeI(exprPay.llvmVal.type)) {
            exprRet.llvmVal.val = llvmBuilder.CreateNeg(exprPay.llvmVal.val, "sneg_tmp");
        } else if (getTypeTable()->worksAsTypeF(exprPay.llvmVal.type)) {
            exprRet.llvmVal.val = llvmBuilder.CreateFNeg(exprPay.llvmVal.val, "fneg_tmp");
        } else {
            msgs->errorExprUnBadType(ast->codeLoc, op, exprPay.llvmVal.type);
            return NodeVal();
        }
    } else if (op == Token::O_INC) {
        if (getTypeTable()->worksAsTypeCn(exprPay.llvmVal.type)) {
            msgs->errorExprUnOnCn(ast->codeLoc, op);
            return NodeVal();
        }
        if (!(getTypeTable()->worksAsTypeI(exprPay.llvmVal.type) ||
            getTypeTable()->worksAsTypeU(exprPay.llvmVal.type)) ||
            exprPay.llvmVal.refBroken()) {
            msgs->errorExprUnBadType(ast->codeLoc, op, exprPay.llvmVal.type);
            return NodeVal();
        }
        exprRet.llvmVal.val = llvmBuilder.CreateAdd(exprPay.llvmVal.val, llvm::ConstantInt::get(getLlvmType(exprPay.llvmVal.type), 1), "inc_tmp");
        exprRet.llvmVal.ref = exprPay.llvmVal.ref;
        llvmBuilder.CreateStore(exprRet.llvmVal.val, exprRet.llvmVal.ref);
    } else if (op == Token::O_DEC) {
        if (getTypeTable()->worksAsTypeCn(exprPay.llvmVal.type)) {
            msgs->errorExprUnOnCn(ast->codeLoc, op);
            return NodeVal();
        }
        if (!(getTypeTable()->worksAsTypeI(exprPay.llvmVal.type) ||
            getTypeTable()->worksAsTypeU(exprPay.llvmVal.type)) ||
            exprPay.llvmVal.refBroken()) {
            msgs->errorExprUnBadType(ast->codeLoc, op, exprPay.llvmVal.type);
            return NodeVal();
        }
        exprRet.llvmVal.val = llvmBuilder.CreateSub(exprPay.llvmVal.val, llvm::ConstantInt::get(getLlvmType(exprPay.llvmVal.type), 1), "dec_tmp");
        exprRet.llvmVal.ref = exprPay.llvmVal.ref;
        llvmBuilder.CreateStore(exprRet.llvmVal.val, exprRet.llvmVal.ref);
    } else if (op == Token::O_BIT_NOT) {
        if (!(getTypeTable()->worksAsTypeI(exprPay.llvmVal.type) || getTypeTable()->worksAsTypeU(exprPay.llvmVal.type))) {
            msgs->errorExprUnBadType(ast->codeLoc, op, exprPay.llvmVal.type);
            return NodeVal();
        }
        exprRet.llvmVal.val = llvmBuilder.CreateNot(exprPay.llvmVal.val, "bit_not_tmp");
    } else if (op == Token::O_NOT) {
        if (!getTypeTable()->worksAsTypeB(exprPay.llvmVal.type)) {
            msgs->errorExprUnBadType(ast->codeLoc, op, exprPay.llvmVal.type);
            return NodeVal();
        }
        exprRet.llvmVal.val = llvmBuilder.CreateNot(exprPay.llvmVal.val, "not_tmp");
    } else if (op == Token::O_MUL) {
        optional<TypeTable::Id> typeId = symbolTable->getTypeTable()->addTypeDerefOf(exprPay.llvmVal.type);
        if (!typeId) {
            msgs->errorExprDerefOnBadType(ast->codeLoc, exprPay.llvmVal.type);
            return NodeVal();
        }
        exprRet.llvmVal.type = typeId.value();
        exprRet.llvmVal.val = llvmBuilder.CreateLoad(exprPay.llvmVal.val, "deref_tmp");
        exprRet.llvmVal.ref = exprPay.llvmVal.val;
    } else if (op == Token::O_BIT_AND) {
        if (exprPay.llvmVal.ref == nullptr) {
            msgs->errorExprAddressOfNoRef(ast->codeLoc);
            return NodeVal();
        }
        TypeTable::Id typeId = symbolTable->getTypeTable()->addTypeAddrOf(exprPay.llvmVal.type);
        exprRet.llvmVal.type = typeId;
        exprRet.llvmVal.val = exprPay.llvmVal.ref;
    } else {
        msgs->errorNonUnOp(ast->codeLoc, op);
        return NodeVal();
    }
    return exprRet;
}

NodeVal Codegen::codegenOper(CodeLoc codeLoc, Token::Oper op, const NodeVal &lhs, const NodeVal &rhs) {
    NodeVal exprPayL, exprPayR, exprPayRet;

    bool assignment = operInfos.at(op).assignment;

    exprPayL = lhs;
    if (!checkValueUnbroken(codeLoc, exprPayL, true)) return NodeVal();

    if (assignment) {
        if (!exprPayL.isLlvmVal() || exprPayL.llvmVal.refBroken()) {
            msgs->errorExprAsgnNonRef(codeLoc, op);
            return NodeVal();
        }
        if (getTypeTable()->worksAsTypeCn(exprPayL.llvmVal.type)) {
            msgs->errorExprAsgnOnCn(codeLoc, op);
            return NodeVal();
        }
    }

    exprPayR = rhs;
    if (!checkValueUnbroken(codeLoc, exprPayR, true)) return NodeVal();

    if (exprPayL.isKnownVal() && !exprPayR.isKnownVal()) {
        if (!promoteKnownVal(exprPayL, exprPayR.llvmVal.type)) {
            msgs->errorExprCannotPromote(codeLoc, exprPayR.llvmVal.type);
            return NodeVal();
        }
    } else if (!exprPayL.isKnownVal() && exprPayR.isKnownVal()) {
        if (!promoteKnownVal(exprPayR, exprPayL.llvmVal.type)) {
            msgs->errorExprCannotPromote(codeLoc, exprPayL.llvmVal.type);
            return NodeVal();
        }
    } else if (exprPayL.isKnownVal() && exprPayR.isKnownVal()) {
        return evaluator->calculateOper(codeLoc, op, exprPayL.knownVal, exprPayR.knownVal);
    }

    exprPayRet = NodeVal(NodeVal::Kind::kLlvmVal);

    llvm::Value *valL = exprPayL.llvmVal.val, *valR = exprPayR.llvmVal.val;
    exprPayRet.llvmVal.type = exprPayL.llvmVal.type;
    exprPayRet.llvmVal.val = nullptr;
    exprPayRet.llvmVal.ref = assignment ? exprPayL.llvmVal.ref : nullptr;

    if (exprPayL.llvmVal.type != exprPayR.llvmVal.type) {
        if (getTypeTable()->isImplicitCastable(exprPayR.llvmVal.type, exprPayL.llvmVal.type)) {
            createCast(valR, exprPayR.llvmVal.type, exprPayL.llvmVal.type);
            exprPayRet.llvmVal.type = exprPayL.llvmVal.type;
        } else if (getTypeTable()->isImplicitCastable(exprPayL.llvmVal.type, exprPayR.llvmVal.type) && !assignment) {
            createCast(valL, exprPayL.llvmVal.type, exprPayR.llvmVal.type);
            exprPayRet.llvmVal.type = exprPayR.llvmVal.type;
        } else {
            msgs->errorExprCannotImplicitCastEither(codeLoc, exprPayL.llvmVal.type, exprPayR.llvmVal.type);
            return NodeVal();
        }
    }

    if (getTypeTable()->worksAsTypeB(exprPayRet.llvmVal.type)) {
        switch (op) {
        case Token::O_ASGN:
            exprPayRet.llvmVal.val = valR;
            break;
        case Token::O_EQ:
            exprPayRet.llvmVal.val = llvmBuilder.CreateICmpEQ(valL, valR, "bcmp_eq_tmp");
            break;
        case Token::O_NEQ:
            exprPayRet.llvmVal.val = llvmBuilder.CreateICmpNE(valL, valR, "bcmp_neq_tmp");
            break;
        default:
            break;
        }
    } else {
        bool isTypeI = getTypeTable()->worksAsTypeI(exprPayRet.llvmVal.type);
        bool isTypeU = getTypeTable()->worksAsTypeU(exprPayRet.llvmVal.type);
        bool isTypeF = getTypeTable()->worksAsTypeF(exprPayRet.llvmVal.type);
        bool isTypeC = getTypeTable()->worksAsTypeC(exprPayRet.llvmVal.type);
        bool isTypeP = symbolTable->getTypeTable()->worksAsTypeAnyP(exprPayRet.llvmVal.type);

        switch (op) {
            case Token::O_ASGN:
                exprPayRet.llvmVal.val = valR;
                break;
            case Token::O_ADD:
            case Token::O_ADD_ASGN:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFAdd(valL, valR, "fadd_tmp");
                else if (isTypeI || isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateAdd(valL, valR, "add_tmp");
                break;
            case Token::O_SUB:
            case Token::O_SUB_ASGN:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFSub(valL, valR, "fsub_tmp");
                else if (isTypeI || isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateSub(valL, valR, "sub_tmp");
                break;
            case Token::O_SHL:
            case Token::O_SHL_ASGN:
                if (isTypeI || isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateShl(valL, valR, "shl_tmp");
                break;
            case Token::O_SHR:
            case Token::O_SHR_ASGN:
                if (isTypeI)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateAShr(valL, valR, "ashr_tmp");
                else if (isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateLShr(valL, valR, "lshr_tmp");
                break;
            case Token::O_BIT_AND:
            case Token::O_BIT_AND_ASGN:
                if (isTypeI || isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateAnd(valL, valR, "and_tmp");
                break;
            case Token::O_BIT_XOR:
            case Token::O_BIT_XOR_ASGN:
                if (isTypeI || isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateXor(valL, valR, "xor_tmp");
                break;
            case Token::O_BIT_OR:
            case Token::O_BIT_OR_ASGN:
                if (isTypeI || isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateOr(valL, valR, "or_tmp");
                break;
            case Token::O_MUL:
            case Token::O_MUL_ASGN:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFMul(valL, valR, "fmul_tmp");
                else if (isTypeI || isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateMul(valL, valR, "mul_tmp");
                break;
            case Token::O_DIV:
            case Token::O_DIV_ASGN:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFDiv(valL, valR, "fdiv_tmp");
                else if (isTypeI)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateSDiv(valL, valR, "sdiv_tmp");
                else if (isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateUDiv(valL, valR, "udiv_tmp");
                break;
            case Token::O_REM:
            case Token::O_REM_ASGN:
                if (isTypeI)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateSRem(valL, valR, "srem_tmp");
                else if (isTypeU)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateURem(valL, valR, "urem_tmp");
                else if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFRem(valL, valR, "frem_tmp");
                break;
            case Token::O_EQ:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFCmpOEQ(valL, valR, "fcmp_eq_tmp");
                else if (isTypeI || isTypeU || isTypeC)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpEQ(valL, valR, "cmp_eq_tmp");
                else if (isTypeP) {
                    exprPayRet.llvmVal.val =
                        llvmBuilder.CreateICmpEQ(llvmBuilder.CreatePtrToInt(valL, getLlvmType(getPrimTypeId(TypeTable::WIDEST_I))),
                        llvmBuilder.CreatePtrToInt(valR, getLlvmType(getPrimTypeId(TypeTable::WIDEST_I))), "pcmp_eq_tmp");
                }
                exprPayRet.llvmVal.type = getPrimTypeId(TypeTable::P_BOOL);
                break;
            case Token::O_NEQ:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFCmpONE(valL, valR, "fcmp_neq_tmp");
                else if (isTypeI || isTypeU || isTypeC)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpNE(valL, valR, "cmp_neq_tmp");
                else if (isTypeP) {
                    exprPayRet.llvmVal.val =
                        llvmBuilder.CreateICmpNE(llvmBuilder.CreatePtrToInt(valL, getLlvmType(getPrimTypeId(TypeTable::WIDEST_I))),
                        llvmBuilder.CreatePtrToInt(valR, getLlvmType(getPrimTypeId(TypeTable::WIDEST_I))), "pcmp_eq_tmp");
                }
                exprPayRet.llvmVal.type = getPrimTypeId(TypeTable::P_BOOL);
                break;
            case Token::O_LT:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFCmpOLT(valL, valR, "fcmp_lt_tmp");
                else if (isTypeI)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpSLT(valL, valR, "scmp_lt_tmp");
                else if (isTypeU || isTypeC)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpULT(valL, valR, "ucmp_lt_tmp");
                exprPayRet.llvmVal.type = getPrimTypeId(TypeTable::P_BOOL);
                break;
            case Token::O_LTEQ:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFCmpOLE(valL, valR, "fcmp_lteq_tmp");
                else if (isTypeI)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpSLE(valL, valR, "scmp_lteq_tmp");
                else if (isTypeU || isTypeC)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpULE(valL, valR, "ucmp_lteq_tmp");
                exprPayRet.llvmVal.type = getPrimTypeId(TypeTable::P_BOOL);
                break;
            case Token::O_GT:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFCmpOGT(valL, valR, "fcmp_gt_tmp");
                else if (isTypeI)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpSGT(valL, valR, "scmp_gt_tmp");
                else if (isTypeU || isTypeC)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpUGT(valL, valR, "ucmp_gt_tmp");
                exprPayRet.llvmVal.type = getPrimTypeId(TypeTable::P_BOOL);
                break;
            case Token::O_GTEQ:
                if (isTypeF)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateFCmpOGE(valL, valR, "fcmp_gteq_tmp");
                else if (isTypeI)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpSGE(valL, valR, "scmp_gteq_tmp");
                else if (isTypeU || isTypeC)
                    exprPayRet.llvmVal.val = llvmBuilder.CreateICmpUGE(valL, valR, "ucmp_gteq_tmp");
                exprPayRet.llvmVal.type = getPrimTypeId(TypeTable::P_BOOL);
                break;
            default:
                msgs->errorNonBinOp(codeLoc, op);
                return NodeVal();
        }
    }

    if (exprPayRet.valueBroken()) {
        msgs->errorInternal(codeLoc);
        return NodeVal();
    }

    if (assignment) {
        llvmBuilder.CreateStore(exprPayRet.llvmVal.val, exprPayRet.llvmVal.ref);
    }

    return exprPayRet;
}

NodeVal Codegen::codegenOper(const AstNode *ast, const NodeVal &first) {
    OperInfo opInfo = operInfos.at(first.oper);

    if (opInfo.variadic ?
        !checkAtLeastChildren(ast, 3, true) :
        !checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    if (opInfo.l_assoc) {
        const AstNode *nodeLhs = ast->children[1].get();
        NodeVal lhsVal = codegenNode(nodeLhs);

        for (size_t i = 2; i < ast->children.size(); ++i) {
            const AstNode *nodeRhs = ast->children[i].get();

            lhsVal = codegenOper(nodeRhs->codeLoc, first.oper, lhsVal, codegenNode(nodeRhs));
            if (lhsVal.isInvalid()) return NodeVal();
        }

        return lhsVal;
    } else {
        const AstNode *nodeRhs = ast->children.back().get();
        NodeVal rhsVal = codegenNode(nodeRhs);

        for (size_t i = ast->children.size()-2;; --i) {
            const AstNode *nodeLhs = ast->children[i].get();

            rhsVal = codegenOper(nodeLhs->codeLoc, first.oper, codegenNode(nodeLhs), rhsVal);
            if (rhsVal.isInvalid()) return NodeVal();

            if (i == 1) break;
        }

        return rhsVal;
    }
}

NodeVal Codegen::codegenTuple(const AstNode *ast, const NodeVal &first) {
    if (ast->children.size() == 1) {
        if (!checkValueUnbroken(ast->codeLoc, first, true)) return NodeVal();
        
        return first;
    }

    TypeTable::Tuple tup;
    tup.members.resize(ast->children.size());

    vector<llvm::Value*> llvmMembs(ast->children.size());

    for (size_t i = 0; i < ast->children.size(); ++i) {
        const AstNode *nodeMemb = ast->children[i].get();

        NodeVal nodeValMemb = i == 0 ? first : codegenNode(nodeMemb);
        if (!checkValueUnbroken(nodeMemb->codeLoc, nodeValMemb, true)) return NodeVal();

        if (nodeValMemb.isKnownVal()) {
            promoteKnownVal(nodeValMemb);
        }
        
        tup.members[i] = nodeValMemb.llvmVal.type;
        llvmMembs[i] = nodeValMemb.llvmVal.val;
    }

    optional<TypeTable::Id> tupTyOpt = getTypeTable()->addTuple(move(tup));
    if (!tupTyOpt.has_value()) {
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }
    TypeTable::Id tupTy = tupTyOpt.value();

    llvm::StructType *tupLlvmTy = (llvm::StructType*) getLlvmType(tupTy);
    if (tupLlvmTy == nullptr) {
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }
    
    llvm::Value *tupLlvmRef = createAlloca(tupLlvmTy, "tup");
    for (size_t i = 0; i < llvmMembs.size(); ++i) {
        llvmBuilder.CreateStore(llvmMembs[i], llvmBuilder.CreateStructGEP(tupLlvmRef, i));
    }
    llvm::Value *tupLlvmVal = llvmBuilder.CreateLoad(tupLlvmRef, "tmp_tup");

    NodeVal ret(NodeVal::Kind::kLlvmVal);
    ret.llvmVal.type = tupTy;
    ret.llvmVal.val = tupLlvmVal;
    return ret;
}

NodeVal Codegen::codegenCall(const AstNode *ast, const NodeVal &first) {
    size_t argCnt = ast->children.size()-1;

    NamePool::Id funcName = first.id;

    FuncCallSite call(argCnt);
    call.name = funcName;

    vector<llvm::Value*> args(argCnt);
    vector<NodeVal> exprs(args.size());

    for (size_t i = 0; i < argCnt; ++i) {
        const AstNode *nodeArg = ast->children[i+1].get();

        exprs[i] = codegenNode(nodeArg);
        if (!checkValueUnbroken(nodeArg->codeLoc, exprs[i], true)) return NodeVal();

        if (exprs[i].isLlvmVal()) {
            call.set(i, exprs[i].llvmVal.type);
            args[i] = exprs[i].llvmVal.val;
        } else {
            call.set(i, exprs[i].knownVal);
        }
    }

    SymbolTable::FuncForCallPayload funcFind = symbolTable->getFuncForCall(call);
    if (funcFind.res == SymbolTable::FuncForCallPayload::kNotFound) {
        msgs->errorFuncNotFound(ast->children[0].get()->codeLoc, funcName);
        return NodeVal();
    } else if (funcFind.res == SymbolTable::FuncForCallPayload::kAmbigious) {
        msgs->errorFuncAmbigious(ast->children[0].get()->codeLoc, funcName);
        return NodeVal();
    }
    const FuncValue &func = funcFind.funcVal.value();

    for (size_t i = 0; i < argCnt; ++i) {
        const AstNode *nodeArg = ast->children[i+1].get();

        if (i >= func.argTypes.size()) {
            // variadic arguments
            if (exprs[i].isKnownVal()) {
                if (!promoteKnownVal(exprs[i])) {
                    msgs->errorInternal(nodeArg->codeLoc);
                    return NodeVal();
                }
                args[i] = exprs[i].llvmVal.val;
            }
        } else {
            if (exprs[i].isKnownVal()) {
                // this also checks whether sint/uint literals fit into the arg type size
                if (!promoteKnownVal(exprs[i], func.argTypes[i])) {
                    msgs->errorExprCannotPromote(nodeArg->codeLoc, func.argTypes[i]);
                    return NodeVal();
                }
                args[i] = exprs[i].llvmVal.val;
            } else if (call.argTypes[i] != func.argTypes[i]) {
                if (!createCast(args[i], call.argTypes[i], func.argTypes[i])) {
                    msgs->errorExprCannotImplicitCast(nodeArg->codeLoc, call.argTypes[i], func.argTypes[i]);
                    return NodeVal();
                }
            }
        }
    }

    NodeVal ret;
    if (func.hasRet()) {
        ret = NodeVal(NodeVal::Kind::kLlvmVal);
        ret.llvmVal.type = func.retType.value();
        ret.llvmVal.val = llvmBuilder.CreateCall(func.func, args, "call_tmp");
    } else {
        ret = NodeVal(NodeVal::Kind::kEmpty);
        llvmBuilder.CreateCall(func.func, args, "");
    }
    return ret;
}

NodeVal Codegen::codegenCast(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeType = ast->children[1].get();
    const AstNode *nodeVal = ast->children[2].get();

    optional<TypeTable::Id> valTypeId = evaluator->getType(nodeType, true);
    if (!valTypeId.has_value()) return NodeVal();

    llvm::Type *type = getLlvmType(valTypeId.value());
    if (type == nullptr) return NodeVal();

    NodeVal exprVal = codegenNode(nodeVal);
    if (!checkValueUnbroken(nodeVal->codeLoc, exprVal, true)) return NodeVal();

    if (exprVal.isKnownVal()) {
        if (!promoteKnownVal(exprVal)) {
            msgs->errorInternal(ast->codeLoc);
            return NodeVal();
        }
    }
    
    llvm::Value *val = exprVal.llvmVal.val;
    if (exprVal.llvmVal.type != valTypeId.value()) {
        if (!createCast(val, exprVal.llvmVal.type, type, valTypeId.value())) {
            msgs->errorExprCannotCast(nodeVal->codeLoc, exprVal.llvmVal.type, valTypeId.value());
            return NodeVal();
        }
    }

    NodeVal ret(NodeVal::Kind::kLlvmVal);
    ret.llvmVal.type = valTypeId.value();
    ret.llvmVal.val = val;
    return ret;
}

NodeVal Codegen::codegenArr(const AstNode *ast) {
    if (!checkAtLeastChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeArrType = ast->children[1].get();

    size_t arrLen = ast->children.size()-2;

    NodeVal arrDeclTypeId = codegenNode(nodeArrType);
    if (!checkIsType(nodeArrType->codeLoc, arrDeclTypeId, true)) return NodeVal();

    if (!getTypeTable()->worksAsTypeArrP(arrDeclTypeId.type) &&
        !getTypeTable()->worksAsTypeArrOfLen(arrDeclTypeId.type, arrLen)) {
        msgs->errorExprIndexOnBadType(ast->codeLoc, arrDeclTypeId.type);
        return NodeVal();
    }

    optional<TypeTable::Id> elemTypeIdFind = getTypeTable()->addTypeIndexOf(arrDeclTypeId.type);
    if (!elemTypeIdFind) {
        msgs->errorExprIndexOnBadType(ast->codeLoc, arrDeclTypeId.type);
        return NodeVal();
    }

    TypeTable::Id elemTypeId = elemTypeIdFind.value();

    llvm::Type *elemType = getLlvmType(elemTypeId);
    if (elemType == nullptr) {
        msgs->errorInternal(nodeArrType->codeLoc);
        return NodeVal();
    }

    TypeTable::Id arrTypeId = getTypeTable()->addTypeArrOfLenIdOf(
        elemTypeId, arrLen);
    
    llvm::Value *arrRef = createAlloca(getLlvmType(arrTypeId), "tmp_arr");

    for (size_t i = 0; i < arrLen; ++i) {
        const AstNode *nodeElem = ast->children[i+2].get();

        NodeVal exprPay = codegenConversion(nodeElem, elemTypeId);
        if (exprPay.isInvalid()) return NodeVal();

        llvm::Value *elemRef = llvmBuilder.CreateGEP(arrRef, 
            {llvm::ConstantInt::get(getLlvmType(getPrimTypeId(TypeTable::WIDEST_I)), 0),
            llvm::ConstantInt::get(getLlvmType(getPrimTypeId(TypeTable::WIDEST_I)), i)});
        llvmBuilder.CreateStore(exprPay.llvmVal.val, elemRef);
    }

    llvm::Value *arrVal = llvmBuilder.CreateLoad(arrRef);

    NodeVal retPay(NodeVal::Kind::kLlvmVal);
    retPay.llvmVal.type = arrTypeId;
    retPay.llvmVal.val = arrVal;
    return retPay;
}