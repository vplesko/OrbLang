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
    case AST_TernCondExpr:
        return codegen((const TernCondExprAst*)ast);
    case AST_CallExpr:
        return codegen((const CallExprAst*)ast);
    case AST_CastExpr:
        return codegen((const CastExprAst*)ast);
    default:
        panic = true;
        return {};
    }
}

Codegen::ExprGenPayload Codegen::codegen(const UntypedExprAst *ast) {
    if (ast->getVal().type == UntypedVal::T_NONE) {
        panic = true;
        return {};
    }

    return { .untyVal = ast->getVal() };
}

Codegen::ExprGenPayload Codegen::codegen(const VarExprAst *ast) {
    pair<SymbolTable::VarPayload, bool> var = symbolTable->getVar(ast->getNameId());
    if (!var.second) {
        panic = true;
        return {};
    }
    return {var.first.type, llvmBuilder.CreateLoad(var.first.val, namePool->get(ast->getNameId())), var.first.val};
}

Codegen::ExprGenPayload Codegen::codegen(const IndExprAst *ast) {
    ExprGenPayload baseExprPay = codegenExpr(ast->getBase());
    // no literal can indexed (no indexing on null)
    if (valBroken(baseExprPay)) return {};

    ExprGenPayload indExprPay = codegenExpr(ast->getInd());
    if (valueBroken(indExprPay)) return {};
    if (!getTypeTable()->isTypeI(indExprPay.type) && !getTypeTable()->isTypeU(indExprPay.type) && indExprPay.untyVal.type != UntypedVal::T_SINT) {
        panic = true;
        return {};
    }

    pair<bool, TypeTable::Id> typeId = symbolTable->getTypeTable()->addTypeIndex(baseExprPay.type);
    if (typeId.first == false) return {};

    ExprGenPayload retPay;
    retPay.type = typeId.second;

    if (indExprPay.isUntyVal()) {
        // TODO warning if oob index on sized array
        if (indExprPay.untyVal.type != UntypedVal::T_SINT) return {};
        if (!promoteUntyped(indExprPay, TypeTable::shortestFittingTypeI(indExprPay.untyVal.val_si))) return {};
    }

    if (symbolTable->getTypeTable()->isTypeArrP(baseExprPay.type)) {
        retPay.ref = llvmBuilder.CreateGEP(baseExprPay.val, indExprPay.val);
        retPay.val = llvmBuilder.CreateLoad(retPay.ref, "index_tmp");
    } else if (symbolTable->getTypeTable()->isTypeArr(baseExprPay.type)) {
        if (baseExprPay.ref != nullptr) {
            retPay.ref = llvmBuilder.CreateGEP(baseExprPay.ref,
                {llvm::ConstantInt::get(getType(indExprPay.type), 0), indExprPay.val});
            retPay.val = llvmBuilder.CreateLoad(retPay.ref, "index_tmp");
        } else {
            // extractvalue requires compile-time known indices
            llvm::Value *tmp = createAlloca(getType(baseExprPay.type), "tmp");
            llvmBuilder.CreateStore(baseExprPay.val, tmp);
            tmp = llvmBuilder.CreateGEP(tmp,
                {llvm::ConstantInt::get(getType(indExprPay.type), 0), indExprPay.val});
            retPay.val = llvmBuilder.CreateLoad(tmp, "index_tmp");
        }
    } else {
        panic = true;
        return {};
    }

    return retPay;
}

