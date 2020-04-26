#include "Codegen.h"
using namespace std;

Codegen::ExprGenPayload Codegen::codegenExpr(const ExprAst *ast) {
    switch (ast->type()) {
    case AST_UntypedExpr:
        return codegen((const UntypedExprAst*)ast);
    case AST_VarExpr:
        return codegen((const VarExprAst*)ast);
    case AST_IndExpr:
        return codegen((const IndExprAst*)ast);
    case AST_UnExpr:
        return codegen((const UnExprAst*)ast);
    case AST_BinExpr:
        return codegen((const BinExprAst*)ast);
    case AST_CallExpr:
        return codegen((const CallExprAst*)ast);
    case AST_CastExpr:
        return codegen((const CastExprAst*)ast);
    case AST_ArrayExpr:
        return codegen((const ArrayExprAst*)ast);
    default:
        // should not happen
        msgs->errorUnknown(ast->loc());
        return {};
    }
}

Codegen::ExprGenPayload Codegen::codegen(const UntypedExprAst *ast) {
    if (ast->getVal().type == UntypedVal::T_NONE) {
        // should not happen
        msgs->errorUnknown(ast->loc());
        return {};
    }

    return { .untyVal = ast->getVal() };
}

Codegen::ExprGenPayload Codegen::codegen(const VarExprAst *ast) {
    optional<SymbolTable::VarPayload> var = symbolTable->getVar(ast->getNameId());
    if (!var.has_value()) {
        msgs->errorVarNotFound(ast->loc(), ast->getNameId());
        return {};
    }
    return {var.value().type,
        llvmBuilder.CreateLoad(var.value().val,
        namePool->get(ast->getNameId())),
        var.value().val};
}

Codegen::ExprGenPayload Codegen::codegen(const IndExprAst *ast) {
    ExprGenPayload baseExprPay = codegenExpr(ast->getBase());
    if (valueBroken(baseExprPay)) return {};

    if (baseExprPay.isUntyVal()) {
        if (baseExprPay.untyVal.type == UntypedVal::T_STRING) {
            if (!promoteUntyped(baseExprPay, getTypeTable()->getTypeIdStr())) {
                // should not happen
                msgs->errorUnknown(ast->loc());
                return {};
            }
        } else {
            msgs->errorExprIndexOnBadType(ast->loc());
            return {};
        }
    }

    ExprGenPayload indExprPay = codegenExpr(ast->getInd());
    if (valueBroken(indExprPay)) return {};

    if (indExprPay.isUntyVal()) {
        if (indExprPay.untyVal.type == UntypedVal::T_SINT) {
            if (!promoteUntyped(indExprPay, TypeTable::shortestFittingTypeI(indExprPay.untyVal.val_si))) {
                // should not happen
                msgs->errorUnknown(ast->getInd()->loc());
                return {};
            }
            if (getTypeTable()->isTypeArr(baseExprPay.type)) {
                size_t len = getTypeTable()->getArrLen(baseExprPay.type).value();
                if (indExprPay.untyVal.val_si < 0 || ((size_t) indExprPay.untyVal.val_si) >= len) {
                    msgs->warnExprIndexOutOfBounds(ast->getInd()->loc());
                }
            }
        } else {
            msgs->errorExprIndexNotIntegral(ast->getInd()->loc());
            return {};
        }
    }

    if (!getTypeTable()->isTypeI(indExprPay.type) && !getTypeTable()->isTypeU(indExprPay.type)) {
        msgs->errorExprIndexNotIntegral(ast->getInd()->loc());
        return {};
    }

    optional<TypeTable::Id> typeId = symbolTable->getTypeTable()->addTypeIndex(baseExprPay.type);
    if (!typeId) {
        msgs->errorExprIndexOnBadType(ast->loc(), baseExprPay.type);
        return {};
    }

    ExprGenPayload retPay;
    retPay.type = typeId.value();

    if (symbolTable->getTypeTable()->isTypeArrP(baseExprPay.type)) {
        retPay.ref = llvmBuilder.CreateGEP(baseExprPay.val, indExprPay.val);
        retPay.val = llvmBuilder.CreateLoad(retPay.ref, "index_tmp");
    } else if (symbolTable->getTypeTable()->isTypeArr(baseExprPay.type)) {
        if (baseExprPay.ref != nullptr) {
            retPay.ref = llvmBuilder.CreateGEP(baseExprPay.ref,
                {llvm::ConstantInt::get(getType(indExprPay.type), 0), indExprPay.val});
            retPay.val = llvmBuilder.CreateLoad(retPay.ref, "index_tmp");
        } else {
            // llvm's extractvalue requires compile-time known indices
            llvm::Value *tmp = createAlloca(getType(baseExprPay.type), "tmp");
            llvmBuilder.CreateStore(baseExprPay.val, tmp);
            tmp = llvmBuilder.CreateGEP(tmp,
                {llvm::ConstantInt::get(getType(indExprPay.type), 0), indExprPay.val});
            retPay.val = llvmBuilder.CreateLoad(tmp, "index_tmp");
        }
    } else {
        // should not happen, verified above
        msgs->errorUnknown(ast->loc());
        return {};
    }

    return retPay;
}

