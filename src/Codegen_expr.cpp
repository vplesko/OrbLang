#include "Codegen.h"
using namespace std;

NodeVal Codegen::codegenExpr(const AstNode *ast) {
    optional<Token::Type> keyw = getStartingKeyword(ast);

    if (getUntypedVal(ast, false).has_value()) {
        return codegenUntypedVal(ast);
    } else if (getId(ast, false).has_value()) {
        return codegenVar(ast);
    } else if (keyw.has_value()) {
        optional<Token::Type> keyw = getStartingKeyword(ast);

        switch (keyw.value()) {
        case Token::T_CAST:
            return codegenCast(ast);
        case Token::T_ARR:
            return codegenArr(ast);
        default:
            msgs->errorInternal(ast->codeLoc);
            return NodeVal();
        }
    } else if (checkNotTerminal(ast, false)) {
        if (getId(ast->children[0].get(), false).has_value()) {
            return codegenCall(ast);
        } else if (getOper(ast->children[0].get(), false).has_value()) {
            if (ast->children.size() == 2) {
                return codegenUn(ast);
            } else if (ast->children.size() == 3) {
                return codegenBin(ast);
            }
        }
    }
    
    msgs->errorUnknown(ast->codeLoc);
    return NodeVal();
}

NodeVal Codegen::codegenUntypedVal(const AstNode *ast) {
    optional<UntypedVal> val = getUntypedVal(ast, true);
    if (!val.has_value()) return NodeVal();

    if (val.value().kind == UntypedVal::Kind::kNone) {
        // should not happen
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kUntyVal);
    ret.untyVal = val.value();
    return ret;
}

NodeVal Codegen::codegenVar(const AstNode *ast) {
    optional<NamePool::Id> name = getId(ast, true);
    if (!name.has_value()) return NodeVal();

    optional<SymbolTable::VarPayload> var = symbolTable->getVar(name.value());
    if (!var.has_value()) {
        msgs->errorVarNotFound(ast->codeLoc, name.value());
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kLlvmVal);
    ret.llvmVal.type = var.value().type;
    ret.llvmVal.val = llvmBuilder.CreateLoad(var.value().val, namePool->get(name.value()));
    ret.llvmVal.ref = var.value().val;
    return ret;
}

