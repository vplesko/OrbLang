#include "Codegen.h"
#include <unordered_set>
#include "llvm/IR/Verifier.h"
using namespace std;

bool Codegen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId) {
    if (srcTypeId == dstTypeId) return true;

    if (val == nullptr || type == nullptr) {
        return false;
    }

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
            val = nullptr;
            return false;
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
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->isTypeF(srcTypeId)) {
        if (getTypeTable()->isTypeI(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToSI, val, type, "f2i_cast"));
        else if (getTypeTable()->isTypeU(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToUI, val, type, "f2u_cast"));
        else if (getTypeTable()->isTypeF(dstTypeId))
            val = llvmBuilder.CreateFPCast(val, type, "f2f_cast");
        else {
            val = nullptr;
            return false;
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
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->isTypeB(srcTypeId)) {
        if (getTypeTable()->isTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "b2i_cast");
        else if (getTypeTable()->isTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "b2u_cast");
        else {
            val = nullptr;
            return false;
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
                llvmBuilder.CreatePtrToInt(val, getType(getPrimTypeId(TypeTable::WIDEST_I))),
                llvm::ConstantInt::get(getType(getPrimTypeId(TypeTable::WIDEST_I)), 0),
                "p2b_cast");
        } else {
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->isTypeArr(srcTypeId) || getTypeTable()->isDataType(srcTypeId)) {
        // NOTE data types and arrs are only castable when changing constness
        if (!getTypeTable()->isImplicitCastable(srcTypeId, dstTypeId)) {
            // no action is needed in case of a cast
            val = nullptr;
            return false;
        }
    } else {
        val = nullptr;
        return false;
    }

    return true;
}

bool Codegen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    return createCast(val, srcTypeId, getType(dstTypeId), dstTypeId);
}

bool Codegen::createCast(ExprGenPayload &e, TypeTable::Id t) {
    if (!createCast(e.val, e.type, t)) return false;
    e.type = t;
    return true;
}

void Codegen::codegenNode(const BaseAst *ast, bool blockMakeScope) {
    switch (ast->type()) {
    case AST_Empty:
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
    case AST_Data:
        codegen((const DataAst*)ast);
        return;
    default:
        codegenExpr((const ExprAst*)ast);
    }
}

llvm::Type* Codegen::codegenType(const TypeAst *ast) {
    llvm::Type *type = getType(ast->getTypeId());
    if (type == nullptr) {
        msgs->errorUndefinedType(ast->loc());
        return nullptr;
    }

    return type;
}

void Codegen::codegen(const DeclAst *ast) {
    TypeTable::Id typeId = ast->getType()->getTypeId();
    if (!getTypeTable()->isNonOpaqueType(typeId)) {
        if (getTypeTable()->isDataType(typeId)) {
            msgs->errorDataOpaqueInit(ast->getType()->loc());
        } else {
            msgs->errorUndefinedType(ast->getType()->loc());
        }
        return;
    }

    llvm::Type *type = codegenType(ast->getType());
    if (type == nullptr) return;

    for (const auto &it : ast->getDecls()) {
        if (!symbolTable->varMayTakeName(it.first->getNameId())) {
            msgs->errorVarNameTaken(it.first->loc(), it.first->getNameId());
            return;
        }

        const string &name = namePool->get(it.first->getNameId());

        llvm::Value *val;
        if (isGlobalScope()) {
            llvm::Constant *initConst = nullptr;

            const ExprAst *init = it.second.get();
            if (init != nullptr) {
                ExprGenPayload initPay = codegenExpr(init);
                if (valueBroken(initPay)) {
                    return;
                }
                if (!initPay.isUntyVal()) {
                    msgs->errorExprNotBaked(init->loc());
                    return;
                }
                if (!promoteUntyped(initPay, typeId)) {
                    msgs->errorExprCannotPromote(init->loc(), typeId);
                    return;
                }
                initConst = (llvm::Constant*) initPay.val;
            } else {
                if (getTypeTable()->isTypeCn(typeId)) {
                    msgs->errorCnNoInit(it.first->loc(), it.first->getNameId());
                    return;
                }
            }

            val = createGlobal(type, initConst, getTypeTable()->isTypeCn(typeId), name);
        } else {
            val = createAlloca(type, name);

            const ExprAst *init = it.second.get();
            if (init != nullptr) {
                ExprGenPayload initPay = codegenExpr(init);
                if (valueBroken(initPay))
                    return;

                if (initPay.isUntyVal()) {
                    if (!promoteUntyped(initPay, typeId)) {
                        msgs->errorExprCannotPromote(init->loc(), typeId);
                        return;
                    }
                }

                llvm::Value *src = initPay.val;

                if (initPay.type != typeId) {
                    if (!getTypeTable()->isImplicitCastable(initPay.type, typeId)) {
                        msgs->errorExprCannotImplicitCast(init->loc(), initPay.type, typeId);
                        return;
                    }

                    createCast(src, initPay.type, type, typeId);
                }

                llvmBuilder.CreateStore(src, val);
            } else {
                if (getTypeTable()->isTypeCn(typeId)) {
                    msgs->errorCnNoInit(it.first->loc(), it.first->getNameId());
                    return;
                }
            }
        }

        symbolTable->addVar(it.first->getNameId(), {typeId, val});
    }
}