Codegen::ExprGenPayload Codegen::codegen(const UnExprAst *ast) {
    ExprGenPayload exprPay = codegenExpr(ast->getExpr());
    if (valueBroken(exprPay)) return {};

    if (exprPay.isUntyVal()) return codegenUntypedUn(ast->loc(), ast->getOp(), exprPay.untyVal);

    ExprGenPayload exprRet;
    exprRet.type = exprPay.type;
    if (ast->getOp() == Token::O_ADD) {
        if (!(getTypeTable()->isTypeI(exprPay.type) || getTypeTable()->isTypeU(exprPay.type) || getTypeTable()->isTypeF(exprPay.type))) {
            msgs->errorExprUnBadType(ast->loc(), ast->getOp(), exprPay.type);
            return {};
        }
        exprRet.val = exprPay.val;
    } else if (ast->getOp() == Token::O_SUB) {
        if (getTypeTable()->isTypeI(exprPay.type)) {
            exprRet.val = llvmBuilder.CreateNeg(exprPay.val, "sneg_tmp");
        } else if (getTypeTable()->isTypeF(exprPay.type)) {
            exprRet.val = llvmBuilder.CreateFNeg(exprPay.val, "fneg_tmp");
        } else {
            msgs->errorExprUnBadType(ast->loc(), ast->getOp(), exprPay.type);
            return {};
        }
    } else if (ast->getOp() == Token::O_INC) {
        if (getTypeTable()->isTypeCn(exprPay.type)) {
            msgs->errorExprUnOnCn(ast->loc(), ast->getOp());
            return {};
        }
        if (!(getTypeTable()->isTypeI(exprPay.type) || getTypeTable()->isTypeU(exprPay.type)) || refBroken(exprPay)) {
            msgs->errorExprUnBadType(ast->loc(), ast->getOp(), exprPay.type);
            return {};
        }
        exprRet.val = llvmBuilder.CreateAdd(exprPay.val, llvm::ConstantInt::get(getType(exprPay.type), 1), "inc_tmp");
        exprRet.ref = exprPay.ref;
        llvmBuilder.CreateStore(exprRet.val, exprRet.ref);
    } else if (ast->getOp() == Token::O_DEC) {
        if (getTypeTable()->isTypeCn(exprPay.type)) {
            msgs->errorExprUnOnCn(ast->loc(), ast->getOp());
            return {};
        }
        if (!(getTypeTable()->isTypeI(exprPay.type) || getTypeTable()->isTypeU(exprPay.type)) || refBroken(exprPay)) {
            msgs->errorExprUnBadType(ast->loc(), ast->getOp(), exprPay.type);
            return {};
        }
        exprRet.val = llvmBuilder.CreateSub(exprPay.val, llvm::ConstantInt::get(getType(exprPay.type), 1), "dec_tmp");
        exprRet.ref = exprPay.ref;
        llvmBuilder.CreateStore(exprRet.val, exprRet.ref);
    } else if (ast->getOp() == Token::O_BIT_NOT) {
        if (!(getTypeTable()->isTypeI(exprPay.type) || getTypeTable()->isTypeU(exprPay.type))) {
            msgs->errorExprUnBadType(ast->loc(), ast->getOp(), exprPay.type);
            return {};
        }
        exprRet.val = llvmBuilder.CreateNot(exprPay.val, "bit_not_tmp");
    } else if (ast->getOp() == Token::O_NOT) {
        if (!getTypeTable()->isTypeB(exprPay.type)) {
            msgs->errorExprUnBadType(ast->loc(), ast->getOp(), exprPay.type);
            return {};
        }
        exprRet.val = llvmBuilder.CreateNot(exprPay.val, "not_tmp");
    } else if (ast->getOp() == Token::O_MUL) {
        optional<TypeTable::Id> typeId = symbolTable->getTypeTable()->addTypeDeref(exprPay.type);
        if (!typeId) {
            msgs->errorExprDerefOnBadType(ast->loc(), exprPay.type);
            return {};
        }
        exprRet.type = typeId.value();
        exprRet.val = llvmBuilder.CreateLoad(exprPay.val, "deref_tmp");
        exprRet.ref = exprPay.val;
    } else if (ast->getOp() == Token::O_BIT_AND) {
        if (exprPay.ref == nullptr) {
            msgs->errorExprAddressOfNoRef(ast->loc());
            return {};
        }
        TypeTable::Id typeId = symbolTable->getTypeTable()->addTypeAddr(exprPay.type);
        exprRet.type = typeId;
        exprRet.val = exprPay.ref;
    } else {
        // should not happen
        msgs->errorUnknown(ast->loc());
        return {};
    }
    return exprRet;
}