Codegen::ExprGenPayload Codegen::codegen(const UnExprAst *ast) {
    ExprGenPayload exprPay = codegenExpr(ast->getExpr());
    if (valueBroken(exprPay)) return {};

    if (exprPay.isUntyVal()) return codegenUntypedUn(ast->getOp(), exprPay.untyVal);

    ExprGenPayload exprRet;
    exprRet.type = exprPay.type;
    if (ast->getOp() == Token::O_ADD) {
        if (!(getTypeTable()->isTypeI(exprPay.type) || getTypeTable()->isTypeU(exprPay.type) || getTypeTable()->isTypeF(exprPay.type))) {
            panic = true;
            return {};
        }
        exprRet.val = exprPay.val;
    } else if (ast->getOp() == Token::O_SUB) {
        if (getTypeTable()->isTypeI(exprPay.type)) {
            exprRet.val = llvmBuilder.CreateNeg(exprPay.val, "sneg_tmp");
        } else if (getTypeTable()->isTypeF(exprPay.type)) {
            exprRet.val = llvmBuilder.CreateFNeg(exprPay.val, "fneg_tmp");
        } else {
            panic = true;
            return {};
        }
    } else if (ast->getOp() == Token::O_INC) {
        if (getTypeTable()->isTypeCn(exprPay.type)) {
            panic = true;
            return {};
        }
        if (!(getTypeTable()->isTypeI(exprPay.type) || getTypeTable()->isTypeU(exprPay.type)) || refBroken(exprPay)) {
            panic = true;
            return {};
        }
        exprRet.val = llvmBuilder.CreateAdd(exprPay.val, llvm::ConstantInt::get(getType(exprPay.type), 1), "inc_tmp");
        exprRet.ref = exprPay.ref;
        llvmBuilder.CreateStore(exprRet.val, exprRet.ref);
    } else if (ast->getOp() == Token::O_DEC) {
        if (getTypeTable()->isTypeCn(exprPay.type)) {
            panic = true;
            return {};
        }
        if (!(getTypeTable()->isTypeI(exprPay.type) || getTypeTable()->isTypeU(exprPay.type)) || refBroken(exprPay)) {
            panic = true;
            return {};
        }
        exprRet.val = llvmBuilder.CreateSub(exprPay.val, llvm::ConstantInt::get(getType(exprPay.type), 1), "dec_tmp");
        exprRet.ref = exprPay.ref;
        llvmBuilder.CreateStore(exprRet.val, exprRet.ref);
    } else if (ast->getOp() == Token::O_BIT_NOT) {
        if (!(getTypeTable()->isTypeI(exprPay.type) || getTypeTable()->isTypeU(exprPay.type))) {
            panic = true;
            return {};
        }
        exprRet.val = llvmBuilder.CreateNot(exprPay.val, "bit_not_tmp");
    } else if (ast->getOp() == Token::O_NOT) {
        if (!getTypeTable()->isTypeB(exprPay.type)) {
            panic = true;
            return {};
        }
        exprRet.val = llvmBuilder.CreateNot(exprPay.val, "not_tmp");
    } else if (ast->getOp() == Token::O_MUL) {
        pair<bool, TypeTable::Id> typeId = symbolTable->getTypeTable()->addTypeDeref(exprPay.type);
        if (typeId.first == false) {
            panic = true;
            return {};
        }
        exprRet.type = typeId.second;
        exprRet.val = llvmBuilder.CreateLoad(exprPay.val, "deref_tmp");
        exprRet.ref = exprPay.val;
    } else if (ast->getOp() == Token::O_BIT_AND) {
        if (exprPay.ref == nullptr) {
            panic = true;
            return {};
        }
        TypeTable::Id typeId = symbolTable->getTypeTable()->addTypeAddr(exprPay.type);
        exprRet.type = typeId;
        exprRet.val = exprPay.ref;
    } else {
        panic = true;
        return {};
    }
    return exprRet;
}

