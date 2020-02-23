#include "Codegen.h"
using namespace std;

CodeGen::ExprGenPayload CodeGen::codegenExpr(const ExprAST *ast) {
    switch (ast->type()) {
    case AST_LiteralExpr:
        return codegen((const LiteralExprAST*)ast);
    case AST_VarExpr:
        return codegen((const VarExprAST*)ast);
    case AST_UnExpr:
        return codegen((const UnExprAST*)ast);
    case AST_BinExpr:
        return codegen((const BinExprAST*)ast);
    case AST_TernCondExpr:
        return codegen((const TernCondExprAST*)ast);
    case AST_CallExpr:
        return codegen((const CallExprAST*)ast);
    case AST_CastExpr:
        return codegen((const CastExprAST*)ast);
    default:
        panic = true;
        return {};
    }
}

CodeGen::ExprGenPayload CodeGen::codegen(const LiteralExprAST *ast) {
    if (ast->getVal().type == LiteralVal::T_NONE) {
        panic = true;
        return {};
    }

    return { .litVal = ast->getVal() };
}

CodeGen::ExprGenPayload CodeGen::codegen(const VarExprAST *ast) {
    const SymbolTable::VarPayload *var = symbolTable->getVar(ast->getNameId());
    if (broken(var)) return {};
    return {var->type, llvmBuilder.CreateLoad(var->val, namePool->get(ast->getNameId())), var->val};
}

CodeGen::ExprGenPayload CodeGen::codegen(const UnExprAST *ast) {
    ExprGenPayload exprPay = codegenExpr(ast->getExpr());
    if (valueBroken(exprPay)) return {};

    if (exprPay.isLitVal()) return codegenLiteralUn(ast->getOp(), exprPay.litVal);

    ExprGenPayload exprRet;
    exprRet.type = exprPay.type;
    if (ast->getOp() == Token::O_ADD) {
        if (!(TypeTable::isTypeI(exprPay.type) || TypeTable::isTypeU(exprPay.type) || TypeTable::isTypeF(exprPay.type))) {
            panic = true;
            return {};
        }
        exprRet.val = exprPay.val;
    } else if (ast->getOp() == Token::O_SUB) {
        if (TypeTable::isTypeI(exprPay.type)) {
            exprRet.val = llvmBuilder.CreateNeg(exprPay.val, "sneg_tmp");
        } else if (TypeTable::isTypeF(exprPay.type)) {
            exprRet.val = llvmBuilder.CreateFNeg(exprPay.val, "fneg_tmp");
        } else {
            panic = true;
            return {};
        }
    } else if (ast->getOp() == Token::O_INC) {
        if (!(TypeTable::isTypeI(exprPay.type) || TypeTable::isTypeU(exprPay.type)) || refBroken(exprPay)) {
            panic = true;
            return {};
        }
        exprRet.val = llvmBuilder.CreateAdd(exprPay.val, llvm::ConstantInt::get(getType(exprPay.type), 1), "inc_tmp");
        exprRet.ref = exprPay.ref;
        llvmBuilder.CreateStore(exprRet.val, exprRet.ref);
    } else if (ast->getOp() == Token::O_DEC) {
        if (!(TypeTable::isTypeI(exprPay.type) || TypeTable::isTypeU(exprPay.type)) || refBroken(exprPay)) {
            panic = true;
            return {};
        }
        exprRet.val = llvmBuilder.CreateSub(exprPay.val, llvm::ConstantInt::get(getType(exprPay.type), 1), "dec_tmp");
        exprRet.ref = exprPay.ref;
        llvmBuilder.CreateStore(exprRet.val, exprRet.ref);
    } else if (ast->getOp() == Token::O_BIT_NOT) {
        if (!(TypeTable::isTypeI(exprPay.type) || TypeTable::isTypeU(exprPay.type))) {
            panic = true;
            return {};
        }
        exprRet.val = llvmBuilder.CreateNot(exprPay.val, "bit_not_tmp");
    } else if (ast->getOp() == Token::O_NOT) {
        if (!TypeTable::isTypeB(exprPay.type)) {
            panic = true;
            return {};
        }
        exprRet.val = llvmBuilder.CreateNot(exprPay.val, "not_tmp");
    } else {
        panic = true;
        return {};
    }
    return exprRet;
}