Codegen::ExprGenPayload Codegen::codegenUntypedUn(CodeLoc codeLoc, Token::Oper op, UntypedVal unty) {
    ExprGenPayload exprRet;
    exprRet.untyVal.type = unty.type;
    if (op == Token::O_ADD) {
        if (unty.type != UntypedVal::T_SINT && unty.type != UntypedVal::T_FLOAT) {
            msgs->errorExprUnBadType(codeLoc, op);
            return {};
        }
        exprRet.untyVal = unty;
    } else if (op == Token::O_SUB) {
        if (unty.type == UntypedVal::T_SINT) {
            exprRet.untyVal.val_si = -unty.val_si;
        } else if (unty.type == UntypedVal::T_FLOAT) {
            exprRet.untyVal.val_f = -unty.val_f;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return {};
        }
    } else if (op == Token::O_BIT_NOT) {
        if (unty.type == UntypedVal::T_SINT) {
            exprRet.untyVal.val_si = ~unty.val_si;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return {};
        }
    } else if (op == Token::O_NOT) {
        if (unty.type == UntypedVal::T_BOOL) {
            exprRet.untyVal.val_b = !unty.val_b;
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
            return {};
        }
    } else {
        if (op == Token::O_MUL && unty.type == UntypedVal::T_NULL) {
            msgs->errorExprUnOnNull(codeLoc, op);
        } else if (op == Token::O_BIT_AND) {
            msgs->errorExprAddressOfNoRef(codeLoc);
        } else {
            msgs->errorExprUnBadType(codeLoc, op);
        }
        return {};
    }
    return exprRet;
}