void Codegen::codegen(const IfAst *ast) {
    // unlike C++, then and else may eclipse vars declared in if's init
    ScopeControl scope(ast->hasInit() ? symbolTable : nullptr);

    if (ast->hasInit()) {
        codegenNode(ast->getInit());
        if (msgs->isAbort()) return;
    }

    ExprGenPayload condExpr = codegenExpr(ast->getCond());
    if (condExpr.isUntyVal() && !promoteUntyped(condExpr, getPrimTypeId(TypeTable::P_BOOL))) {
        msgs->errorExprCannotPromote(ast->getCond()->loc(), getPrimTypeId(TypeTable::P_BOOL));
        return;
    }
    if (valBroken(condExpr) || !getTypeTable()->isTypeB(condExpr.type)) {
        msgs->errorExprCannotImplicitCast(ast->getCond()->loc(), condExpr.type, getPrimTypeId(TypeTable::P_BOOL));
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
        if (msgs->isAbort()) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    if (ast->hasElse()) {
        ScopeControl elseScope(symbolTable);
        func->getBasicBlockList().push_back(elseBlock);
        llvmBuilder.SetInsertPoint(elseBlock);
        codegenNode(ast->getElse(), false);
        if (msgs->isAbort()) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void Codegen::codegen(const ForAst *ast) {
    ScopeControl scope(ast->getInit()->type() == AST_Decl ? symbolTable : nullptr);

    codegenNode(ast->getInit());
    if (msgs->isAbort()) return;

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
            if (condExpr.isUntyVal() && !promoteUntyped(condExpr, getPrimTypeId(TypeTable::P_BOOL))) {
                msgs->errorExprCannotPromote(ast->getCond()->loc(), getPrimTypeId(TypeTable::P_BOOL));
                return;
            }
            if (valBroken(condExpr) || !getTypeTable()->isTypeB(condExpr.type)) {
                msgs->errorExprCannotImplicitCast(ast->getCond()->loc(), condExpr.type, getPrimTypeId(TypeTable::P_BOOL));
                return;
            }
        } else {
            condExpr.type = getPrimTypeId(TypeTable::P_BOOL);
            condExpr.val = getConstB(true);
        }

        llvmBuilder.CreateCondBr(condExpr.val, bodyBlock, afterBlock);
    }

    {
        ScopeControl scopeBody(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);
        codegenNode(ast->getBody(), false);
        if (msgs->isAbort()) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(iterBlock);
    }
    
    {
        func->getBasicBlockList().push_back(iterBlock);
        llvmBuilder.SetInsertPoint(iterBlock);

        if (ast->hasIter()) {
            codegenNode(ast->getIter());
            if (msgs->isAbort()) return;
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
        if (condExpr.isUntyVal() && !promoteUntyped(condExpr, getPrimTypeId(TypeTable::P_BOOL))) {
            msgs->errorExprCannotPromote(ast->getCond()->loc(), getPrimTypeId(TypeTable::P_BOOL));
            return;
        }
        if (valBroken(condExpr) || !getTypeTable()->isTypeB(condExpr.type)) {
            msgs->errorExprCannotImplicitCast(ast->getCond()->loc(), condExpr.type, getPrimTypeId(TypeTable::P_BOOL));
            return;
        }

        llvmBuilder.CreateCondBr(condExpr.val, bodyBlock, afterBlock);
    }

    {
        ScopeControl scope(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);
        codegenNode(ast->getBody(), false);
        if (msgs->isAbort()) return;
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
        if (msgs->isAbort()) return;
        if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);
    }

    {
        func->getBasicBlockList().push_back(condBlock);
        llvmBuilder.SetInsertPoint(condBlock);

        ExprGenPayload condExpr = codegenExpr(ast->getCond());
        if (condExpr.isUntyVal() && !promoteUntyped(condExpr, getPrimTypeId(TypeTable::P_BOOL))) {
            msgs->errorExprCannotPromote(ast->getCond()->loc(), getPrimTypeId(TypeTable::P_BOOL));
            return;
        }
        if (valBroken(condExpr) || !getTypeTable()->isTypeB(condExpr.type)) {
            msgs->errorExprCannotImplicitCast(ast->getCond()->loc(), condExpr.type, getPrimTypeId(TypeTable::P_BOOL));
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
        msgs->errorBreakNowhere(ast->loc());
        return;
    }

    llvmBuilder.CreateBr(breakStack.top());
}

void Codegen::codegen(const ContinueAst *ast) {
    if (continueStack.empty()) {
        msgs->errorContinueNowhere(ast->loc());
        return;
    }

    llvmBuilder.CreateBr(continueStack.top());
}

// TODO get rid of llvm::SwitchInst, allow other types
void Codegen::codegen(const SwitchAst *ast) {
    ExprGenPayload valExprPay = codegenExpr(ast->getValue());

    // literals get cast to the widest sint type, along with comparison values
    if (valExprPay.isUntyVal() && !promoteUntyped(valExprPay, getPrimTypeId(TypeTable::WIDEST_I))) {
        msgs->errorSwitchNotIntegral(ast->getValue()->loc());
        return;
    }
    if (valBroken(valExprPay) ||
        !(getTypeTable()->isTypeI(valExprPay.type) || getTypeTable()->isTypeU(valExprPay.type))) {
        msgs->errorSwitchNotIntegral(ast->getValue()->loc());
        return;
    }

    // num of blocks of execution (one for each case)
    size_t caseBlockNum = ast->getCases().size();
    // total num of comparison expressions among cases
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
            if (!compExprPay.isUntyVal()) {
                msgs->errorExprNotBaked(comp_->loc());
                return;
            }
            if (compExprPay.untyVal.type != UntypedVal::T_SINT || !promoteUntyped(compExprPay, valExprPay.type)) {
                msgs->errorExprCannotPromote(comp_->loc(), valExprPay.type);
                return;
            }
            if (caseVals.find(compExprPay.untyVal.val_si) != caseVals.end()) {
                msgs->errorSwitchMatchDuplicate(comp_->loc());
                return;
            }

            caseComps[i].push_back((llvm::ConstantInt*) compExprPay.val);
            caseVals.insert(compExprPay.untyVal.val_si);
            ++caseCompNum;
        }
    }

    optional<size_t> def = ast->getDefault();
    llvm::BasicBlock *defBlock = def ? blocks[def.value()] : afterBlock;

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
        if (msgs->isAbort()) return;

        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);
}

