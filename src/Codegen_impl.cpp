#include "Codegen.h"
#include "llvm/IR/Verifier.h"
#include <unordered_set>
using namespace std;

void Codegen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId) {
    if (srcTypeId == dstTypeId) return;

    if (val == nullptr || type == nullptr) {
        panic = true;
        return;
    }

    // TODO warn if removing cn, may give undefined behaviour
    if (getTypeTable()->isTypeI(srcTypeId)) {
        if (getTypeTable()->isTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, true, "i2i_cast");
        else if (getTypeTable()->isTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "i2u_cast");
        else if (getTypeTable()->isTypeF(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::SIToFP, val, type, "i2f_cast"));
        else if (getTypeTable()->isTypeC(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "i2c_cast");
        else if (getTypeTable()->isTypeB(dstTypeId)) {
            llvm::Value *z = llvmBuilder.CreateIntCast(getConstB(false), val->getType(), true);
            val = llvmBuilder.CreateICmpNE(val, z, "i2b_cast");
        } else if (symbolTable->getTypeTable()->isTypeAnyP(dstTypeId)) {
            val = llvmBuilder.CreatePointerCast(val, type, "i2p_cast");
        } else {
            panic = true;
            val = nullptr;
        }
    } else if (getTypeTable()->isTypeU(srcTypeId)) {
        if (getTypeTable()->isTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, true, "u2i_cast");
        else if (getTypeTable()->isTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "u2u_cast");
        else if (getTypeTable()->isTypeF(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::UIToFP, val, type, "u2f_cast"));
        else if (getTypeTable()->isTypeC(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "u2c_cast");
        else if (getTypeTable()->isTypeB(dstTypeId)) {
            llvm::Value *z = llvmBuilder.CreateIntCast(getConstB(false), val->getType(), false);
            val = llvmBuilder.CreateICmpNE(val, z, "u2b_cast");
        } else if (symbolTable->getTypeTable()->isTypeAnyP(dstTypeId)) {
            val = llvmBuilder.CreatePointerCast(val, type, "u2p_cast");
        } else {
            panic = true;
            val = nullptr;
        }
    } else if (getTypeTable()->isTypeF(srcTypeId)) {
        if (getTypeTable()->isTypeI(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToSI, val, type, "f2i_cast"));
        else if (getTypeTable()->isTypeU(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToUI, val, type, "f2u_cast"));
        else if (getTypeTable()->isTypeF(dstTypeId))
            val = llvmBuilder.CreateFPCast(val, type, "f2f_cast");
        else {
            panic = true;
            val = nullptr;
        }
    } else if (getTypeTable()->isTypeC(srcTypeId)) {
        if (getTypeTable()->isTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, true, "c2i_cast");
        else if (getTypeTable()->isTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "c2u_cast");
        else if (getTypeTable()->isTypeC(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "c2c_cast");
        else if (getTypeTable()->isTypeB(dstTypeId)) {
            llvm::Value *z = llvmBuilder.CreateIntCast(getConstB(false), val->getType(), false);
            val = llvmBuilder.CreateICmpNE(val, z, "c2b_cast");
        } else {
            panic = true;
            val = nullptr;
        }
    } else if (getTypeTable()->isTypeB(srcTypeId)) {
        if (getTypeTable()->isTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "b2i_cast");
        else if (getTypeTable()->isTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "b2u_cast");
        else {
            panic = true;
            val = nullptr;
        }
    } else if (getTypeTable()->isTypeAnyP(srcTypeId)) {
        if (getTypeTable()->isTypeI(dstTypeId))
            val = llvmBuilder.CreatePtrToInt(val, type, "p2i_cast");
        else if (getTypeTable()->isTypeU(dstTypeId))
            val = llvmBuilder.CreatePtrToInt(val, type, "p2u_cast");
        else if (symbolTable->getTypeTable()->isTypeAnyP(dstTypeId))
            val = llvmBuilder.CreatePointerCast(val, type, "p2p_cast");
        else if (getTypeTable()->isTypeB(dstTypeId)) {
            val = llvmBuilder.CreateICmpNE(
                llvmBuilder.CreatePtrToInt(val, getType(TypeTable::WIDEST_I)),
                llvm::ConstantInt::get(getType(TypeTable::WIDEST_I), 0),
                "p2b_cast");
        } else {
            panic = true;
            val = nullptr;
        }
    } else if (getTypeTable()->isTypeArr(srcTypeId)) {
        // REM arrs are only castable when changing constness
        if (!getTypeTable()->isImplicitCastable(srcTypeId, dstTypeId)) {
            // no action is needed in case of a cast
            panic = true;
            val = nullptr;
        }
    } else {
        panic = true;
        val = nullptr;
    }
}

void Codegen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    createCast(val, srcTypeId, getType(dstTypeId), dstTypeId);
}

void Codegen::createCast(ExprGenPayload &e, TypeTable::Id t) {
    createCast(e.val, e.type, t);
    if (panic) return;
    e.type = t;
}

void Codegen::codegenNode(const BaseAst *ast, bool blockMakeScope) {
    switch (ast->type()) {
    case AST_EmptyExpr:
        return;
    case AST_Decl:
        codegen((const DeclAst*)ast);
        return;
    case AST_If:
        codegen((const IfAst*)ast);
        return;
    case AST_For:
        codegen((const ForAst*) ast);
        return;
    case AST_While:
        codegen((const WhileAst*) ast);
        return;
    case AST_DoWhile:
        codegen((const DoWhileAst*) ast);
        return;
    case AST_Break:
        codegen((const BreakAst*) ast);
        return;
    case AST_Continue:
        codegen((const ContinueAst*) ast);
        return;
    case AST_Switch:
        codegen ((const SwitchAst*) ast);
        return;
    case AST_Ret:
        codegen((const RetAst*)ast);
        return;
    case AST_Block:
        codegen((const BlockAst*)ast, blockMakeScope);
        return;
    case AST_FuncProto:
        codegen((const FuncProtoAst*)ast, false);
        return;
    case AST_Func:
        codegen((const FuncAst*)ast);
        return;
    default:
        codegenExpr((const ExprAst*)ast);
    }
}

llvm::Type* Codegen::codegenType(const TypeAst *ast) {
    llvm::Type *type = getType(ast->getTypeId());
    if (broken(type)) return nullptr;

    return type;
}

void Codegen::codegen(const DeclAst *ast) {
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

            const ExprAst *init = it.second.get();
            if (init != nullptr) {
                ExprGenPayload initPay = codegenExpr(init);
                if (valueBroken(initPay) || !initPay.isUntyVal() || !promoteUntyped(initPay, typeId)) {
                    panic = true;
                    return;
                }
                initConst = (llvm::Constant*) initPay.val;
            }

            val = createGlobal(type, initConst, getTypeTable()->isTypeCn(typeId), name);
        } else {
            val = createAlloca(type, name);

            const ExprAst *init = it.second.get();
            if (init != nullptr) {
                ExprGenPayload initPay = codegenExpr(init);
                if (valueBroken(initPay)) return;

                if (initPay.isUntyVal()) {
                    if (!promoteUntyped(initPay, typeId)) return;
                }

                llvm::Value *src = initPay.val;

                if (initPay.type != typeId) {
                    if (!getTypeTable()->isImplicitCastable(initPay.type, typeId)) {
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

void Codegen::codegen(const IfAst *ast) {
    // unlike C++, then and else may eclipse vars declared in if's init
    ScopeControl scope(ast->hasInit() ? symbolTable : nullptr);

    if (ast->hasInit()) {
        codegenNode(ast->getInit());
        if (panic) return;
    }

    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (condExpr.isUntyVal() && !promoteUntyped(condExpr, TypeTable::P_BOOL)) {
        panic = true;
        return;
    }
    if (valBroken(condExpr) || !getTypeTable()->isTypeB(condExpr.type)) {
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

void Codegen::codegen(const ForAst *ast) {
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
            if (condExpr.isUntyVal() && !promoteUntyped(condExpr, TypeTable::P_BOOL)) {
                panic = true;
                return;
            }
            if (valBroken(condExpr) || !getTypeTable()->isTypeB(condExpr.type)) {
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

void Codegen::codegen(const WhileAst *ast) {
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
        if (condExpr.isUntyVal() && !promoteUntyped(condExpr, TypeTable::P_BOOL)) {
            panic = true;
            return;
        }
        if (valBroken(condExpr) || !getTypeTable()->isTypeB(condExpr.type)) {
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

void Codegen::codegen(const DoWhileAst *ast) {
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
        if (condExpr.isUntyVal() && !promoteUntyped(condExpr, TypeTable::P_BOOL)) {
            panic = true;
            return;
        }
        if (valBroken(condExpr) || !getTypeTable()->isTypeB(condExpr.type)) {
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

void Codegen::codegen(const BreakAst *ast) {
    if (breakStack.empty()) {
        panic = true;
        return;
    }

    llvmBuilder.CreateBr(breakStack.top());
}

void Codegen::codegen(const ContinueAst *ast) {
    if (continueStack.empty()) {
        panic = true;
        return;
    }

    llvmBuilder.CreateBr(continueStack.top());
}

// TODO get rid of llvm::SwitchInst, allow other types
void Codegen::codegen(const SwitchAst *ast) {
    ExprGenPayload valExprPay = codegenExpr(ast->getValue());

    // literals get cast to the widest sint type, along with comparison values
    if (valExprPay.isUntyVal() && !promoteUntyped(valExprPay, TypeTable::WIDEST_I)) {
        panic = true;
        return;
    }
    if (valBroken(valExprPay) ||
        !(getTypeTable()->isTypeI(valExprPay.type) || getTypeTable()->isTypeU(valExprPay.type))) {
        panic = true;
        return;
    }

    size_t caseBlockNum = ast->getCases().size();
    size_t caseCompNum = 0;

    vector<llvm::BasicBlock*> blocks(caseBlockNum);
    for (size_t i = 0; i < blocks.size(); ++i) {
        blocks[i] = llvm::BasicBlock::Create(llvmContext, "case");
    }
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    vector<vector<llvm::ConstantInt*>> caseComps(caseBlockNum);
    unordered_set<int64_t> caseVals;
    for (size_t i = 0; i < caseBlockNum; ++i) {
        for (const auto &comp_ : ast->getCases()[i].comparisons) {
            ExprGenPayload compExprPay = codegenExpr(comp_.get());
            if (!compExprPay.isUntyVal() ||
                compExprPay.untyVal.type != UntypedVal::T_SINT || !promoteUntyped(compExprPay, valExprPay.type) ||
                caseVals.find(compExprPay.untyVal.val_si) != caseVals.end()) {
                panic = true;
                return;
            }

            caseComps[i].push_back((llvm::ConstantInt*) compExprPay.val);
            caseVals.insert(compExprPay.untyVal.val_si);
            ++caseCompNum;
        }
    }

    pair<bool, size_t> def = ast->getDefault();
    llvm::BasicBlock *defBlock = def.first ? blocks[def.second] : afterBlock;

    llvm::SwitchInst *switchInst = llvmBuilder.CreateSwitch(valExprPay.val, defBlock, caseCompNum);
    for (size_t i = 0; i < caseBlockNum; ++i) {
        for (const auto &it : caseComps[i]) {
            switchInst->addCase(it, blocks[i]);
        }
    }

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();
    for (size_t i = 0; i < caseBlockNum; ++i) {
        func->getBasicBlockList().push_back(blocks[i]);
        llvmBuilder.SetInsertPoint(blocks[i]);

        codegen(ast->getCases()[i].body.get(), true);
        if (panic) return;

        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void Codegen::codegen(const RetAst *ast) {
    pair<FuncValue, bool> currFunc = symbolTable->getCurrFunc();
    if (currFunc.second == false) {
        panic = true;
        return;
    }

    if (!ast->getVal()) {
        if (currFunc.first.hasRet) {
            panic = true;
            return;
        }
        llvmBuilder.CreateRetVoid();
        return;
    }

    ExprGenPayload retExpr = codegenExpr(ast->getVal());
    if (retExpr.isUntyVal() && !promoteUntyped(retExpr, currFunc.first.retType)) return;
    if (valBroken(retExpr)) return;

    llvm::Value *retVal = retExpr.val;
    if (retExpr.type != currFunc.first.retType) {
        if (!getTypeTable()->isImplicitCastable(retExpr.type, currFunc.first.retType)) {
            panic = true;
            return;
        }
        createCast(retVal, retExpr.type, currFunc.first.retType);
    }

    llvmBuilder.CreateRet(retVal);
}

void Codegen::codegen(const BlockAst *ast, bool makeScope) {
    ScopeControl scope(makeScope ? symbolTable : nullptr);

    for (const auto &it : ast->getBody()) codegenNode(it.get());
}

std::pair<FuncValue, bool> Codegen::codegen(const FuncProtoAst *ast, bool definition) {
    if (symbolTable->funcNameTaken(ast->getName())) {
        panic = true;
        return make_pair(FuncValue(), false);
    }

    FuncValue val;
    val.name = ast->getName();
    val.argTypes = vector<TypeTable::Id>(ast->getArgCnt());
    for (size_t i = 0; i < ast->getArgCnt(); ++i) val.argTypes[i] = ast->getArgType(i)->getTypeId();
    val.hasRet = ast->hasRetVal();
    if (val.hasRet) val.retType = ast->getRetType()->getTypeId();
    val.defined = definition;
    val.variadic = ast->isVariadic();
    val.noNameMangle = ast->isNoNameMangle();

    if (!symbolTable->canRegisterFunc(val)) {
        panic = true;
        return make_pair(FuncValue(), false);
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < ast->getArgCnt(); ++i) {
        for (size_t j = i+1; j < ast->getArgCnt(); ++j) {
            if (ast->getArgName(i) == ast->getArgName(j)) {
                panic = true;
                return make_pair(FuncValue(), false);
            }
        }
    }

    NamePool::Id funcName = ast->getName();
    if (!val.noNameMangle) {
        pair<bool, NamePool::Id> mangled = mangleName(val);
        if (mangled.first == false) {
            panic = true;
            return make_pair(FuncValue(), false);
        }
        funcName = mangled.second;
    }

    llvm::Function *func = symbolTable->getFunction(val);
    if (func == nullptr) {
        vector<llvm::Type*> argTypes(ast->getArgCnt());
        for (size_t i = 0; i < argTypes.size(); ++i)
            argTypes[i] = getType(ast->getArgType(i)->getTypeId());
        llvm::Type *retType = ast->hasRetVal() ? getType(ast->getRetType()->getTypeId()) : llvm::Type::getVoidTy(llvmContext);
        llvm::FunctionType *funcType = llvm::FunctionType::get(retType, argTypes, val.variadic);

        // TODO optimize on const args
        func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                namePool->get(funcName), llvmModule.get());
        
        size_t i = 0;
        for (auto &arg : func->args()) {
            arg.setName(namePool->get(ast->getArgName(i)));
            ++i;
        }
    }

    val.func = func;

    return make_pair(symbolTable->registerFunc(val), true);
}

void Codegen::codegen(const FuncAst *ast) {
    std::pair<FuncValue, bool> funcValRet = codegen(ast->getProto(), true);
    if (funcValRet.second == false) {
        panic = true;
        return;
    }

    const FuncValue *funcVal = &funcValRet.first;

    ScopeControl scope(*symbolTable, *funcVal);

    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", funcVal->func));

    llvm::BasicBlock *body = llvm::BasicBlock::Create(llvmContext, "entry", funcVal->func);
    llvmBuilder.SetInsertPoint(body);

    size_t i = 0;
    for (auto &arg : funcVal->func->args()) {
        const TypeAst *astArgType = ast->getProto()->getArgType(i);
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