Codegen::ExprGenPayload Codegen::codegen(const BinExprAst *ast) {
    if (ast->getOp() == Token::O_AND || ast->getOp() == Token::O_OR) {
        return codegenLogicAndOr(ast);
    }

    ExprGenPayload exprPayL, exprPayR, exprPayRet;

    bool assignment = operInfos.at(ast->getOp()).assignment;

    exprPayL = codegenExpr(ast->getL());
    if (assignment) {
        if (refBroken(exprPayL)) {
            msgs->errorExprAsgnNonRef(ast->loc(), ast->getOp());
            return {};
        }
        if (getTypeTable()->isTypeCn(exprPayL.type)) {
            msgs->errorExprAsgnOnCn(ast->loc(), ast->getOp());
            return {};
        }
    } else {
        if (valueBroken(exprPayL)) return {};
    }

    exprPayR = codegenExpr(ast->getR());
    if (valueBroken(exprPayR)) return {};

    if (exprPayL.isUntyVal() && !exprPayR.isUntyVal()) {
        if (!promoteUntyped(exprPayL, exprPayR.type)) {
            msgs->errorExprCannotPromote(ast->getL()->loc(), exprPayR.type);
            return {};
        }
    } else if (!exprPayL.isUntyVal() && exprPayR.isUntyVal()) {
        if (!promoteUntyped(exprPayR, exprPayL.type)) {
            msgs->errorExprCannotPromote(ast->getR()->loc(), exprPayL.type);
            return {};
        }
    } else if (exprPayL.isUntyVal() && exprPayR.isUntyVal()) {
        return codegenUntypedBin(ast->loc(), ast->getOp(), exprPayL.untyVal, exprPayR.untyVal);
    }

    llvm::Value *valL = exprPayL.val, *valR = exprPayR.val;
    exprPayRet.type = exprPayL.type;
    exprPayRet.val = nullptr;
    exprPayRet.ref = assignment ? exprPayL.ref : nullptr;

    if (exprPayL.type != exprPayR.type) {
        if (getTypeTable()->isImplicitCastable(exprPayR.type, exprPayL.type)) {
            createCast(valR, exprPayR.type, exprPayL.type);
            exprPayRet.type = exprPayL.type;
        } else if (getTypeTable()->isImplicitCastable(exprPayL.type, exprPayR.type) && !assignment) {
            createCast(valL, exprPayL.type, exprPayR.type);
            exprPayRet.type = exprPayR.type;
        } else {
            msgs->errorExprCannotImplicitCastEither(ast->loc(), exprPayL.type, exprPayR.type);
            return {};
        }
    }

    if (getTypeTable()->isTypeB(exprPayRet.type)) {
        switch (ast->getOp()) {
        case Token::O_ASGN:
            exprPayRet.val = valR;
            break;
        case Token::O_EQ:
            exprPayRet.val = llvmBuilder.CreateICmpEQ(valL, valR, "bcmp_eq_tmp");
            break;
        case Token::O_NEQ:
            exprPayRet.val = llvmBuilder.CreateICmpNE(valL, valR, "bcmp_neq_tmp");
            break;
        default:
            break;
        }
    } else {
        bool isTypeI = getTypeTable()->isTypeI(exprPayRet.type);
        bool isTypeU = getTypeTable()->isTypeU(exprPayRet.type);
        bool isTypeF = getTypeTable()->isTypeF(exprPayRet.type);
        bool isTypeC = getTypeTable()->isTypeC(exprPayRet.type);
        bool isTypeP = symbolTable->getTypeTable()->isTypeAnyP(exprPayRet.type);

        switch (ast->getOp()) {
            case Token::O_ASGN:
                exprPayRet.val = valR;
                break;
            case Token::O_ADD:
            case Token::O_ADD_ASGN:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFAdd(valL, valR, "fadd_tmp");
                else if (isTypeI || isTypeU)
                    exprPayRet.val = llvmBuilder.CreateAdd(valL, valR, "add_tmp");
                break;
            case Token::O_SUB:
            case Token::O_SUB_ASGN:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFSub(valL, valR, "fsub_tmp");
                else if (isTypeI || isTypeU)
                    exprPayRet.val = llvmBuilder.CreateSub(valL, valR, "sub_tmp");
                break;
            case Token::O_SHL:
            case Token::O_SHL_ASGN:
                if (isTypeI || isTypeU)
                    exprPayRet.val = llvmBuilder.CreateShl(valL, valR, "shl_tmp");
                break;
            case Token::O_SHR:
            case Token::O_SHR_ASGN:
                if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateAShr(valL, valR, "ashr_tmp");
                else if (isTypeU)
                    exprPayRet.val = llvmBuilder.CreateLShr(valL, valR, "lshr_tmp");
                break;
            case Token::O_BIT_AND:
            case Token::O_BIT_AND_ASGN:
                if (isTypeI || isTypeU)
                    exprPayRet.val = llvmBuilder.CreateAnd(valL, valR, "and_tmp");
                break;
            case Token::O_BIT_XOR:
            case Token::O_BIT_XOR_ASGN:
                if (isTypeI || isTypeU)
                    exprPayRet.val = llvmBuilder.CreateXor(valL, valR, "xor_tmp");
                break;
            case Token::O_BIT_OR:
            case Token::O_BIT_OR_ASGN:
                if (isTypeI || isTypeU)
                    exprPayRet.val = llvmBuilder.CreateOr(valL, valR, "or_tmp");
                break;
            case Token::O_MUL:
            case Token::O_MUL_ASGN:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFMul(valL, valR, "fmul_tmp");
                else if (isTypeI || isTypeU)
                    exprPayRet.val = llvmBuilder.CreateMul(valL, valR, "mul_tmp");
                break;
            case Token::O_DIV:
            case Token::O_DIV_ASGN:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFDiv(valL, valR, "fdiv_tmp");
                else if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateSDiv(valL, valR, "sdiv_tmp");
                else if (isTypeU)
                    exprPayRet.val = llvmBuilder.CreateUDiv(valL, valR, "udiv_tmp");
                break;
            case Token::O_REM:
            case Token::O_REM_ASGN:
                if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateSRem(valL, valR, "srem_tmp");
                else if (isTypeU)
                    exprPayRet.val = llvmBuilder.CreateURem(valL, valR, "urem_tmp");
                else if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFRem(valL, valR, "frem_tmp");
                break;
            case Token::O_EQ:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpOEQ(valL, valR, "fcmp_eq_tmp");
                else if (isTypeI || isTypeU || isTypeC)
                    exprPayRet.val = llvmBuilder.CreateICmpEQ(valL, valR, "cmp_eq_tmp");
                else if (isTypeP) {
                    exprPayRet.val = llvmBuilder.CreateICmpEQ(llvmBuilder.CreatePtrToInt(valL, getType(TypeTable::WIDEST_I)), 
                        llvmBuilder.CreatePtrToInt(valR, getType(TypeTable::WIDEST_I)), "pcmp_eq_tmp");
                }
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_NEQ:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpONE(valL, valR, "fcmp_neq_tmp");
                else if (isTypeI || isTypeU || isTypeC)
                    exprPayRet.val = llvmBuilder.CreateICmpNE(valL, valR, "cmp_neq_tmp");
                else if (isTypeP) {
                    exprPayRet.val = llvmBuilder.CreateICmpNE(llvmBuilder.CreatePtrToInt(valL, getType(TypeTable::WIDEST_I)), 
                        llvmBuilder.CreatePtrToInt(valR, getType(TypeTable::WIDEST_I)), "pcmp_eq_tmp");
                }
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_LT:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpOLT(valL, valR, "fcmp_lt_tmp");
                else if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateICmpSLT(valL, valR, "scmp_lt_tmp");
                else if (isTypeU || isTypeC)
                    exprPayRet.val = llvmBuilder.CreateICmpULT(valL, valR, "ucmp_lt_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_LTEQ:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpOLE(valL, valR, "fcmp_lteq_tmp");
                else if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateICmpSLE(valL, valR, "scmp_lteq_tmp");
                else if (isTypeU || isTypeC)
                    exprPayRet.val = llvmBuilder.CreateICmpULE(valL, valR, "ucmp_lteq_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_GT:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpOGT(valL, valR, "fcmp_gt_tmp");
                else if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateICmpSGT(valL, valR, "scmp_gt_tmp");
                else if (isTypeU || isTypeC)
                    exprPayRet.val = llvmBuilder.CreateICmpUGT(valL, valR, "ucmp_gt_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_GTEQ:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpOGE(valL, valR, "fcmp_gteq_tmp");
                else if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateICmpSGE(valL, valR, "scmp_gteq_tmp");
                else if (isTypeU || isTypeC)
                    exprPayRet.val = llvmBuilder.CreateICmpUGE(valL, valR, "ucmp_gteq_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            default:
                // should not happen
                msgs->errorUnknown(ast->loc());
                return {};
        }
    }

    if (valueBroken(exprPayRet)) {
        // should not happen
        msgs->errorUnknown(ast->loc());
        return {};
    }

    if (assignment) {
        llvmBuilder.CreateStore(exprPayRet.val, exprPayRet.ref);
    }

    return exprPayRet;
}

Codegen::ExprGenPayload Codegen::codegenLogicAndOr(const BinExprAst *ast) {
    if (isGlobalScope()) {
        return codegenLogicAndOrGlobalScope(ast);
    }

    ExprGenPayload exprPayL, exprPayR, exprPayRet;

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *firstBlock = llvm::BasicBlock::Create(llvmContext, "start", func);
    llvm::BasicBlock *otherBlock = llvm::BasicBlock::Create(llvmContext, "other");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(firstBlock);

    llvmBuilder.SetInsertPoint(firstBlock);
    exprPayL = codegenExpr(ast->getL());
    if (valueBroken(exprPayL)) {
        return {};
    }
    if (!isBool(exprPayL)) {
        msgs->errorExprCannotImplicitCast(ast->getL()->loc(), exprPayL.type, TypeTable::P_BOOL);
        return {};
    }

    if (ast->getOp() == Token::O_AND) {
        if (exprPayL.isUntyVal()) {
            llvmBuilder.CreateBr(exprPayL.untyVal.val_b ? otherBlock : afterBlock);
        } else {
            llvmBuilder.CreateCondBr(exprPayL.val, otherBlock, afterBlock);
        }
    } else if (ast->getOp() == Token::O_OR) {
        if (exprPayL.isUntyVal()) {
            llvmBuilder.CreateBr(exprPayL.untyVal.val_b ? afterBlock : otherBlock);
        } else {
            llvmBuilder.CreateCondBr(exprPayL.val, afterBlock, otherBlock);
        }
    } else {
        // should not happen
        msgs->errorUnknown(ast->loc());
        return {};
    }
    firstBlock = llvmBuilder.GetInsertBlock();

    func->getBasicBlockList().push_back(otherBlock);
    llvmBuilder.SetInsertPoint(otherBlock);
    exprPayR = codegenExpr(ast->getR());
    if (valueBroken(exprPayR)) {
        return {};
    }
    if (!isBool(exprPayR)) {
        msgs->errorExprCannotImplicitCast(ast->getR()->loc(), exprPayR.type, TypeTable::P_BOOL);
        return {};
    }
    llvmBuilder.CreateBr(afterBlock);
    otherBlock = llvmBuilder.GetInsertBlock();

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
    ExprGenPayload ret;
    if (exprPayL.isUntyVal() && exprPayR.isUntyVal()) {
        // this cannot be moved to other codegen methods,
        // as we don't know whether exprs are untyVals until we call codegenExpr,
        // but calling it emits code to LLVM at that point
        ret.untyVal.type = UntypedVal::T_BOOL;
        if (ast->getOp() == Token::O_AND)
            ret.untyVal.val_b = exprPayL.untyVal.val_b && exprPayR.untyVal.val_b;
        else
            ret.untyVal.val_b = exprPayL.untyVal.val_b || exprPayR.untyVal.val_b;
    } else {
        llvm::PHINode *phi = llvmBuilder.CreatePHI(getType(TypeTable::P_BOOL), 2, "logic_tmp");

        if (ast->getOp() == Token::O_AND)
            phi->addIncoming(getConstB(false), firstBlock);
        else
            phi->addIncoming(getConstB(true), firstBlock);
        
        if (exprPayR.isUntyVal()) {
            phi->addIncoming(getConstB(exprPayR.untyVal.val_b), otherBlock);
        } else {
            phi->addIncoming(exprPayR.val, otherBlock);
        }

        ret.type = TypeTable::P_BOOL;
        ret.val = phi;
    }

    return ret;
}

Codegen::ExprGenPayload Codegen::codegenLogicAndOrGlobalScope(const BinExprAst *ast) {
    ExprGenPayload exprPayL = codegenExpr(ast->getL());
    if (!exprPayL.isUntyVal()) {
        msgs->errorExprNotBaked(ast->getL()->loc());
        return {};
    }
    if (!isBool(exprPayL)) {
        msgs->errorExprCannotImplicitCast(ast->getL()->loc(), exprPayL.type, TypeTable::P_BOOL);
        return {};
    }

    ExprGenPayload exprPayR = codegenExpr(ast->getR());
    if (!exprPayR.isUntyVal()) {
        msgs->errorExprNotBaked(ast->getR()->loc());
        return {};
    }
    if (!isBool(exprPayR)) {
        msgs->errorExprCannotImplicitCast(ast->getR()->loc(), exprPayR.type, TypeTable::P_BOOL);
        return {};
    }

    ExprGenPayload exprPayRet;
    exprPayRet.untyVal.type = UntypedVal::T_BOOL;
    if (ast->getOp() == Token::O_AND)
        exprPayRet.untyVal.val_b = exprPayL.untyVal.val_b && exprPayR.untyVal.val_b;
    else
        exprPayRet.untyVal.val_b = exprPayL.untyVal.val_b || exprPayR.untyVal.val_b;

    return exprPayRet;
}

Codegen::ExprGenPayload Codegen::codegenUntypedBin(CodeLoc codeLoc, Token::Oper op, UntypedVal untyL, UntypedVal untyR) {
    if (untyL.type != untyR.type) {
        msgs->errorExprUntyMismatch(codeLoc);
        return {};
    }
    if (untyL.type == UntypedVal::T_NONE) {
        // should not happen
        msgs->errorUnknown(codeLoc);
        return {};
    }

    UntypedVal untyValRet;
    untyValRet.type = untyL.type;

    if (untyValRet.type == UntypedVal::T_BOOL) {
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
            return {};
        }
    } else if (untyValRet.type == UntypedVal::T_NULL) {
        // both are null
        switch (op) {
        case Token::O_EQ:
            untyValRet.type = UntypedVal::T_BOOL;
            untyValRet.val_b = true;
            break;
        case Token::O_NEQ:
            untyValRet.type = UntypedVal::T_BOOL;
            untyValRet.val_b = false;
            break;
        default:
            msgs->errorExprUntyBinBadOp(codeLoc, op);
            return {};
        }
    } else {
        bool isTypeI = untyValRet.type == UntypedVal::T_SINT;
        bool isTypeC = untyValRet.type == UntypedVal::T_CHAR;
        bool isTypeF = untyValRet.type == UntypedVal::T_FLOAT;

        if (!isTypeI && !isTypeC && !isTypeF) {
            // NOTE cannot == nor != on two string literals
            if (untyValRet.type == UntypedVal::T_STRING) {
                msgs->errorExprCompareStringLits(codeLoc);
            } else {
                msgs->errorExprUntyBinBadOp(codeLoc, op);
            }
            return {};
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
                        return {};
                    }
                    break;
                case Token::O_SHR:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si>>untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return {};
                    }
                    break;
                case Token::O_BIT_AND:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si&untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return {};
                    }
                    break;
                case Token::O_BIT_XOR:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si^untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return {};
                    }
                    break;
                case Token::O_BIT_OR:
                    if (isTypeI) {
                        untyValRet.val_si = untyL.val_si|untyR.val_si;
                    } else {
                        msgs->errorExprUntyBinBadOp(codeLoc, op);
                        return {};
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
                    untyValRet.type = UntypedVal::T_BOOL;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f == untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si == untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c == untyR.val_c;
                    break;
                case Token::O_NEQ:
                    untyValRet.type = UntypedVal::T_BOOL;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f != untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si != untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c != untyR.val_c;
                    break;
                case Token::O_LT:
                    untyValRet.type = UntypedVal::T_BOOL;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f < untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si < untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c < untyR.val_c;
                    break;
                case Token::O_LTEQ:
                    untyValRet.type = UntypedVal::T_BOOL;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f <= untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si <= untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c <= untyR.val_c;
                    break;
                case Token::O_GT:
                    untyValRet.type = UntypedVal::T_BOOL;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f > untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si > untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c > untyR.val_c;
                    break;
                case Token::O_GTEQ:
                    untyValRet.type = UntypedVal::T_BOOL;
                    if (isTypeF)
                        untyValRet.val_b = untyL.val_f >= untyR.val_f;
                    else if (isTypeI)
                        untyValRet.val_b = untyL.val_si >= untyR.val_si;
                    else if (isTypeC)
                        untyValRet.val_b = untyL.val_c >= untyR.val_c;
                    break;
                default:
                    msgs->errorExprUntyBinBadOp(codeLoc, op);
                    return {};
            }
        }
    }

    return { .untyVal = untyValRet };
}

