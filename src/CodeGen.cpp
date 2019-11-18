#include "Codegen.h"
#include "Parser.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Verifier.h"
using namespace std;

CodeGen::CodeGen(NamePool *namePool, SymbolTable *symbolTable) : namePool(namePool), symbolTable(symbolTable), 
        llvmBuilder(llvmContext), llvmBuilderAlloca(llvmContext), panic(false) {
    llvmModule = std::make_unique<llvm::Module>(llvm::StringRef("test"), llvmContext);
}

llvm::Value* CodeGen::getConstB(bool val) {
    if (val) return llvm::ConstantInt::getTrue(llvmContext);
    else return llvm::ConstantInt::getFalse(llvmContext);
}

llvm::Type* CodeGen::genPrimTypeBool() {
    return llvm::IntegerType::get(llvmContext, 1);
}

llvm::Type* CodeGen::genPrimTypeI(unsigned bits) {
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* CodeGen::genPrimTypeU(unsigned bits) {
    // LLVM makes no distinction between signed and unsigned int
    return llvm::IntegerType::get(llvmContext, bits);
}

llvm::Type* CodeGen::genPrimTypeF16() {
    return llvm::Type::getHalfTy(llvmContext);
}

llvm::Type* CodeGen::genPrimTypeF32() {
    return llvm::Type::getFloatTy(llvmContext);
}

llvm::Type* CodeGen::genPrimTypeF64() {
    return llvm::Type::getDoubleTy(llvmContext);
}

llvm::AllocaInst* CodeGen::createAlloca(llvm::Type *type, const string &name) {
    return llvmBuilderAlloca.CreateAlloca(type, 0, name);
}

bool CodeGen::isBlockTerminated() const {
    return !llvmBuilder.GetInsertBlock()->empty() && llvmBuilder.GetInsertBlock()->back().isTerminator();
}

llvm::GlobalValue* CodeGen::createGlobal(llvm::Type *type, const std::string &name) {
    return new llvm::GlobalVariable(
        *llvmModule.get(),
        type,
        false,
        llvm::GlobalValue::CommonLinkage,
        nullptr,
        name);
}

void CodeGen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId) {
    if (srcTypeId == dstTypeId) return;

    // TODO test the casts
    // yes, all of them
    if (TypeTable::isTypeI(srcTypeId)) {
        if (TypeTable::isTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, true, "i2i_cast");
        else if (TypeTable::isTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "i2u_cast");
        else if (TypeTable::isTypeF(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::SIToFP, val, type, "i2f_cast"));
        else if (dstTypeId == TypeTable::P_BOOL) {
            llvm::Value *z = llvmBuilder.CreateIntCast(getConstB(false), val->getType(), true);
            val = llvmBuilder.CreateICmpNE(val, z, "i2b_cast");
        } else {
            panic = true;
            val = nullptr;
        }
    } else if (TypeTable::isTypeU(srcTypeId)) {
        if (TypeTable::isTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, true, "u2i_cast");
        else if (TypeTable::isTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "u2u_cast");
        else if (TypeTable::isTypeF(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::UIToFP, val, type, "u2f_cast"));
        else if (dstTypeId == TypeTable::P_BOOL) {
            llvm::Value *z = llvmBuilder.CreateIntCast(getConstB(false), val->getType(), false);
            val = llvmBuilder.CreateICmpNE(val, z, "i2b_cast");
        } else {
            panic = true;
            val = nullptr;
        }
    } else if (TypeTable::isTypeF(srcTypeId)) {
        if (TypeTable::isTypeI(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToSI, val, type, "f2i_cast"));
        else if (TypeTable::isTypeU(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToUI, val, type, "f2u_cast"));
        else if (TypeTable::isTypeF(dstTypeId))
            val = llvmBuilder.CreateFPCast(val, type, "f2f_cast");
        else {
            panic = true;
            val = nullptr;
        }
    } else if (srcTypeId == TypeTable::P_BOOL) {
        if (TypeTable::isTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "b2i_cast");
        else if (TypeTable::isTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "b2u_cast");
        else {
            panic = true;
            val = nullptr;
        }
    } else {
        panic = true;
        val = nullptr;
    }
}