// TODO * and &
// TODO can't deref null or ptr
CodeGen::ExprGenPayload CodeGen::codegenLiteralUn(Token::Oper op, LiteralVal lit) {
    ExprGenPayload exprRet;
    exprRet.litVal.type = lit.type;
    if (op == Token::O_ADD) {
        if (lit.type != LiteralVal::T_SINT && lit.type != LiteralVal::T_FLOAT) {
            panic = true;
            return {};
        }
        exprRet.litVal = lit;
    } else if (op == Token::O_SUB) {
        if (lit.type == LiteralVal::T_SINT) {
            exprRet.litVal.val_si = -lit.val_si;
        } else if (lit.type == LiteralVal::T_FLOAT) {
            exprRet.litVal.val_f = -lit.val_f;
        } else {
            panic = true;
            return {};
        }
    } else if (op == Token::O_BIT_NOT) {
        if (lit.type == LiteralVal::T_SINT) {
            exprRet.litVal.val_si = ~lit.val_si;
        } else {
            panic = true;
            return {};
        }
    } else if (op == Token::O_NOT) {
        if (lit.type == LiteralVal::T_BOOL) {
            exprRet.litVal.val_b = !lit.val_b;
        } else {
            panic = true;
            return {};
        }
    } else {
        panic = true;
        return {};
    }
    return exprRet;
}