Codegen::ExprGenPayload Codegen::codegen(const CallExprAst *ast) {
    FuncCallSite call(ast->getArgs().size());
    call.name = ast->getName();

    vector<llvm::Value*> args(ast->getArgs().size());
    vector<ExprGenPayload> exprs(args.size());

    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        exprs[i] = codegenExpr(ast->getArgs()[i].get());
        if (valueBroken(exprs[i])) return {};

        call.set(i, exprs[i].type);
        args[i] = exprs[i].val;
        call.set(i, exprs[i].untyVal);
    }

    optional<FuncValue> func = symbolTable->getFuncForCall(call);
    if (!func.has_value()) {
        msgs->errorFuncNotFound(ast->loc(), ast->getName());
        return {};
    }

    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        if (i >= func.value().argTypes.size()) {
            // variadic arguments
            if (exprs[i].isUntyVal()) {
                msgs->errorExprCallVariadUnty(ast->getArgs()[i]->loc(), ast->getName());
                return {};
            }
            // don't need to do anything in else case
        } else {
            if (exprs[i].isUntyVal()) {
                // this also checks whether sint/uint literals fit into the arg type size
                if (!promoteUntyped(exprs[i], func.value().argTypes[i])) {
                    msgs->errorExprCannotPromote(ast->getArgs()[i]->loc(), func.value().argTypes[i]);
                    return {};
                }
                args[i] = exprs[i].val;
            } else if (call.argTypes[i] != func.value().argTypes[i]) {
                if (!createCast(args[i], call.argTypes[i], func.value().argTypes[i])) {
                    msgs->errorExprCannotImplicitCast(ast->getArgs()[i]->loc(), call.argTypes[i], func.value().argTypes[i]);
                    return {};
                }
            }
        }
    }

    // NOTE it's lvalue if returning a lvalue (by ref)
    ExprGenPayload ret;
    ret.val = llvmBuilder.CreateCall(func.value().func, args, func.value().hasRet() ? "call_tmp" : "");
    if (func.value().hasRet()) {
        ret.type = func.value().retType.value();
    }
    return ret;
}