NodeVal Codegen::codegenInd(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeBase = ast->children[1].get();
    const AstNode *nodeInd = ast->children[2].get();

    NodeVal baseExprPay = codegenExpr(nodeBase);
    if (!checkValueUnbroken(nodeBase->codeLoc, baseExprPay, true)) return NodeVal();

    if (baseExprPay.isUntyVal()) {
        if (baseExprPay.untyVal.kind == UntypedVal::Kind::kString) {
            if (!promoteUntyped(baseExprPay, getTypeTable()->getTypeIdStr())) {
                msgs->errorInternal(nodeBase->codeLoc);
                return NodeVal();
            }
        } else {
            msgs->errorExprIndexOnBadType(ast->codeLoc);
            return NodeVal();
        }
    }

    NodeVal indExprPay = codegenExpr(nodeInd);
    if (!checkValueUnbroken(nodeInd->codeLoc, indExprPay, true)) return NodeVal();

    if (indExprPay.isUntyVal()) {
        if (indExprPay.untyVal.kind == UntypedVal::Kind::kSint) {
            int64_t untyInd = indExprPay.untyVal.val_si;
            if (!promoteUntyped(indExprPay, getTypeTable()->shortestFittingTypeIId(untyInd))) {
                msgs->errorInternal(nodeInd->codeLoc);
                return NodeVal();
            }
            if (getTypeTable()->worksAsTypeArr(baseExprPay.llvmVal.type)) {
                size_t len = getTypeTable()->extractLenOfArr(baseExprPay.llvmVal.type).value();
                if (untyInd < 0 || ((size_t) untyInd) >= len) {
                    msgs->warnExprIndexOutOfBounds(nodeInd->codeLoc);
                }
            }
        } else {
            msgs->errorExprIndexNotIntegral(nodeInd->codeLoc);
            return NodeVal();
        }
    }

    if (!getTypeTable()->worksAsTypeI(indExprPay.llvmVal.type) && !getTypeTable()->worksAsTypeU(indExprPay.llvmVal.type)) {
        msgs->errorExprIndexNotIntegral(nodeInd->codeLoc);
        return NodeVal();
    }

    optional<TypeTable::Id> typeId = symbolTable->getTypeTable()->addTypeIndexOf(baseExprPay.llvmVal.type);
    if (!typeId) {
        msgs->errorExprIndexOnBadType(ast->codeLoc, baseExprPay.llvmVal.type);
        return NodeVal();
    }

    NodeVal retPay(NodeVal::Kind::kLlvmVal);
    retPay.llvmVal.type = typeId.value();

    if (symbolTable->getTypeTable()->worksAsTypeArrP(baseExprPay.llvmVal.type)) {
        retPay.llvmVal.ref = llvmBuilder.CreateGEP(baseExprPay.llvmVal.val, indExprPay.llvmVal.val);
        retPay.llvmVal.val = llvmBuilder.CreateLoad(retPay.llvmVal.ref, "index_tmp");
    } else if (symbolTable->getTypeTable()->worksAsTypeArr(baseExprPay.llvmVal.type)) {
        if (baseExprPay.llvmVal.ref != nullptr) {
            retPay.llvmVal.ref = llvmBuilder.CreateGEP(baseExprPay.llvmVal.ref,
                {llvm::ConstantInt::get(getType(indExprPay.llvmVal.type), 0), indExprPay.llvmVal.val});
            retPay.llvmVal.val = llvmBuilder.CreateLoad(retPay.llvmVal.ref, "index_tmp");
        } else {
            // llvm's extractvalue requires compile-time known indices
            llvm::Value *tmp = createAlloca(getType(baseExprPay.llvmVal.type), "tmp");
            llvmBuilder.CreateStore(baseExprPay.llvmVal.val, tmp);
            tmp = llvmBuilder.CreateGEP(tmp,
                {llvm::ConstantInt::get(getType(indExprPay.llvmVal.type), 0), indExprPay.llvmVal.val});
            retPay.llvmVal.val = llvmBuilder.CreateLoad(tmp, "index_tmp");
        }
    } else {
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    return retPay;
}

NodeVal Codegen::codegenDot(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeBase = ast->children[1].get();
    const AstNode *nodeMemb = ast->children[2].get();

    optional<NamePool::Id> membName = getId(nodeMemb, true);
    if (!membName.has_value()) return NodeVal();

    NodeVal baseExpr = codegenExpr(nodeBase);
    if (!checkValueUnbroken(nodeBase->codeLoc, baseExpr, true)) return NodeVal();
    if (baseExpr.isUntyVal()) {
        msgs->errorExprDotInvalidBase(nodeBase->codeLoc);
        return NodeVal();
    }

    NodeVal leftExpr = baseExpr;

    if (getTypeTable()->worksAsTypeP(leftExpr.llvmVal.type)) {
        optional<TypeTable::Id> derefType = symbolTable->getTypeTable()->addTypeDerefOf(leftExpr.llvmVal.type);
        if (!derefType.has_value()) {
            msgs->errorInternal(nodeBase->codeLoc);
            return NodeVal();
        }
        leftExpr.llvmVal.type = derefType.value();
        leftExpr.llvmVal.ref = leftExpr.llvmVal.val;
        leftExpr.llvmVal.val = llvmBuilder.CreateLoad(leftExpr.llvmVal.val, "deref_tmp");
    }
    
    optional<const TypeTable::DataType*> dataTypeOpt = getTypeTable()->extractDataType(leftExpr.llvmVal.type);
    if (!dataTypeOpt.has_value()) {
        msgs->errorExprDotInvalidBase(nodeBase->codeLoc);
        return NodeVal();
    }
    const TypeTable::DataType &dataType = *(dataTypeOpt.value());

    optional<size_t> memberIndOpt;
    for (size_t i = 0; i < dataType.members.size(); ++i) {
        if (dataType.members[i].name == membName.value()) {
            memberIndOpt = i;
            break;
        }
    }
    if (!memberIndOpt.has_value()) {
        msgs->errorDataUnknownMember(nodeMemb->codeLoc, membName.value());
        return NodeVal();
    }
    size_t memberInd = memberIndOpt.value();

    NodeVal exprRet(NodeVal::Kind::kLlvmVal);

    if (leftExpr.llvmVal.ref != nullptr) {
        exprRet.llvmVal.ref = llvmBuilder.CreateStructGEP(leftExpr.llvmVal.ref, memberInd);
        exprRet.llvmVal.val = llvmBuilder.CreateLoad(exprRet.llvmVal.ref, "dot_tmp");
    } else {
        llvm::Value *tmp = createAlloca(getType(leftExpr.llvmVal.type), "tmp");
        llvmBuilder.CreateStore(leftExpr.llvmVal.val, tmp);
        tmp = llvmBuilder.CreateStructGEP(tmp, memberInd);
        exprRet.llvmVal.val = llvmBuilder.CreateLoad(tmp, "dot_tmp");
    }

    exprRet.llvmVal.type = dataType.members[memberInd].type;
    if (getTypeTable()->worksAsTypeCn(leftExpr.llvmVal.type)) {
        exprRet.llvmVal.type = getTypeTable()->addTypeCnOf(exprRet.llvmVal.type);
    }

    return exprRet;
}

NodeVal Codegen::codegenUn(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 2, true)) {
        return NodeVal();
    }

    const AstNode *nodeOp = ast->children[0].get();
    const AstNode *nodeVal = ast->children[1].get();

    optional<Token::Oper> optOp = getOper(nodeOp, true);
    if (!optOp.has_value()) return NodeVal();
    Token::Oper op = optOp.value();

    NodeVal exprPay = codegenExpr(nodeVal);
    if (!checkValueUnbroken(nodeVal->codeLoc, exprPay, true)) return NodeVal();

    if (exprPay.isUntyVal()) return codegenUntypedUn(ast->codeLoc, op, exprPay.untyVal);

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
        exprRet.llvmVal.val = llvmBuilder.CreateAdd(exprPay.llvmVal.val, llvm::ConstantInt::get(getType(exprPay.llvmVal.type), 1), "inc_tmp");
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
        exprRet.llvmVal.val = llvmBuilder.CreateSub(exprPay.llvmVal.val, llvm::ConstantInt::get(getType(exprPay.llvmVal.type), 1), "dec_tmp");
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
        if (!typeId || !getTypeTable()->isNonOpaqueType(typeId.value())) {
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
        // should not happen
        msgs->errorUnknown(ast->codeLoc);
        return NodeVal();
    }
    return exprRet;
}