void CodeGen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    createCast(val, srcTypeId, symbolTable->getTypeTable()->getType(dstTypeId), dstTypeId);
}

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
    case AST_CallExpr:
        return codegen((const CallExprAST*)ast);
    case AST_CastExpr:
        return codegen((const CastExprAST*)ast);
    default:
        panic = true;
        return {};
    }
}

void CodeGen::codegenNode(const BaseAST *ast, bool blockMakeScope) {
    switch (ast->type()) {
    case AST_NullExpr:
        return;
    case AST_Decl:
        codegen((const DeclAST*)ast);
        return;
    case AST_If:
        codegen((const IfAST*)ast);
        return;
    case AST_For:
        codegen((const ForAST*) ast);
        return;
    case AST_While:
        codegen((const WhileAST*) ast);
        return;
    case AST_DoWhile:
        codegen((const DoWhileAST*) ast);
        return;
    case AST_Ret:
        codegen((const RetAST*)ast);
        return;
    case AST_Block:
        codegen((const BlockAST*)ast, blockMakeScope);
        return;
    case AST_FuncProto:
        codegen((const FuncProtoAST*)ast, false);
        return;
    case AST_Func:
        codegen((const FuncAST*)ast);
        return;
    default:
        codegenExpr((const ExprAST*)ast);
    }
}

CodeGen::ExprGenPayload CodeGen::codegen(const LiteralExprAST *ast) {
    // TODO literals of other types
    if (TypeTable::isTypeI(ast->getType())) {
        return {
            ast->getType(),
            llvm::ConstantInt::get(
                symbolTable->getTypeTable()->getType(ast->getType()), 
                ast->getValI(), true),
            nullptr
        };
    } else if (ast->getType() == TypeTable::P_BOOL) {
        return {
            ast->getType(),
            ast->getValB() ? getConstB(true) : getConstB(false),
            nullptr
        };
    } else {
        panic = true;
        return {};
    }
}

CodeGen::ExprGenPayload CodeGen::codegen(const VarExprAST *ast) {
    const SymbolTable::VarPayload *var = symbolTable->getVar(ast->getNameId());
    if (broken(var)) return {};
    return {var->type, llvmBuilder.CreateLoad(var->val, namePool->get(ast->getNameId())), var->val};
}

CodeGen::ExprGenPayload CodeGen::codegen(const UnExprAST *ast) {
    ExprGenPayload exprPay = codegenExpr(ast->getExpr());
    if (panic || exprPay.ref == nullptr) {
        panic = true;
        return {};
    }

    if (!(TypeTable::isTypeI(exprPay.type) || TypeTable::isTypeU(exprPay.type))) {
        panic = true;
        return {};
    }

    ExprGenPayload exprRet;
    exprRet.type = exprPay.type;
    if (ast->getOp() == Token::O_INC) {
        exprRet.val = llvmBuilder.CreateAdd(exprPay.val, llvm::ConstantInt::get(symbolTable->getTypeTable()->getType(exprPay.type), 1), "inc");
        exprRet.ref = exprPay.ref;
        llvmBuilder.CreateStore(exprRet.val, exprRet.ref);
    } else if (ast->getOp() == Token::O_DEC) {
        exprRet.val = llvmBuilder.CreateSub(exprPay.val, llvm::ConstantInt::get(symbolTable->getTypeTable()->getType(exprPay.type), 1), "dec");
        exprRet.ref = exprPay.ref;
        llvmBuilder.CreateStore(exprRet.val, exprRet.ref);
    } else {
        panic = true;
        return {};
    }
    return exprRet;
}