void Codegen::codegen(const RetAst *ast) {
    optional<FuncValue> currFunc = symbolTable->getCurrFunc();
    if (!currFunc.has_value()) {
        // should not happen, cannot parse stmnt outside of functions
        msgs->errorUnknown(ast->loc());
        return;
    }

    if (!ast->getVal()) {
        if (currFunc.value().hasRet()) {
            msgs->errorRetNoValue(ast->loc(), currFunc.value().retType.value());
            return;
        }
        llvmBuilder.CreateRetVoid();
        return;
    }

    ExprGenPayload retExpr = codegenExpr(ast->getVal());
    if (retExpr.isUntyVal() && !promoteUntyped(retExpr, currFunc.value().retType.value())) {
        msgs->errorExprCannotPromote(ast->getVal()->loc(), currFunc.value().retType.value());
        return;
    }
    if (valBroken(retExpr)) return;

    llvm::Value *retVal = retExpr.val;
    if (retExpr.type != currFunc.value().retType.value()) {
        if (!getTypeTable()->isImplicitCastable(retExpr.type, currFunc.value().retType.value())) {
            msgs->errorExprCannotImplicitCast(ast->getVal()->loc(), retExpr.type, currFunc.value().retType.value());
            return;
        }
        createCast(retVal, retExpr.type, currFunc.value().retType.value());
    }

    llvmBuilder.CreateRet(retVal);
}

