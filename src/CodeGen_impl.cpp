#include "Codegen.h"
#include "llvm/IR/Verifier.h"
using namespace std;

void CodeGen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId) {
    if (srcTypeId == dstTypeId) return;

    if (val == nullptr || type == nullptr) {
        panic = true;
        return;
    }

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

void CodeGen::codegenNode(const BaseAST *ast, bool blockMakeScope) {
    switch (ast->type()) {
    case AST_EmptyExpr:
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
    case AST_Break:
        codegen((const BreakAST*) ast);
        return;
    case AST_Continue:
        codegen((const ContinueAST*) ast);
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
        if (isGlobalScope()) {
            llvm::Constant *initConst = nullptr;

            const ExprAST *init = it.second.get();
            if (init != nullptr) {
                ExprGenPayload initPay = codegenExpr(init);
                if (valueBroken(initPay) || !initPay.isLitVal() || !promoteLiteral(initPay, typeId)) {
                    panic = true;
                    return;
                }
                initConst = (llvm::Constant*) initPay.val;
            }

            val = createGlobal(type, initConst, name);
        } else {
            val = createAlloca(type, name);

            const ExprAST *init = it.second.get();
            if (init != nullptr) {
                ExprGenPayload initPay = codegenExpr(init);
                if (valueBroken(initPay)) return;

                if (initPay.isLitVal()) {
                    if (!promoteLiteral(initPay, typeId)) return;
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
    if (condExpr.isLitVal() && !promoteLiteral(condExpr, TypeTable::P_BOOL)) {
        panic = true;
        return;
    }
    if (valBroken(condExpr) || condExpr.type != TypeTable::P_BOOL) {
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
    llvm::BasicBlock *iterBlock = llvm::BasicBlock::Create(llvmContext, "iter");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    continueStack.push(iterBlock);
    breakStack.push(afterBlock);

    llvmBuilder.CreateBr(condBlock);
    llvmBuilder.SetInsertPoint(condBlock);

    {
        ExprGenPayload condExpr;
        if (ast->hasCond()) {
            condExpr = codegenExpr(ast->getCond());
            if (condExpr.isLitVal() && !promoteLiteral(condExpr, TypeTable::P_BOOL)) {
                panic = true;
                return;
            }
            if (valBroken(condExpr) || condExpr.type != TypeTable::P_BOOL) {
                panic = true;
                return;
            }
        } else {
            condExpr.type = TypeTable::P_BOOL;
            condExpr.val = getConstB(true);
        }

        llvmBuilder.CreateCondBr(condExpr.val, bodyBlock, afterBlock);
    }

    {
        ScopeControl scopeBody(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);
        codegenNode(ast->getBody(), false);
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(iterBlock);
    }
    
    {
        func->getBasicBlockList().push_back(iterBlock);
        llvmBuilder.SetInsertPoint(iterBlock);

        if (ast->hasIter()) {
            codegenNode(ast->getIter());
            if (panic) return;
        }

        if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);

    breakStack.pop();
    continueStack.pop();
}

void CodeGen::codegen(const WhileAST *ast) {
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(llvmContext, "cond", func);
    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    continueStack.push(condBlock);
    breakStack.push(afterBlock);

    llvmBuilder.CreateBr(condBlock);
    llvmBuilder.SetInsertPoint(condBlock);

    {
        ExprGenPayload condExpr = codegenExpr(ast->getCond());
        if (condExpr.isLitVal() && !promoteLiteral(condExpr, TypeTable::P_BOOL)) {
            panic = true;
            return;
        }
        if (valBroken(condExpr) || condExpr.type != TypeTable::P_BOOL) {
            panic = true;
            return;
        }

        llvmBuilder.CreateCondBr(condExpr.val, bodyBlock, afterBlock);
    }

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

    breakStack.pop();
    continueStack.pop();
}

void CodeGen::codegen(const DoWhileAST *ast) {
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body", func);
    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(llvmContext, "cond");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    continueStack.push(condBlock);
    breakStack.push(afterBlock);

    llvmBuilder.CreateBr(bodyBlock);
    llvmBuilder.SetInsertPoint(bodyBlock);

    {
        ScopeControl scope(symbolTable);
        codegenNode(ast->getBody(), false);
        if (panic) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);
    }

    {
        func->getBasicBlockList().push_back(condBlock);
        llvmBuilder.SetInsertPoint(condBlock);

        ExprGenPayload condExpr = codegenExpr(ast->getCond());
        if (condExpr.isLitVal() && !promoteLiteral(condExpr, TypeTable::P_BOOL)) {
            panic = true;
            return;
        }
        if (valBroken(condExpr) || condExpr.type != TypeTable::P_BOOL) {
            panic = true;
            return;
        }

        llvmBuilder.CreateCondBr(condExpr.val, bodyBlock, afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);

    breakStack.pop();
    continueStack.pop();
}

void CodeGen::codegen(const BreakAST *ast) {
    if (breakStack.empty()) {
        panic = true;
        return;
    }

    llvmBuilder.CreateBr(breakStack.top());
}

void CodeGen::codegen(const ContinueAST *ast) {
    if (continueStack.empty()) {
        panic = true;
        return;
    }

    llvmBuilder.CreateBr(continueStack.top());
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

    ExprGenPayload retExpr = codegenExpr(ast->getVal());
    if (retExpr.isLitVal() && !promoteLiteral(retExpr, currFunc->retType)) return;
    if (valBroken(retExpr)) return;

    llvm::Value *retVal = retExpr.val;
    if (retExpr.type != currFunc->retType) {
        if (!TypeTable::isImplicitCastable(retExpr.type, currFunc->retType)) {
            panic = true;
            return;
        }
        createCast(retVal, retExpr.type, currFunc->retType);
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

    // check for definition clashes with existing functions
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
    if (broken(funcVal)) {
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

    if (llvm::verifyFunction(*funcVal->func, &llvm::errs())) cout << endl;
}