NodeVal Codegen::codegenUntypedUn(CodeLoc codeLoc, Token::Oper op, UntypedVal unty) {
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

NodeVal Codegen::codegenBin(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeOp = ast->children[0].get();
    const AstNode *nodeL = ast->children[1].get();
    const AstNode *nodeR = ast->children[2].get();

    optional<Token::Oper> optOp = getOper(nodeOp, true);
    if (!optOp.has_value()) return NodeVal();
    Token::Oper op = optOp.value();

    if (op == Token::O_AND || op == Token::O_OR) {
        return codegenLogicAndOr(ast);
    } else if (op == Token::O_IND) {
        return codegenInd(ast);
    } else if (op == Token::O_DOT) {
        return codegenDot(ast);
    }

    NodeVal exprPayL, exprPayR, exprPayRet;

    bool assignment = operInfos.at(op).assignment;

    exprPayL = codegenExpr(nodeL);
    if (!checkValueUnbroken(nodeL->codeLoc, exprPayL, true)) return NodeVal();

    if (assignment) {
        if (!exprPayL.isLlvmVal() || exprPayL.llvmVal.refBroken()) {
            msgs->errorExprAsgnNonRef(ast->codeLoc, op);
            return NodeVal();
        }
        if (getTypeTable()->worksAsTypeCn(exprPayL.llvmVal.type)) {
            msgs->errorExprAsgnOnCn(ast->codeLoc, op);
            return NodeVal();
        }
    }

    exprPayR = codegenExpr(nodeR);
    if (!checkValueUnbroken(nodeR->codeLoc, exprPayR, true)) return NodeVal();

    if (exprPayL.isUntyVal() && !exprPayR.isUntyVal()) {
        if (!promoteUntyped(exprPayL, exprPayR.llvmVal.type)) {
            msgs->errorExprCannotPromote(nodeL->codeLoc, exprPayR.llvmVal.type);
            return NodeVal();
        }
    } else if (!exprPayL.isUntyVal() && exprPayR.isUntyVal()) {
        if (!promoteUntyped(exprPayR, exprPayL.llvmVal.type)) {
            msgs->errorExprCannotPromote(nodeR->codeLoc, exprPayL.llvmVal.type);
            return NodeVal();
        }
    } else if (exprPayL.isUntyVal() && exprPayR.isUntyVal()) {
        return codegenUntypedBin(ast->codeLoc, op, exprPayL.untyVal, exprPayR.untyVal);
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
            msgs->errorExprCannotImplicitCastEither(ast->codeLoc, exprPayL.llvmVal.type, exprPayR.llvmVal.type);
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
                        llvmBuilder.CreateICmpEQ(llvmBuilder.CreatePtrToInt(valL, getType(getPrimTypeId(TypeTable::WIDEST_I))),
                        llvmBuilder.CreatePtrToInt(valR, getType(getPrimTypeId(TypeTable::WIDEST_I))), "pcmp_eq_tmp");
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
                        llvmBuilder.CreateICmpNE(llvmBuilder.CreatePtrToInt(valL, getType(getPrimTypeId(TypeTable::WIDEST_I))),
                        llvmBuilder.CreatePtrToInt(valR, getType(getPrimTypeId(TypeTable::WIDEST_I))), "pcmp_eq_tmp");
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
                msgs->errorInternal(ast->codeLoc);
                return NodeVal();
        }
    }

    if (exprPayRet.valueBroken()) {
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }

    if (assignment) {
        llvmBuilder.CreateStore(exprPayRet.llvmVal.val, exprPayRet.llvmVal.ref);
    }

    return exprPayRet;
}