CodeGen::ExprGenPayload CodeGen::codegen(const BinExprAST *ast) {
    if (ast->getOp() == Token::O_AND || ast->getOp() == Token::O_OR) {
        return codegenLogicAndOr(ast);
    }

    ExprGenPayload exprPayL, exprPayR, exprPayRet;

    bool assignment = operInfos.at(ast->getOp()).assignment;

    exprPayL = codegenExpr(ast->getL());
    if (assignment) {
        if (refBroken(exprPayL)) return {};
    } else {
        if (valueBroken(exprPayL)) return {};
    }

    exprPayR = codegenExpr(ast->getR());
    if (valueBroken(exprPayR)) return {};

    if (exprPayL.isLitVal() && !exprPayR.isLitVal()) {
        if (!promoteLiteral(exprPayL, exprPayR.type)) return {};
    } else if (!exprPayL.isLitVal() && exprPayR.isLitVal()) {
        if (!promoteLiteral(exprPayR, exprPayL.type)) return {};
    } else if (exprPayL.isLitVal() && exprPayR.isLitVal()) {
        return codegenLiteralBin(ast->getOp(), exprPayL.litVal, exprPayR.litVal);
    }

    llvm::Value *valL = exprPayL.val, *valR = exprPayR.val;
    exprPayRet.type = exprPayL.type;
    exprPayRet.val = nullptr;
    exprPayRet.ref = assignment ? exprPayL.ref : nullptr;

    if (exprPayL.type != exprPayR.type) {
        if (TypeTable::isImplicitCastable(exprPayR.type, exprPayL.type)) {
            createCast(valR, exprPayR.type, exprPayL.type);
            exprPayRet.type = exprPayL.type;
        } else if (TypeTable::isImplicitCastable(exprPayL.type, exprPayR.type) && !assignment) {
            createCast(valL, exprPayL.type, exprPayR.type);
            exprPayRet.type = exprPayR.type;
        } else {
            panic = true;
            return {};
        }
    }

    if (TypeTable::isTypeB(exprPayRet.type)) {
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
        bool isTypeI = TypeTable::isTypeI(exprPayRet.type);
        bool isTypeU = TypeTable::isTypeU(exprPayRet.type);
        bool isTypeF = TypeTable::isTypeF(exprPayRet.type);
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
                else if (isTypeI || isTypeU)
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
                else if (isTypeI || isTypeU)
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
                else if (isTypeU)
                    exprPayRet.val = llvmBuilder.CreateICmpULT(valL, valR, "ucmp_lt_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_LTEQ:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpOLE(valL, valR, "fcmp_lteq_tmp");
                else if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateICmpSLE(valL, valR, "scmp_lteq_tmp");
                else if (isTypeU)
                    exprPayRet.val = llvmBuilder.CreateICmpULE(valL, valR, "ucmp_lteq_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_GT:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpOGT(valL, valR, "fcmp_gt_tmp");
                else if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateICmpSGT(valL, valR, "scmp_gt_tmp");
                else if (isTypeU)
                    exprPayRet.val = llvmBuilder.CreateICmpUGT(valL, valR, "ucmp_gt_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_GTEQ:
                if (isTypeF)
                    exprPayRet.val = llvmBuilder.CreateFCmpOGE(valL, valR, "fcmp_gteq_tmp");
                else if (isTypeI)
                    exprPayRet.val = llvmBuilder.CreateICmpSGE(valL, valR, "scmp_gteq_tmp");
                else if (isTypeU)
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

CodeGen::ExprGenPayload CodeGen::codegenLogicAndOr(const BinExprAST *ast) {
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
    if (valueBroken(exprPayL) || !exprPayL.isBool()) {
        panic = true;
        return {};
    }

    if (ast->getOp() == Token::O_AND) {
        if (exprPayL.isLitVal()) {
            llvmBuilder.CreateBr(exprPayL.litVal.val_b ? otherBlock : afterBlock);
        } else {
            llvmBuilder.CreateCondBr(exprPayL.val, otherBlock, afterBlock);
        }
    } else if (ast->getOp() == Token::O_OR) {
        if (exprPayL.isLitVal()) {
            llvmBuilder.CreateBr(exprPayL.litVal.val_b ? afterBlock : otherBlock);
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
    if (valueBroken(exprPayR) || !exprPayR.isBool()) {
        panic = true;
        return {};
    }
    llvmBuilder.CreateBr(afterBlock);
    otherBlock = llvmBuilder.GetInsertBlock();

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
    ExprGenPayload ret;
    if (exprPayL.isLitVal() && exprPayR.isLitVal()) {
        // this cannot be moved to other codegen methods,
        // as we don't know whether exprs are litVals until we call codegenExpr,
        // but calling it emits code to LLVM at that point
        ret.litVal.type = LiteralVal::T_BOOL;
        if (ast->getOp() == Token::O_AND)
            ret.litVal.val_b = exprPayL.litVal.val_b && exprPayR.litVal.val_b;
        else
            ret.litVal.val_b = exprPayL.litVal.val_b || exprPayR.litVal.val_b;
    } else {
        llvm::PHINode *phi = llvmBuilder.CreatePHI(getType(TypeTable::P_BOOL), 2, "logic_tmp");

        if (ast->getOp() == Token::O_AND)
            phi->addIncoming(getConstB(false), firstBlock);
        else
            phi->addIncoming(getConstB(true), firstBlock);
        
        if (exprPayR.isLitVal()) {
            phi->addIncoming(getConstB(exprPayR.litVal.val_b), otherBlock);
        } else {
            phi->addIncoming(exprPayR.val, otherBlock);
        }

        ret.type = TypeTable::P_BOOL;
        ret.val = phi;
    }

    return ret;
}

CodeGen::ExprGenPayload CodeGen::codegenLogicAndOrGlobalScope(const BinExprAST *ast) {
    ExprGenPayload exprPayL = codegenExpr(ast->getL());
    if (!exprPayL.isLitVal() || !exprPayL.isBool()) {
        panic = true;
        return {};
    }

    ExprGenPayload exprPayR = codegenExpr(ast->getR());
    if (!exprPayR.isLitVal() || !exprPayR.isBool()) {
        panic = true;
        return {};
    }

    ExprGenPayload exprPayRet;
    exprPayRet.litVal.type = LiteralVal::T_BOOL;
    if (ast->getOp() == Token::O_AND)
        exprPayRet.litVal.val_b = exprPayL.litVal.val_b && exprPayR.litVal.val_b;
    else
        exprPayRet.litVal.val_b = exprPayL.litVal.val_b || exprPayR.litVal.val_b;

    return exprPayRet;
}

CodeGen::ExprGenPayload CodeGen::codegenLiteralBin(Token::Oper op, LiteralVal litL, LiteralVal litR) {
    if (litL.type != litR.type || litL.type == LiteralVal::T_NONE) {
        panic = true;
        return {};
    }

    LiteralVal litValRet;
    litValRet.type = litL.type;

    if (litValRet.type == LiteralVal::T_BOOL) {
        switch (op) {
        case Token::O_EQ:
            litValRet.val_b = litL.val_b == litR.val_b;
            break;
        case Token::O_NEQ:
            litValRet.val_b = litL.val_b != litR.val_b;
            break;
        // AND and OR handled with the non-litVal cases
        default:
            panic = true;
            break;
        }
    } else if (litValRet.type == LiteralVal::T_NULL) {
        // both are null
        switch (op) {
        case Token::O_EQ:
            litValRet.type = LiteralVal::T_BOOL;
            litValRet.val_b = true;
            break;
        case Token::O_NEQ:
            litValRet.type = LiteralVal::T_BOOL;
            litValRet.val_b = false;
            break;
        default:
            panic = true;
            break;
        }
    } else {
        bool isTypeI = litValRet.type == LiteralVal::T_SINT;
        bool isTypeF = litValRet.type == LiteralVal::T_FLOAT;

        switch (op) {
            case Token::O_ADD:
                if (isTypeF)
                    litValRet.val_f = litL.val_f+litR.val_f;
                else if (isTypeI)
                    litValRet.val_si = litL.val_si+litR.val_si;
                break;
            case Token::O_SUB:
                if (isTypeF)
                    litValRet.val_f = litL.val_f-litR.val_f;
                else if (isTypeI)
                    litValRet.val_si = litL.val_si-litR.val_si;
                break;
            case Token::O_SHL:
                if (isTypeI)
                    litValRet.val_si = litL.val_si<<litR.val_si;
                else
                    panic = true;
                break;
            case Token::O_SHR:
                if (isTypeI)
                    litValRet.val_si = litL.val_si>>litR.val_si;
                else
                    panic = true;
                break;
            case Token::O_BIT_AND:
                if (isTypeI)
                    litValRet.val_si = litL.val_si&litR.val_si;
                else
                    panic = true;
                break;
            case Token::O_BIT_XOR:
                if (isTypeI)
                    litValRet.val_si = litL.val_si^litR.val_si;
                else
                    panic = true;
                break;
            case Token::O_BIT_OR:
                if (isTypeI)
                    litValRet.val_si = litL.val_si|litR.val_si;
                else
                    panic = true;
                break;
            case Token::O_MUL:
                if (isTypeF)
                    litValRet.val_f = litL.val_f*litR.val_f;
                else if (isTypeI)
                    litValRet.val_si = litL.val_si*litR.val_si;
                break;
            case Token::O_DIV:
                if (isTypeF)
                    litValRet.val_f = litL.val_f/litR.val_f;
                else if (isTypeI)
                    litValRet.val_si = litL.val_si/litR.val_si;
                break;
            case Token::O_REM:
                if (isTypeF)
                    litValRet.val_f = fmod(litL.val_f, litR.val_f);
                else if (isTypeI)
                    litValRet.val_si = litL.val_si%litR.val_si;
                break;
            case Token::O_EQ:
                litValRet.type = LiteralVal::T_BOOL;
                if (isTypeF)
                    litValRet.val_b = litL.val_f == litR.val_f;
                else if (isTypeI)
                    litValRet.val_b = litL.val_si == litR.val_si;
                break;
            case Token::O_NEQ:
                litValRet.type = LiteralVal::T_BOOL;
                if (isTypeF)
                    litValRet.val_b = litL.val_f != litR.val_f;
                else if (isTypeI)
                    litValRet.val_b = litL.val_si != litR.val_si;
                break;
            case Token::O_LT:
                litValRet.type = LiteralVal::T_BOOL;
                if (isTypeF)
                    litValRet.val_b = litL.val_f < litR.val_f;
                else if (isTypeI)
                    litValRet.val_b = litL.val_si < litR.val_si;
                break;
            case Token::O_LTEQ:
                litValRet.type = LiteralVal::T_BOOL;
                if (isTypeF)
                    litValRet.val_b = litL.val_f <= litR.val_f;
                else if (isTypeI)
                    litValRet.val_b = litL.val_si <= litR.val_si;
                break;
            case Token::O_GT:
                litValRet.type = LiteralVal::T_BOOL;
                if (isTypeF)
                    litValRet.val_b = litL.val_f > litR.val_f;
                else if (isTypeI)
                    litValRet.val_b = litL.val_si > litR.val_si;
                break;
            case Token::O_GTEQ:
                litValRet.type = LiteralVal::T_BOOL;
                if (isTypeF)
                    litValRet.val_b = litL.val_f >= litR.val_f;
                else if (isTypeI)
                    litValRet.val_b = litL.val_si >= litR.val_si;
                break;
            default:
                panic = true;
        }
    }

    if (panic) return {};
    return { .litVal = litValRet };
}

CodeGen::ExprGenPayload CodeGen::codegen(const TernCondExprAST *ast) {
    if (isGlobalScope()) {
        return codegenGlobalScope(ast);
    }
    
    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (valueBroken(condExpr) || !condExpr.isBool()) {
        panic = true;
        return {};
    }

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *trueBlock = llvm::BasicBlock::Create(llvmContext, "true", func);
    llvm::BasicBlock *falseBlock = llvm::BasicBlock::Create(llvmContext, "else");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    if (condExpr.isLitVal()) {
        llvmBuilder.CreateBr(condExpr.litVal.val_b ? trueBlock : falseBlock);
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

    if (!trueExpr.isLitVal() && !falseExpr.isLitVal()) {
        if (trueExpr.type != falseExpr.type) {
            if (TypeTable::isImplicitCastable(trueExpr.type, falseExpr.type)) {
                llvmBuilder.SetInsertPoint(trueBlock);
                createCast(trueExpr, falseExpr.type);
                trueBlock = llvmBuilder.GetInsertBlock();
            } else if (TypeTable::isImplicitCastable(falseExpr.type, trueExpr.type)) {
                llvmBuilder.SetInsertPoint(falseBlock);
                createCast(falseExpr, trueExpr.type);
                falseBlock = llvmBuilder.GetInsertBlock();
            } else {
                panic = true;
                return {};
            }
        }
    } else if (trueExpr.isLitVal() && !falseExpr.isLitVal()) {
        if (!promoteLiteral(trueExpr, falseExpr.type)) return {};
    } else if (!trueExpr.isLitVal() && falseExpr.isLitVal()) {
        if (!promoteLiteral(falseExpr, trueExpr.type)) return {};
    } else if (trueExpr.isLitVal() && falseExpr.isLitVal()) {
        if (trueExpr.litVal.type != falseExpr.litVal.type) {
            panic = true;
            return {};
        }

        // if all three litVals, we will return a litVal, handled below in function
        if (!condExpr.isLitVal()) {
            if (trueExpr.litVal.type == LiteralVal::T_BOOL) {
                if (!promoteLiteral(trueExpr, TypeTable::P_BOOL) ||
                    !promoteLiteral(falseExpr, TypeTable::P_BOOL))
                    return {};
            } else if (trueExpr.litVal.type == LiteralVal::T_SINT) {
                TypeTable::Id trueT = TypeTable::shortestFittingTypeI(trueExpr.litVal.val_si);
                TypeTable::Id falseT = TypeTable::shortestFittingTypeI(falseExpr.litVal.val_si);

                if (TypeTable::isImplicitCastable(trueT, falseT)) {
                    if (!promoteLiteral(trueExpr, falseT) ||
                        !promoteLiteral(falseExpr, falseT))
                        return {};
                } else {
                    if (!promoteLiteral(trueExpr, trueT) ||
                        !promoteLiteral(falseExpr, trueT))
                        return {};
                }
            } else if (trueExpr.litVal.type == LiteralVal::T_FLOAT) {
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
            } else if (trueExpr.litVal.type == LiteralVal::T_NULL) {
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
    if (condExpr.isLitVal()) {
        if (trueExpr.isLitVal()/* && falseExpr.isLitVal()*/) {
            ret.litVal.type = trueExpr.litVal.type;
            if (ret.litVal.type == LiteralVal::T_BOOL)
                ret.litVal.val_b = condExpr.litVal.val_b ? trueExpr.litVal.val_b : falseExpr.litVal.val_b;
            else if (ret.litVal.type == LiteralVal::T_SINT)
                ret.litVal.val_si = condExpr.litVal.val_b ? trueExpr.litVal.val_si : falseExpr.litVal.val_si;
            else if (ret.litVal.type == LiteralVal::T_FLOAT)
                ret.litVal.val_f = condExpr.litVal.val_b ? trueExpr.litVal.val_f : falseExpr.litVal.val_f;
            else if (ret.litVal.type == LiteralVal::T_NULL)
                ; //ret.litVal.type = LiteralVal::T_NULL;
            else {
                panic = true;
                return {};
            }
        } else/* both not litVals */ {
            ret.type = trueExpr.type;
            ret.val = condExpr.litVal.val_b ? trueExpr.val : falseExpr.val;
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

CodeGen::ExprGenPayload CodeGen::codegenGlobalScope(const TernCondExprAST *ast) {
    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (!condExpr.isLitVal() || !condExpr.isBool()) {
        panic = true;
        return {};
    }

    ExprGenPayload trueExpr = codegenExpr(ast->getOp1());
    if (!trueExpr.isLitVal()) return {};

    ExprGenPayload falseExpr = codegenExpr(ast->getOp2());
    if (!falseExpr.isLitVal()) return {};

    if (trueExpr.litVal.type != falseExpr.litVal.type) {
        panic = true;
        return {};
    }

    ExprGenPayload ret;
    ret.litVal.type = trueExpr.litVal.type;
    if (ret.litVal.type == LiteralVal::T_BOOL)
        ret.litVal.val_b = condExpr.litVal.val_b ? trueExpr.litVal.val_b : falseExpr.litVal.val_b;
    else if (ret.litVal.type == LiteralVal::T_SINT)
        ret.litVal.val_si = condExpr.litVal.val_b ? trueExpr.litVal.val_si : falseExpr.litVal.val_si;
    else if (ret.litVal.type == LiteralVal::T_FLOAT)
        ret.litVal.val_f = condExpr.litVal.val_b ? trueExpr.litVal.val_f : falseExpr.litVal.val_f;
    else if (ret.litVal.type == LiteralVal::T_NULL)
        ; //ret.litVal.type = LiteralVal::T_NULL;
    else {
        panic = true;
        return {};
    }

    return ret;
}

CodeGen::ExprGenPayload CodeGen::codegen(const CallExprAST *ast) {
    FuncSignature sig;
    sig.name = ast->getName();
    sig.argTypes = vector<TypeTable::Id>(ast->getArgs().size());

    vector<llvm::Value*> args(ast->getArgs().size());
    vector<ExprGenPayload> exprs(args.size());
    vector<LiteralVal> litVals(args.size());

    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        exprs[i] = codegenExpr(ast->getArgs()[i].get());
        if (valueBroken(exprs[i])) return {};

        sig.argTypes[i] = exprs[i].type;
        args[i] = exprs[i].val;
        litVals[i] = exprs[i].litVal;
    }

    pair<const FuncSignature*, const FuncValue*> func = symbolTable->getFuncCastsAllowed(sig, litVals.data());
    if (broken(func.second)) return {};

    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        if (exprs[i].isLitVal()) {
            // this also checks whether sint/uint literals fit into the arg type size
            if (!promoteLiteral(exprs[i], func.first->argTypes[i])) return {};
            args[i] = exprs[i].val;
        } else if (sig.argTypes[i] != func.first->argTypes[i]) {
            createCast(args[i], sig.argTypes[i], func.first->argTypes[i]);
        }
    }

    // reminder, it's lvalue if returning a lvalue (by ref)
    return {func.second->retType, llvmBuilder.CreateCall(func.second->func, args, 
        func.second->hasRet ? "call_tmp" : ""), nullptr};
}

CodeGen::ExprGenPayload CodeGen::codegen(const CastExprAST *ast) {
    llvm::Type *type = codegenType(ast->getType());
    if (broken(type)) return {};

    ExprGenPayload exprVal = codegenExpr(ast->getVal());
    if (valueBroken(exprVal)) return {};

    if (exprVal.isLitVal()) {
        TypeTable::Id promoType;
        switch (exprVal.litVal.type) {
        case LiteralVal::T_BOOL:
            promoType = TypeTable::P_BOOL;
            break;
        case LiteralVal::T_SINT:
            promoType = TypeTable::shortestFittingTypeI(exprVal.litVal.val_si);
            break;
        case LiteralVal::T_FLOAT:
            // cast to widest float type
            promoType = TypeTable::WIDEST_F;
            break;
        case LiteralVal::T_NULL:
            promoType = TypeTable::P_PTR;
            break;
        default:
            panic = true;
            return {};
        }
        if (!promoteLiteral(exprVal, promoType)) return {};
    }
    
    llvm::Value *val = exprVal.val;
    createCast(val, exprVal.type, type, ast->getType()->getTypeId());

    if (val == nullptr) panic = true;
    return {ast->getType()->getTypeId(), val, nullptr};
}