Codegen::ExprGenPayload Codegen::codegenUntypedUn(Token::Oper op, UntypedVal unty) {
    ExprGenPayload exprRet;
    exprRet.untyVal.type = unty.type;
    if (op == Token::O_ADD) {
        if (unty.type != UntypedVal::T_SINT && unty.type != UntypedVal::T_FLOAT) {
            panic = true;
            return {};
        }
        exprRet.untyVal = unty;
    } else if (op == Token::O_SUB) {
        if (unty.type == UntypedVal::T_SINT) {
            exprRet.untyVal.val_si = -unty.val_si;
        } else if (unty.type == UntypedVal::T_FLOAT) {
            exprRet.untyVal.val_f = -unty.val_f;
        } else {
            panic = true;
            return {};
        }
    } else if (op == Token::O_BIT_NOT) {
        if (unty.type == UntypedVal::T_SINT) {
            exprRet.untyVal.val_si = ~unty.val_si;
        } else {
            panic = true;
            return {};
        }
    } else if (op == Token::O_NOT) {
        if (unty.type == UntypedVal::T_BOOL) {
            exprRet.untyVal.val_b = !unty.val_b;
        } else {
            panic = true;
            return {};
        }
    } else {
        // TODO error msg when null, can't & or * or index
        panic = true;
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
        if (refBroken(exprPayL)) return {};
        if (getTypeTable()->isTypeCn(exprPayL.type)) {
            panic = true;
            return {};
        }
    } else {
        if (valueBroken(exprPayL)) return {};
    }

    exprPayR = codegenExpr(ast->getR());
    if (valueBroken(exprPayR)) return {};

    if (exprPayL.isUntyVal() && !exprPayR.isUntyVal()) {
        if (!promoteUntyped(exprPayL, exprPayR.type)) return {};
    } else if (!exprPayL.isUntyVal() && exprPayR.isUntyVal()) {
        if (!promoteUntyped(exprPayR, exprPayL.type)) return {};
    } else if (exprPayL.isUntyVal() && exprPayR.isUntyVal()) {
        return codegenUntypedBin(ast->getOp(), exprPayL.untyVal, exprPayR.untyVal);
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
            panic = true;
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
        bool isTypeP = symbolTable->getTypeTable()->isTypeP(exprPayRet.type);

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
                panic = true;
                return {};
        }
    }

    if (valueBroken(exprPayRet)) return {};

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
    if (valueBroken(exprPayL) || !isBool(exprPayL)) {
        panic = true;
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
        panic = true;
        return {};
    }
    firstBlock = llvmBuilder.GetInsertBlock();

    func->getBasicBlockList().push_back(otherBlock);
    llvmBuilder.SetInsertPoint(otherBlock);
    exprPayR = codegenExpr(ast->getR());
    if (valueBroken(exprPayR) || !isBool(exprPayR)) {
        panic = true;
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
    if (!exprPayL.isUntyVal() || !isBool(exprPayL)) {
        panic = true;
        return {};
    }

    ExprGenPayload exprPayR = codegenExpr(ast->getR());
    if (!exprPayR.isUntyVal() || !isBool(exprPayR)) {
        panic = true;
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

Codegen::ExprGenPayload Codegen::codegenUntypedBin(Token::Oper op, UntypedVal untyL, UntypedVal untyR) {
    if (untyL.type != untyR.type || untyL.type == UntypedVal::T_NONE) {
        panic = true;
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
            panic = true;
            break;
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
            panic = true;
            break;
        }
    } else {
        bool isTypeI = untyValRet.type == UntypedVal::T_SINT;
        bool isTypeC = untyValRet.type == UntypedVal::T_CHAR;
        bool isTypeF = untyValRet.type == UntypedVal::T_FLOAT;

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
                if (isTypeI)
                    untyValRet.val_si = untyL.val_si<<untyR.val_si;
                else
                    panic = true;
                break;
            case Token::O_SHR:
                if (isTypeI)
                    untyValRet.val_si = untyL.val_si>>untyR.val_si;
                else
                    panic = true;
                break;
            case Token::O_BIT_AND:
                if (isTypeI)
                    untyValRet.val_si = untyL.val_si&untyR.val_si;
                else
                    panic = true;
                break;
            case Token::O_BIT_XOR:
                if (isTypeI)
                    untyValRet.val_si = untyL.val_si^untyR.val_si;
                else
                    panic = true;
                break;
            case Token::O_BIT_OR:
                if (isTypeI)
                    untyValRet.val_si = untyL.val_si|untyR.val_si;
                else
                    panic = true;
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
                panic = true;
        }
    }

    if (panic) return {};
    return { .untyVal = untyValRet };
}