CodeGen::ExprGenPayload CodeGen::codegen(const BinExprAST *ast) {
    ExprGenPayload exprPayL, exprPayR, exprPayRet;

    if (ast->getOp() == Token::O_ASGN) {
        exprPayL = codegenExpr(ast->getL());
        if (panic || exprPayL.ref == nullptr) {
            panic = true;
            return {};
        }
    } else {
        exprPayL = codegenExpr(ast->getL());
        if (broken(exprPayL.val)) return {};
    }

    exprPayR = codegenExpr(ast->getR());
    if (broken(exprPayR.val)) return {};

    llvm::Value *valL = exprPayL.val, *valR = exprPayR.val;
    exprPayRet.type = exprPayL.type;
    exprPayRet.ref = nullptr;

    if (exprPayL.type != exprPayR.type) {
        if (TypeTable::isImplicitCastable(exprPayR.type, exprPayL.type)) {
            createCast(valR, exprPayR.type, exprPayL.type);
            exprPayRet.type = exprPayL.type;
        } else if (TypeTable::isImplicitCastable(exprPayL.type, exprPayR.type) && ast->getOp() != Token::O_ASGN) {
            createCast(valL, exprPayL.type, exprPayR.type);
            exprPayRet.type = exprPayR.type;
        } else {
            panic = true;
            return {};
        }
    }

    if (exprPayRet.type == TypeTable::P_BOOL) {
        // TODO moah bool operations
        switch (ast->getOp()) {
        case Token::O_ASGN:
            llvmBuilder.CreateStore(valR, exprPayL.ref);
            exprPayRet.val = valR;
            exprPayRet.ref = exprPayL.ref;
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
        switch (ast->getOp()) {
            case Token::O_ASGN:
                llvmBuilder.CreateStore(valR, exprPayL.ref);
                exprPayRet.val = valR;
                exprPayRet.ref = exprPayL.ref;
                break;
            case Token::O_ADD:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFAdd(valL, valR, "fadd_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateAdd(valL, valR, "add_tmp");
                break;
            case Token::O_SUB:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFSub(valL, valR, "fsub_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateSub(valL, valR, "sub_tmp");
                break;
            case Token::O_MUL:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFMul(valL, valR, "fmul_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateMul(valL, valR, "mul_tmp");
                break;
            case Token::O_DIV:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFDiv(valL, valR, "fdiv_tmp");
                else if (TypeTable::isTypeI(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateSDiv(valL, valR, "sdiv_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateUDiv(valL, valR, "udiv_tmp");
                break;
            case Token::O_EQ:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFCmpOEQ(valL, valR, "fcmp_eq_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateICmpEQ(valL, valR, "cmp_eq_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_NEQ:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFCmpONE(valL, valR, "fcmp_neq_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateICmpNE(valL, valR, "cmp_neq_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_LT:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFCmpOLT(valL, valR, "fcmp_lt_tmp");
                else if (TypeTable::isTypeI(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateICmpSLT(valL, valR, "scmp_lt_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateICmpULT(valL, valR, "ucmp_lt_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_LTEQ:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFCmpOLE(valL, valR, "fcmp_lteq_tmp");
                else if (TypeTable::isTypeI(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateICmpSLE(valL, valR, "scmp_lteq_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateICmpULE(valL, valR, "ucmp_lteq_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_GT:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFCmpOGT(valL, valR, "fcmp_gt_tmp");
                else if (TypeTable::isTypeI(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateICmpSGT(valL, valR, "scmp_gt_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateICmpUGT(valL, valR, "ucmp_gt_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            case Token::O_GTEQ:
                if (TypeTable::isTypeF(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateFCmpOGE(valL, valR, "fcmp_gteq_tmp");
                else if (TypeTable::isTypeI(exprPayRet.type))
                    exprPayRet.val = llvmBuilder.CreateICmpSGE(valL, valR, "scmp_gteq_tmp");
                else
                    exprPayRet.val = llvmBuilder.CreateICmpUGE(valL, valR, "ucmp_gteq_tmp");
                exprPayRet.type = TypeTable::P_BOOL;
                break;
            default:
                panic = true;
                return {};
        }
    }

    return exprPayRet;
}

CodeGen::ExprGenPayload CodeGen::codegen(const CallExprAST *ast) {
    FuncSignature sig;
    sig.name = ast->getName();
    sig.argTypes = vector<TypeTable::Id>(ast->getArgs().size());

    std::vector<llvm::Value*> args(ast->getArgs().size());
    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        ExprGenPayload exprPay = codegenExpr(ast->getArgs()[i].get());

        sig.argTypes[i] = exprPay.type;

        llvm::Value *arg = exprPay.val;
        if (broken(arg)) return {};
        args[i] = arg;
    }

    pair<const FuncSignature*, const FuncValue*> func = symbolTable->getFuncImplicitCastsAllowed(sig);
    if (broken(func.second)) return {};

    for (size_t i = 0; i < ast->getArgs().size(); ++i) {
        if (sig.argTypes[i] != func.first->argTypes[i]) {
            createCast(args[i], sig.argTypes[i], func.first->argTypes[i]);
        }
    }

    // TODO void-y type if func has no ret
    // is lvalue if returning a lvalue (by ref)
    return {func.second->retType, llvmBuilder.CreateCall(func.second->func, args, "call_tmp"), nullptr};
}

CodeGen::ExprGenPayload CodeGen::codegen(const CastExprAST *ast) {
    llvm::Type *type = codegenType(ast->getType());
    if (broken(type)) return {};

    ExprGenPayload exprVal = codegenExpr(ast->getVal());
    if (broken(exprVal.val)) return {};

    llvm::Value *val = exprVal.val;
    createCast(val, exprVal.type, type, ast->getType()->getTypeId());

    if (val == nullptr) panic = true;
    return {ast->getType()->getTypeId(), val, nullptr};
}

llvm::Type* CodeGen::codegenType(const TypeAST *ast) {
    llvm::Type *type = symbolTable->getTypeTable()->getType(ast->getTypeId());
    if (broken(type)) return {};

    return type;
}

void CodeGen::codegen(const DeclAST *ast) {
    TypeTable::Id typeId = ast->getType()->getTypeId();
    llvm::Type *type = codegenType(ast->getType());
    if (broken(type)) return;

    for (const auto &it : ast->getDecls()) {
        if (symbolTable->varNameTaken(it.first)) {
            panic = true;
            return;
        }

        const string &name = namePool->get(it.first);

        llvm::Value *val;
        if (symbolTable->inGlobalScope()) {
            val = createGlobal(type, name);

            if (it.second.get() != nullptr) {
                // TODO allow init global vars
                panic = true;
                return;
            }
        } else {
            val = createAlloca(type, name);

            const ExprAST *init = it.second.get();
            if (init != nullptr) {
                ExprGenPayload initPay = codegenExpr(init);
                if (panic || initPay.val == nullptr) {
                    panic = true;
                    return;
                }

                llvm::Value *src = initPay.val;

                if (initPay.type != typeId) {
                    if (!TypeTable::isImplicitCastable(initPay.type, typeId)) {
                        panic = true;
                        return;
                    }

                    createCast(src, initPay.type, type, typeId);
                }

                llvmBuilder.CreateStore(src, val);
            }
        }

        symbolTable->addVar(it.first, {typeId, val});
    }
}

void CodeGen::codegen(const IfAST *ast) {
    // unlike C++, then and else may eclipse vars declared in if's init
    ScopeControl scope(ast->hasInit() ? symbolTable : nullptr);

    if (ast->hasInit()) {
        codegenNode(ast->getInit());
        if (panic) return;
    }

    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (panic || condExpr.type != TypeTable::P_BOOL || condExpr.val == nullptr) {
        panic = true;
        return;
    }

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(llvmContext, "then", func);
    llvm::BasicBlock *elseBlock = ast->hasElse() ? llvm::BasicBlock::Create(llvmContext, "else") : nullptr;
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateCondBr(condExpr.val, thenBlock, ast->hasElse() ? elseBlock : afterBlock);

    {
        ScopeControl thenScope(symbolTable);
        llvmBuilder.SetInsertPoint(thenBlock);
        codegenNode(ast->getThen(), false);
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    if (ast->hasElse()) {
        ScopeControl elseScope(symbolTable);
        func->getBasicBlockList().push_back(elseBlock);
        llvmBuilder.SetInsertPoint(elseBlock);
        codegenNode(ast->getElse(), false);
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void CodeGen::codegen(const ForAST *ast) {
    ScopeControl scope(ast->getInit()->type() == AST_Decl ? symbolTable : nullptr);

    codegenNode(ast->getInit());
    if (panic) return;

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(llvmContext, "cond", func);
    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(condBlock);
    llvmBuilder.SetInsertPoint(condBlock);

    ExprGenPayload condExpr;
    if (ast->hasCond()) {
        condExpr = codegenExpr(ast->getCond());
        if (panic || condExpr.type != TypeTable::P_BOOL || condExpr.val == nullptr) {
            panic = true;
            return;
        }
    } else {
        condExpr.type = TypeTable::P_BOOL;
        condExpr.val = getConstB(true);
    }

    llvmBuilder.CreateCondBr(condExpr.val, bodyBlock, afterBlock);

    {
        ScopeControl scopeBody(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);

        codegenNode(ast->getBody(), false);
        if (panic) return;
    }
        
    if (ast->hasIter()) {
        codegenNode(ast->getIter());
        if (panic) return;
    }

    if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void CodeGen::codegen(const WhileAST *ast) {
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(llvmContext, "cond", func);
    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(condBlock);
    llvmBuilder.SetInsertPoint(condBlock);

    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (panic || condExpr.type != TypeTable::P_BOOL || condExpr.val == nullptr) {
        panic = true;
        return;
    }

    llvmBuilder.CreateCondBr(condExpr.val, bodyBlock, afterBlock);

    {
        ScopeControl scope(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);
        codegenNode(ast->getBody(), false);
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void CodeGen::codegen(const DoWhileAST *ast) {
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body", func);
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(bodyBlock);
    llvmBuilder.SetInsertPoint(bodyBlock);

    {
        ScopeControl scope(symbolTable);
        codegenNode(ast->getBody(), false);
        if (panic) return;
    }

    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (panic || condExpr.type != TypeTable::P_BOOL || condExpr.val == nullptr) {
        panic = true;
        return;
    }

    llvmBuilder.CreateCondBr(condExpr.val, bodyBlock, afterBlock);

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void CodeGen::codegen(const RetAST *ast) {
    const FuncValue *currFunc = symbolTable->getCurrFunc();
    if (currFunc == nullptr) {
        panic = true;
        return;
    }

    if (!ast->getVal()) {
        if (currFunc->hasRet) {
            panic = true;
            return;
        }
        llvmBuilder.CreateRetVoid();
        return;
    }

    ExprGenPayload condExpr = codegenExpr(ast->getVal());
    if (broken(condExpr.val)) return;

    llvm::Value *retVal = condExpr.val;
    if (condExpr.type != currFunc->retType) {
        if (!TypeTable::isImplicitCastable(condExpr.type, currFunc->retType)) {
            panic = true;
            return;
        }
        createCast(retVal, condExpr.type, currFunc->retType);
    }

    llvmBuilder.CreateRet(retVal);
}

void CodeGen::codegen(const BlockAST *ast, bool makeScope) {
    ScopeControl scope(makeScope ? symbolTable : nullptr);

    for (const auto &it : ast->getBody()) codegenNode(it.get());
}

FuncValue* CodeGen::codegen(const FuncProtoAST *ast, bool definition) {
    if (symbolTable->funcNameTaken(ast->getName())) {
        panic = true;
        return nullptr;
    }

    FuncSignature sig;
    sig.name = ast->getName();
    sig.argTypes = vector<TypeTable::Id>(ast->getArgCnt());
    for (size_t i = 0; i < ast->getArgCnt(); ++i) sig.argTypes[i] = ast->getArgType(i)->getTypeId();

    FuncValue *prev = symbolTable->getFunc(sig);

    if (prev != nullptr) {
        if ((prev->defined && definition) ||
            (prev->hasRet != ast->hasRetVal()) ||
            (prev->hasRet && prev->retType != ast->getRetType()->getTypeId())) {
            panic = true;
            return nullptr;
        }

        if (definition) {
            prev->defined = true;
        }

        return prev;
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < ast->getArgCnt(); ++i) {
        for (size_t j = i+1; j < ast->getArgCnt(); ++j) {
            if (ast->getArgName(i) == ast->getArgName(j)) {
                panic = true;
                return nullptr;
            }
        }
    }

    vector<llvm::Type*> argTypes(ast->getArgCnt());
    for (size_t i = 0; i < argTypes.size(); ++i)
        argTypes[i] = symbolTable->getTypeTable()->getType(ast->getArgType(i)->getTypeId());
    llvm::Type *retType = ast->hasRetVal() ? symbolTable->getTypeTable()->getType(ast->getRetType()->getTypeId()) : llvm::Type::getVoidTy(llvmContext);
    llvm::FunctionType *funcType = llvm::FunctionType::get(retType, argTypes, false);

    llvm::Function *func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
            namePool->get(ast->getName()), llvmModule.get());
    
    size_t i = 0;
    for (auto &arg : func->args()) {
        arg.setName(namePool->get(ast->getArgName(i)));
        ++i;
    }

    FuncValue val;
    val.func = func;
    val.hasRet = ast->hasRetVal();
    if (ast->hasRetVal()) val.retType = ast->getRetType()->getTypeId();
    val.defined = definition;

    symbolTable->addFunc(sig, val);
    // cannot return &val, as it is a local var
    return symbolTable->getFunc(sig);
}

void CodeGen::codegen(const FuncAST *ast) {
    FuncValue *funcVal = codegen(ast->getProto(), true);
    if (panic) {
        return;
    }

    ScopeControl scope(*symbolTable, *funcVal);

    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", funcVal->func));

    llvm::BasicBlock *body = llvm::BasicBlock::Create(llvmContext, "entry", funcVal->func);
    llvmBuilder.SetInsertPoint(body);

    size_t i = 0;
    for (auto &arg : funcVal->func->args()) {
        const TypeAST *astArgType = ast->getProto()->getArgType(i);
        NamePool::Id astArgName = ast->getProto()->getArgName(i);
        const string &name = namePool->get(astArgName);
        llvm::AllocaInst *alloca = createAlloca(arg.getType(), name);
        llvmBuilder.CreateStore(&arg, alloca);
        symbolTable->addVar(astArgName, {astArgType->getTypeId(), alloca});

        ++i;
    }

    codegen(ast->getBody(), false);
    if (panic) {
        funcVal->func->eraseFromParent();
        return;
    }

    llvmBuilderAlloca.CreateBr(body);

    if (!ast->getProto()->hasRetVal() && !isBlockTerminated())
            llvmBuilder.CreateRetVoid();

    cout << endl;
    cout << "LLVM func verification for " << namePool->get(ast->getProto()->getName()) << ":" << endl << endl;
    llvm::verifyFunction(*funcVal->func, &llvm::outs());
}

void CodeGen::printout() const {
    cout << "LLVM module printout:" << endl << endl;
    llvmModule->print(llvm::outs(), nullptr);
}