void Codegen::codegen(const DataAst *ast) {
    TypeTable::DataType &dataType = getTypeTable()->getDataType(ast->getTypeId());
    
    // declaration part
    if (getTypeTable()->getType(ast->getTypeId()) == nullptr) {
        llvm::StructType *structType = llvm::StructType::create(llvmContext, namePool->get(dataType.name));
        getTypeTable()->setType(ast->getTypeId(), structType);
    }

    // definition part
    if (!ast->isDecl()) {
        if (!dataType.isDecl()) {
            msgs->errorDataRedefinition(ast->loc(), dataType.name);
            return;
        }

        llvm::StructType *structType = ((llvm::StructType*) getTypeTable()->getType(ast->getTypeId()));

        vector<llvm::Type*> memberTypes;
        for (const auto &decl : ast->getMembers()) {
            TypeTable::Id memberTypeId = decl->getType()->getTypeId();

            llvm::Type *memberType = codegenType(decl->getType());
            if (memberType == nullptr) return;

            for (const auto &var : decl->getDecls()) {
                NamePool::Id memberName = var.first->getNameId();
                
                if (getTypeTable()->isTypeCn(memberTypeId)) {
                    msgs->errorCnNoInit(var.first->loc(), var.first->getNameId());
                    return;
                }
                
                TypeTable::DataType::Member member;
                member.name = memberName;
                member.type = memberTypeId;
                dataType.addMember(member);

                memberTypes.push_back(memberType);
            }
        }

        structType->setBody(memberTypes);
    }
}

void Codegen::codegen(const BlockAst *ast, bool makeScope) {
    ScopeControl scope(makeScope ? symbolTable : nullptr);

    for (const auto &it : ast->getBody()) codegenNode(it.get());
}

optional<FuncValue> Codegen::codegen(const FuncProtoAst *ast, bool definition) {
    if (!symbolTable->funcMayTakeName(ast->getName())) {
        msgs->errorFuncNameTaken(ast->loc(), ast->getName());
        return nullopt;
    }

    FuncValue val;
    val.name = ast->getName();
    val.argTypes = vector<TypeTable::Id>(ast->getArgCnt());
    for (size_t i = 0; i < ast->getArgCnt(); ++i) val.argTypes[i] = ast->getArgType(i)->getTypeId();
    if (ast->hasRetVal()) val.retType = ast->getRetType()->getTypeId();
    val.defined = definition;
    val.variadic = ast->isVariadic();
    val.noNameMangle = ast->isNoNameMangle();

    if (!symbolTable->canRegisterFunc(val)) {
        msgs->errorFuncSigConflict(ast->loc());
        return nullopt;
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < ast->getArgCnt(); ++i) {
        for (size_t j = i+1; j < ast->getArgCnt(); ++j) {
            if (ast->getArgName(i) == ast->getArgName(j)) {
                msgs->errorFuncArgNameDuplicate(ast->loc(), ast->getArgName(j));
                return nullopt;
            }
        }
    }

    NamePool::Id funcName = ast->getName();
    if (!val.noNameMangle) {
        optional<NamePool::Id> mangled = mangleName(val);
        if (!mangled) {
            // should not happen
            msgs->errorUnknown(ast->loc());
            return nullopt;
        }
        funcName = mangled.value();
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

    return symbolTable->registerFunc(val);
}

void Codegen::codegen(const FuncAst *ast) {
    optional<FuncValue> funcValRet = codegen(ast->getProto(), true);
    if (!funcValRet.has_value()) return;

    const FuncValue *funcVal = &funcValRet.value();

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
    if (msgs->isAbort()) {
        funcVal->func->eraseFromParent();
        return;
    }

    llvmBuilderAlloca.CreateBr(body);

    if (!ast->getProto()->hasRetVal() && !isBlockTerminated())
            llvmBuilder.CreateRetVoid();

    if (llvm::verifyFunction(*funcVal->func, &llvm::errs())) cerr << endl;
}