Codegen::ExprGenPayload Codegen::codegen(const TernCondExprAst *ast) {
    // TODO see if you can use llvm's select instr
    if (isGlobalScope()) {
        return codegenGlobalScope(ast);
    }
    
    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (valueBroken(condExpr) || !isBool(condExpr)) {
        panic = true;
        return {};
    }

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *trueBlock = llvm::BasicBlock::Create(llvmContext, "true", func);
    llvm::BasicBlock *falseBlock = llvm::BasicBlock::Create(llvmContext, "else");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    if (condExpr.isUntyVal()) {
        llvmBuilder.CreateBr(condExpr.untyVal.val_b ? trueBlock : falseBlock);
    } else {
        llvmBuilder.CreateCondBr(condExpr.val, trueBlock, falseBlock);
    }

    llvmBuilder.SetInsertPoint(trueBlock);
    ExprGenPayload trueExpr = codegenExpr(ast->getOp1());
    if (valueBroken(trueExpr)) return {};
    trueBlock = llvmBuilder.GetInsertBlock();

    func->getBasicBlockList().push_back(falseBlock);
    llvmBuilder.SetInsertPoint(falseBlock);
    ExprGenPayload falseExpr = codegenExpr(ast->getOp2());
    if (valueBroken(falseExpr)) return {};
    falseBlock = llvmBuilder.GetInsertBlock();

    if (!trueExpr.isUntyVal() && !falseExpr.isUntyVal()) {
        if (trueExpr.type != falseExpr.type) {
            if (getTypeTable()->isImplicitCastable(trueExpr.type, falseExpr.type)) {
                llvmBuilder.SetInsertPoint(trueBlock);
                createCast(trueExpr, falseExpr.type);
                trueBlock = llvmBuilder.GetInsertBlock();
            } else if (getTypeTable()->isImplicitCastable(falseExpr.type, trueExpr.type)) {
                llvmBuilder.SetInsertPoint(falseBlock);
                createCast(falseExpr, trueExpr.type);
                falseBlock = llvmBuilder.GetInsertBlock();
            } else {
                panic = true;
                return {};
            }
        }
    } else if (trueExpr.isUntyVal() && !falseExpr.isUntyVal()) {
        if (!promoteUntyped(trueExpr, falseExpr.type)) return {};
    } else if (!trueExpr.isUntyVal() && falseExpr.isUntyVal()) {
        if (!promoteUntyped(falseExpr, trueExpr.type)) return {};
    } else if (trueExpr.isUntyVal() && falseExpr.isUntyVal()) {
        if (trueExpr.untyVal.type != falseExpr.untyVal.type) {
            panic = true;
            return {};
        }

        // if all three untyVals, we will return a untyVal, handled below in function
        if (!condExpr.isUntyVal()) {
            if (trueExpr.untyVal.type == UntypedVal::T_BOOL) {
                if (!promoteUntyped(trueExpr, TypeTable::P_BOOL) ||
                    !promoteUntyped(falseExpr, TypeTable::P_BOOL))
                    return {};
            } else if (trueExpr.untyVal.type == UntypedVal::T_SINT) {
                TypeTable::Id trueT = TypeTable::shortestFittingTypeI(trueExpr.untyVal.val_si);
                TypeTable::Id falseT = TypeTable::shortestFittingTypeI(falseExpr.untyVal.val_si);

                if (getTypeTable()->isImplicitCastable(trueT, falseT)) {
                    if (!promoteUntyped(trueExpr, falseT) ||
                        !promoteUntyped(falseExpr, falseT))
                        return {};
                } else {
                    if (!promoteUntyped(trueExpr, trueT) ||
                        !promoteUntyped(falseExpr, trueT))
                        return {};
                }
            } else if (trueExpr.untyVal.type == UntypedVal::T_CHAR) {
                if (!promoteUntyped(trueExpr, TypeTable::P_C8) ||
                    !promoteUntyped(falseExpr, TypeTable::P_C8))
                    return {};
            } else if (trueExpr.untyVal.type == UntypedVal::T_FLOAT) {
                /*
                REM
                Here, we have two floats and need to decide whether to cast them to f32 or f64.
                It's a compromise between more precision and easier casting.
                If we say just cast it to the widest float type, then future versions of the lang
                would not necessarily be backwards-compatible with old code.
                Therefore, it's simply dissalowed. Can be overcome by explicit casting.
                */
                panic = true;
                return {};
            } else if (trueExpr.untyVal.type == UntypedVal::T_NULL) {
                /*
                REM
                Similarily, don't know into which type to cast this null.
                This should rarely be used anyway.
                */
                panic = true;
                return {};
            } else {
                panic = true;
                return {};
            }
        }
    }

    llvmBuilder.SetInsertPoint(trueBlock);
    llvmBuilder.CreateBr(afterBlock);
    trueBlock = llvmBuilder.GetInsertBlock();

    llvmBuilder.SetInsertPoint(falseBlock);
    llvmBuilder.CreateBr(afterBlock);
    falseBlock = llvmBuilder.GetInsertBlock();

    ExprGenPayload ret;

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
    if (condExpr.isUntyVal()) {
        if (trueExpr.isUntyVal()/* && falseExpr.isUntyVal()*/) {
            ret.untyVal.type = trueExpr.untyVal.type;
            if (ret.untyVal.type == UntypedVal::T_BOOL)
                ret.untyVal.val_b = condExpr.untyVal.val_b ? trueExpr.untyVal.val_b : falseExpr.untyVal.val_b;
            else if (ret.untyVal.type == UntypedVal::T_SINT)
                ret.untyVal.val_si = condExpr.untyVal.val_b ? trueExpr.untyVal.val_si : falseExpr.untyVal.val_si;
            else if (ret.untyVal.type == UntypedVal::T_CHAR)
                ret.untyVal.val_c = condExpr.untyVal.val_b ? trueExpr.untyVal.val_c : falseExpr.untyVal.val_c;
            else if (ret.untyVal.type == UntypedVal::T_FLOAT)
                ret.untyVal.val_f = condExpr.untyVal.val_b ? trueExpr.untyVal.val_f : falseExpr.untyVal.val_f;
            else if (ret.untyVal.type == UntypedVal::T_NULL)
                ; //ret.untyVal.type = UntypedVal::T_NULL;
            else {
                panic = true;
                return {};
            }
        } else/* both not untyVals */ {
            ret.type = trueExpr.type;
            ret.val = condExpr.untyVal.val_b ? trueExpr.val : falseExpr.val;
        }
    } else {
        llvm::PHINode *phi = llvmBuilder.CreatePHI(getType(trueExpr.type), 2, "tern_tmp");
        phi->addIncoming(trueExpr.val, trueBlock);
        phi->addIncoming(falseExpr.val, falseBlock);
        ret.type = trueExpr.type;
        ret.val = phi;
    }

    return ret;
}