Codegen::ExprGenPayload Codegen::codegen(const CastExprAst *ast) {
    llvm::Type *type = codegenType(ast->getType());
    if (type == nullptr) return {};

    ExprGenPayload exprVal = codegenExpr(ast->getVal());
    if (valueBroken(exprVal)) return {};

    if (exprVal.isUntyVal()) {
        TypeTable::Id promoType;
        switch (exprVal.untyVal.type) {
        case UntypedVal::T_BOOL:
            promoType = TypeTable::P_BOOL;
            break;
        case UntypedVal::T_SINT:
            promoType = TypeTable::shortestFittingTypeI(exprVal.untyVal.val_si);
            break;
        case UntypedVal::T_CHAR:
            promoType = TypeTable::P_C8;
            break;
        case UntypedVal::T_FLOAT:
            // cast to widest float type
            promoType = TypeTable::WIDEST_F;
            break;
        case UntypedVal::T_STRING:
            promoType = getTypeTable()->getTypeIdStr();
            break;
        case UntypedVal::T_NULL:
            promoType = TypeTable::P_PTR;
            break;
        default:
            // should not happen
            msgs->errorUnknown(ast->loc());
            return {};
        }
        if (!promoteUntyped(exprVal, promoType)) {
            // should not happen
            msgs->errorUnknown(ast->loc());
            return {};
        }
    }
    
    llvm::Value *val = exprVal.val;
    if (!createCast(val, exprVal.type, type, ast->getType()->getTypeId())) {
        msgs->errorExprCannotCast(ast->getVal()->loc(), exprVal.type, ast->getType()->getTypeId());
        return {};
    }

    return {ast->getType()->getTypeId(), val, nullptr};
}