NodeVal Codegen::codegenLogicAndOr(const AstNode *ast) {
    if (isGlobalScope()) {
        return codegenLogicAndOrGlobalScope(ast);
    }

    if (!checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeOp = ast->children[0].get();
    const AstNode *nodeL = ast->children[1].get();
    const AstNode *nodeR = ast->children[2].get();

    optional<Token::Oper> optOp = getOper(nodeOp, true);
    if (!optOp.has_value()) return NodeVal();
    Token::Oper op = optOp.value();

    NodeVal exprPayL, exprPayR, exprPayRet;

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *firstBlock = llvm::BasicBlock::Create(llvmContext, "start", func);
    llvm::BasicBlock *otherBlock = llvm::BasicBlock::Create(llvmContext, "other");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(firstBlock);

    llvmBuilder.SetInsertPoint(firstBlock);
    exprPayL = codegenExpr(nodeL);
    if (!checkValueUnbroken(nodeL->codeLoc, exprPayL, true)) return NodeVal();
    if (!isBool(exprPayL)) {
        if (exprPayL.kind == NodeVal::Kind::kLlvmVal)
            msgs->errorExprCannotImplicitCast(nodeL->codeLoc, exprPayL.llvmVal.type, getPrimTypeId(TypeTable::P_BOOL));
        else
            msgs->errorUnknown(nodeL->codeLoc);
        return NodeVal();
    }

    if (op == Token::O_AND) {
        if (exprPayL.isUntyVal()) {
            llvmBuilder.CreateBr(exprPayL.untyVal.val_b ? otherBlock : afterBlock);
        } else {
            llvmBuilder.CreateCondBr(exprPayL.llvmVal.val, otherBlock, afterBlock);
        }
    } else if (op == Token::O_OR) {
        if (exprPayL.isUntyVal()) {
            llvmBuilder.CreateBr(exprPayL.untyVal.val_b ? afterBlock : otherBlock);
        } else {
            llvmBuilder.CreateCondBr(exprPayL.llvmVal.val, afterBlock, otherBlock);
        }
    } else {
        msgs->errorInternal(ast->codeLoc);
        return NodeVal();
    }
    firstBlock = llvmBuilder.GetInsertBlock();

    func->getBasicBlockList().push_back(otherBlock);
    llvmBuilder.SetInsertPoint(otherBlock);
    exprPayR = codegenExpr(nodeR);
    if (!checkValueUnbroken(nodeR->codeLoc, exprPayR, true)) return NodeVal();
    if (!isBool(exprPayR)) {
        if (exprPayR.kind == NodeVal::Kind::kLlvmVal)
            msgs->errorExprCannotImplicitCast(nodeR->codeLoc, exprPayR.llvmVal.type, getPrimTypeId(TypeTable::P_BOOL));
        else
            msgs->errorUnknown(nodeR->codeLoc);
        return NodeVal();
    }
    llvmBuilder.CreateBr(afterBlock);
    otherBlock = llvmBuilder.GetInsertBlock();

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
    NodeVal ret;
    if (exprPayL.isUntyVal() && exprPayR.isUntyVal()) {
        // this cannot be moved to other codegen methods,
        // as we don't know whether exprs are untyVals until we call codegenExpr,
        // but calling it emits code to LLVM at that point
        ret = NodeVal(NodeVal::Kind::kUntyVal);
        ret.untyVal.kind = UntypedVal::Kind::kBool;
        if (op == Token::O_AND)
            ret.untyVal.val_b = exprPayL.untyVal.val_b && exprPayR.untyVal.val_b;
        else
            ret.untyVal.val_b = exprPayL.untyVal.val_b || exprPayR.untyVal.val_b;
    } else {
        ret = NodeVal(NodeVal::Kind::kLlvmVal);

        llvm::PHINode *phi = llvmBuilder.CreatePHI(getType(getPrimTypeId(TypeTable::P_BOOL)), 2, "logic_tmp");

        if (op == Token::O_AND)
            phi->addIncoming(getConstB(false), firstBlock);
        else
            phi->addIncoming(getConstB(true), firstBlock);
        
        if (exprPayR.isUntyVal()) {
            phi->addIncoming(getConstB(exprPayR.untyVal.val_b), otherBlock);
        } else {
            phi->addIncoming(exprPayR.llvmVal.val, otherBlock);
        }

        ret.llvmVal.type = getPrimTypeId(TypeTable::P_BOOL);
        ret.llvmVal.val = phi;
    }

    return ret;
}

NodeVal Codegen::codegenLogicAndOrGlobalScope(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeOp = ast->children[0].get();
    const AstNode *nodeL = ast->children[1].get();
    const AstNode *nodeR = ast->children[2].get();

    optional<Token::Oper> optOp = getOper(nodeOp, true);
    if (!optOp.has_value()) return NodeVal();
    Token::Oper op = optOp.value();

    NodeVal exprPayL = codegenExpr(nodeL);
    if (!exprPayL.isUntyVal()) {
        msgs->errorExprNotBaked(nodeL->codeLoc);
        return NodeVal();
    }
    if (!isBool(exprPayL)) {
        msgs->errorUnknown(nodeL->codeLoc);
        return NodeVal();
    }

    NodeVal exprPayR = codegenExpr(nodeR);
    if (!exprPayR.isUntyVal()) {
        msgs->errorExprNotBaked(nodeR->codeLoc);
        return NodeVal();
    }
    if (!isBool(exprPayR)) {
        msgs->errorUnknown(nodeR->codeLoc);
        return NodeVal();
    }

    NodeVal exprPayRet(NodeVal::Kind::kUntyVal);
    exprPayRet.untyVal.kind = UntypedVal::Kind::kBool;
    if (op == Token::O_AND)
        exprPayRet.untyVal.val_b = exprPayL.untyVal.val_b && exprPayR.untyVal.val_b;
    else
        exprPayRet.untyVal.val_b = exprPayL.untyVal.val_b || exprPayR.untyVal.val_b;

    return exprPayRet;
}

NodeVal Codegen::codegenUntypedBin(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR) {
    if (untyL.kind != untyR.kind) {
        msgs->errorExprUntyMismatch(codeLoc);
        return NodeVal();
    }
    if (untyL.kind == UntypedVal::Kind::kNone) {
        // should not happen
        msgs->errorUnknown(codeLoc);
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

NodeVal Codegen::codegenCall(const AstNode *ast) {
    if (!checkNotTerminal(ast, true))
        return NodeVal();
    
    const AstNode *nodeFuncName = ast->children[0].get();

    size_t argCnt = ast->children.size()-1;

    optional<NamePool::Id> funcName = getId(nodeFuncName, true);
    if (!funcName.has_value()) return NodeVal();

    FuncCallSite call(argCnt);
    call.name = funcName.value();

    vector<llvm::Value*> args(argCnt);
    vector<NodeVal> exprs(args.size());

    for (size_t i = 0; i < argCnt; ++i) {
        const AstNode *nodeArg = ast->children[i+1].get();

        exprs[i] = codegenExpr(nodeArg);
        if (!checkValueUnbroken(nodeArg->codeLoc, exprs[i], true)) return NodeVal();

        if (exprs[i].isLlvmVal()) {
            call.set(i, exprs[i].llvmVal.type);
            args[i] = exprs[i].llvmVal.val;
        } else {
            call.set(i, exprs[i].untyVal);
        }
    }

    optional<FuncValue> func = symbolTable->getFuncForCall(call);
    if (!func.has_value()) {
        msgs->errorFuncNotFound(nodeFuncName->codeLoc, funcName.value());
        return NodeVal();
    }

    for (size_t i = 0; i < argCnt; ++i) {
        const AstNode *nodeArg = ast->children[i+1].get();

        if (i >= func.value().argTypes.size()) {
            // variadic arguments
            if (exprs[i].isUntyVal()) {
                msgs->errorExprCallVariadUnty(nodeArg->codeLoc, funcName.value());
                return NodeVal();
            }
            // don't need to do anything in else case
        } else {
            if (exprs[i].isUntyVal()) {
                // this also checks whether sint/uint literals fit into the arg type size
                if (!promoteUntyped(exprs[i], func.value().argTypes[i])) {
                    msgs->errorExprCannotPromote(nodeArg->codeLoc, func.value().argTypes[i]);
                    return NodeVal();
                }
                args[i] = exprs[i].llvmVal.val;
            } else if (call.argTypes[i] != func.value().argTypes[i]) {
                if (!createCast(args[i], call.argTypes[i], func.value().argTypes[i])) {
                    msgs->errorExprCannotImplicitCast(nodeArg->codeLoc, call.argTypes[i], func.value().argTypes[i]);
                    return NodeVal();
                }
            }
        }
    }

    NodeVal ret;
    if (func.value().hasRet()) {
        ret = NodeVal(NodeVal::Kind::kLlvmVal);
        ret.llvmVal.type = func.value().retType.value();
        ret.llvmVal.val = llvmBuilder.CreateCall(func.value().func, args, "call_tmp");
    } else {
        ret = NodeVal(NodeVal::Kind::kEmpty);
        llvmBuilder.CreateCall(func.value().func, args, "");
    }
    return ret;
}

NodeVal Codegen::codegenCast(const AstNode *ast) {
    if (!checkStartingKeyword(ast, Token::T_CAST, true) ||
        !checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeType = ast->children[1].get();
    const AstNode *nodeVal = ast->children[2].get();

    NodeVal valTypeId = codegenType(nodeType);
    if (!checkIsType(nodeType->codeLoc, valTypeId, true)) return NodeVal();

    llvm::Type *type = getType(valTypeId.type);
    if (type == nullptr) return NodeVal();

    NodeVal exprVal = codegenExpr(nodeVal);
    if (!checkValueUnbroken(nodeVal->codeLoc, exprVal, true)) return NodeVal();

    if (exprVal.isUntyVal()) {
        TypeTable::Id promoType;
        switch (exprVal.untyVal.kind) {
        case UntypedVal::Kind::kBool:
            promoType = getPrimTypeId(TypeTable::P_BOOL);
            break;
        case UntypedVal::Kind::kSint:
            promoType = getTypeTable()->shortestFittingTypeIId(exprVal.untyVal.val_si);
            break;
        case UntypedVal::Kind::kChar:
            promoType = getPrimTypeId(TypeTable::P_C8);
            break;
        case UntypedVal::Kind::kFloat:
            // cast to widest float type
            promoType = getPrimTypeId(TypeTable::WIDEST_F);
            break;
        case UntypedVal::Kind::kString:
            promoType = getTypeTable()->getTypeIdStr();
            break;
        case UntypedVal::Kind::kNull:
            promoType = getPrimTypeId(TypeTable::P_PTR);
            break;
        default:
            msgs->errorInternal(ast->codeLoc);
            return NodeVal();
        }
        if (!promoteUntyped(exprVal, promoType)) {
            msgs->errorInternal(ast->codeLoc);
            return NodeVal();
        }
    }
    
    llvm::Value *val = exprVal.llvmVal.val;
    if (!createCast(val, exprVal.llvmVal.type, type, valTypeId.type)) {
        msgs->errorExprCannotCast(nodeVal->codeLoc, exprVal.llvmVal.type, valTypeId.type);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kLlvmVal);
    ret.llvmVal.type = valTypeId.type;
    ret.llvmVal.val = val;
    return ret;
}

NodeVal Codegen::codegenArr(const AstNode *ast) {
    if (!checkStartingKeyword(ast, Token::T_ARR, true) ||
        !checkAtLeastChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeArrType = ast->children[1].get();

    size_t arrLen = ast->children.size()-2;

    NodeVal arrDeclTypeId = codegenType(nodeArrType);
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

    llvm::Type *elemType = getType(elemTypeId);
    if (elemType == nullptr) {
        msgs->errorInternal(nodeArrType->codeLoc);
        return NodeVal();
    }

    TypeTable::Id arrTypeId = getTypeTable()->addTypeArrOfLenIdOf(
        elemTypeId, arrLen);
    
    llvm::Value *arrRef = createAlloca(getType(arrTypeId), "tmp_arr");

    for (size_t i = 0; i < arrLen; ++i) {
        const AstNode *nodeElem = ast->children[i+2].get();

        NodeVal exprPay = codegenExpr(nodeElem);
        if (!checkValueUnbroken(nodeElem->codeLoc, exprPay, true)) return NodeVal();

        if (exprPay.isUntyVal() && !promoteUntyped(exprPay, elemTypeId)) {
            msgs->errorExprCannotPromote(nodeElem->codeLoc, elemTypeId);
            return NodeVal();
        }
        if (!getTypeTable()->isImplicitCastable(exprPay.llvmVal.type, elemTypeId)) {
            msgs->errorExprCannotImplicitCast(nodeElem->codeLoc, exprPay.llvmVal.type, elemTypeId);
            return NodeVal();
        }
        createCast(exprPay, elemTypeId);

        llvm::Value *elemRef = llvmBuilder.CreateGEP(arrRef, 
            {llvm::ConstantInt::get(getType(getPrimTypeId(TypeTable::WIDEST_I)), 0),
            llvm::ConstantInt::get(getType(getPrimTypeId(TypeTable::WIDEST_I)), i)});
        llvmBuilder.CreateStore(exprPay.llvmVal.val, elemRef);
    }

    llvm::Value *arrVal = llvmBuilder.CreateLoad(arrRef);

    NodeVal retPay(NodeVal::Kind::kLlvmVal);
    retPay.llvmVal.type = arrTypeId;
    retPay.llvmVal.val = arrVal;
    return retPay;
}