Codegen::ExprGenPayload Codegen::codegenGlobalScope(const TernCondExprAst *ast) {
    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (!condExpr.isUntyVal() || !isBool(condExpr)) {
        panic = true;
        return {};
    }

    ExprGenPayload trueExpr = codegenExpr(ast->getOp1());
    if (!trueExpr.isUntyVal()) return {};

    ExprGenPayload falseExpr = codegenExpr(ast->getOp2());
    if (!falseExpr.isUntyVal()) return {};

    if (trueExpr.untyVal.type != falseExpr.untyVal.type) {
        panic = true;
        return {};
    }

    ExprGenPayload ret;
    ret.untyVal.type = trueExpr.untyVal.type;
    if (ret.untyVal.type == UntypedVal::T_BOOL)
        ret.untyVal.val_b = condExpr.untyVal.val_b ? trueExpr.untyVal.val_b : falseExpr.untyVal.val_b;
    else if (ret.untyVal.type == UntypedVal::T_SINT)
        ret.untyVal.val_si = condExpr.untyVal.val_b ? trueExpr.untyVal.val_si : falseExpr.untyVal.val_si;
    else if (ret.untyVal.type == UntypedVal::T_CHAR)
        ret.untyVal.val_si = condExpr.untyVal.val_b ? trueExpr.untyVal.val_c : falseExpr.untyVal.val_c;
    else if (ret.untyVal.type == UntypedVal::T_FLOAT)
        ret.untyVal.val_f = condExpr.untyVal.val_b ? trueExpr.untyVal.val_f : falseExpr.untyVal.val_f;
    else if (ret.untyVal.type == UntypedVal::T_NULL)
        ; //ret.untyVal.type = UntypedVal::T_NULL;
    else {
        panic = true;
        return {};
    }

    return ret;
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

    pair<FuncValue, bool> func = symbolTable->getFuncForCall(call);
    if (func.second == false) {
        panic = true;
        return {};
    }

    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        if (exprs[i].isUntyVal()) {
            // this also checks whether sint/uint literals fit into the arg type size
            if (!promoteUntyped(exprs[i], func.first.argTypes[i])) return {};
            args[i] = exprs[i].val;
        } else if (call.argTypes[i] != func.first.argTypes[i]) {
            createCast(args[i], call.argTypes[i], func.first.argTypes[i]);
            if (panic) return {};
        }
    }

    // reminder, it's lvalue if returning a lvalue (by ref)
    return {func.first.retType, llvmBuilder.CreateCall(func.first.func, args, 
        func.first.hasRet ? "call_tmp" : ""), nullptr};
}

Codegen::ExprGenPayload Codegen::codegen(const CastExprAst *ast) {
    llvm::Type *type = codegenType(ast->getType());
    if (broken(type)) return {};

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
        case UntypedVal::T_NULL:
            promoType = TypeTable::P_PTR;
            break;
        default:
            panic = true;
            return {};
        }
        if (!promoteUntyped(exprVal, promoType)) return {};
    }
    
    llvm::Value *val = exprVal.val;
    createCast(val, exprVal.type, type, ast->getType()->getTypeId());

    if (val == nullptr) panic = true;
    return {ast->getType()->getTypeId(), val, nullptr};
}