Codegen::ExprGenPayload Codegen::codegen(const ArrayExprAst *ast) {
    TypeTable::Id arrDeclTypeId = ast->getArrayType()->getTypeId();
    if (!getTypeTable()->isTypeArrP(arrDeclTypeId) &&
        !getTypeTable()->isTypeArrOfLen(arrDeclTypeId, ast->getVals().size())) {
        msgs->errorExprIndexOnBadType(ast->loc(), arrDeclTypeId);
        return {};
    }

    optional<TypeTable::Id> elemTypeIdFind = getTypeTable()->addTypeIndex(arrDeclTypeId);
    if (!elemTypeIdFind) {
        msgs->errorExprIndexOnBadType(ast->loc(), arrDeclTypeId);
        return {};
    }

    TypeTable::Id elemTypeId = elemTypeIdFind.value();

    llvm::Type *elemType = getType(elemTypeId);
    if (elemType == nullptr) {
        // should not happen
        msgs->errorUnknown(ast->getArrayType()->loc());
        return {};
    }

    TypeTable::Id arrTypeId = getTypeTable()->addTypeArrOfLenId(
        elemTypeId, ast->getVals().size());
    
    llvm::Value *arrRef = createAlloca(getType(arrTypeId), "tmp_arr");

    for (size_t i = 0; i < ast->getVals().size(); ++i) {
        ExprGenPayload exprPay = codegenExpr(ast->getVals()[i].get());
        if (exprPay.isUntyVal() && !promoteUntyped(exprPay, elemTypeId)) {
            msgs->errorExprCannotPromote(ast->getVals()[i]->loc(), elemTypeId);
            return {};
        }
        if (valBroken(exprPay)) return {};
        if (!getTypeTable()->isImplicitCastable(exprPay.type, elemTypeId)) {
            msgs->errorExprCannotImplicitCast(ast->getVals()[i]->loc(), exprPay.type, elemTypeId);
            return {};
        }
        createCast(exprPay, elemTypeId);

        llvm::Value *elemRef = llvmBuilder.CreateGEP(arrRef, 
            {llvm::ConstantInt::get(getType(TypeTable::WIDEST_I), 0),
            llvm::ConstantInt::get(getType(TypeTable::WIDEST_I), i)});
        llvmBuilder.CreateStore(exprPay.val, elemRef);
    }

    llvm::Value *arrVal = llvmBuilder.CreateLoad(arrRef);

    ExprGenPayload retPay;
    retPay.type = arrTypeId;
    retPay.val = arrVal;
    return retPay;
}