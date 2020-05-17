#include "Codegen.h"
#include <unordered_set>
#include "llvm/IR/Verifier.h"
using namespace std;

bool Codegen::createCast(llvm::Value *&val, TypeTable::Id srcTypeId, llvm::Type *type, TypeTable::Id dstTypeId) {
    if (srcTypeId == dstTypeId) return true;

    if (val == nullptr || type == nullptr) {
        return false;
    }

    if (getTypeTable()->worksAsTypeI(srcTypeId)) {
        if (getTypeTable()->worksAsTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, true, "i2i_cast");
        else if (getTypeTable()->worksAsTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "i2u_cast");
        else if (getTypeTable()->worksAsTypeF(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::SIToFP, val, type, "i2f_cast"));
        else if (getTypeTable()->worksAsTypeC(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "i2c_cast");
        else if (getTypeTable()->worksAsTypeB(dstTypeId)) {
            llvm::Value *z = llvmBuilder.CreateIntCast(getConstB(false), val->getType(), true);
            val = llvmBuilder.CreateICmpNE(val, z, "i2b_cast");
        } else if (symbolTable->getTypeTable()->worksAsTypeAnyP(dstTypeId)) {
            val = llvmBuilder.CreatePointerCast(val, type, "i2p_cast");
        } else {
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->worksAsTypeU(srcTypeId)) {
        if (getTypeTable()->worksAsTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, true, "u2i_cast");
        else if (getTypeTable()->worksAsTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "u2u_cast");
        else if (getTypeTable()->worksAsTypeF(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::UIToFP, val, type, "u2f_cast"));
        else if (getTypeTable()->worksAsTypeC(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "u2c_cast");
        else if (getTypeTable()->worksAsTypeB(dstTypeId)) {
            llvm::Value *z = llvmBuilder.CreateIntCast(getConstB(false), val->getType(), false);
            val = llvmBuilder.CreateICmpNE(val, z, "u2b_cast");
        } else if (symbolTable->getTypeTable()->worksAsTypeAnyP(dstTypeId)) {
            val = llvmBuilder.CreatePointerCast(val, type, "u2p_cast");
        } else {
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->worksAsTypeF(srcTypeId)) {
        if (getTypeTable()->worksAsTypeI(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToSI, val, type, "f2i_cast"));
        else if (getTypeTable()->worksAsTypeU(dstTypeId))
            val = llvmBuilder.Insert(llvm::CastInst::Create(llvm::Instruction::FPToUI, val, type, "f2u_cast"));
        else if (getTypeTable()->worksAsTypeF(dstTypeId))
            val = llvmBuilder.CreateFPCast(val, type, "f2f_cast");
        else {
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->worksAsTypeC(srcTypeId)) {
        if (getTypeTable()->worksAsTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, true, "c2i_cast");
        else if (getTypeTable()->worksAsTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "c2u_cast");
        else if (getTypeTable()->worksAsTypeC(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "c2c_cast");
        else if (getTypeTable()->worksAsTypeB(dstTypeId)) {
            llvm::Value *z = llvmBuilder.CreateIntCast(getConstB(false), val->getType(), false);
            val = llvmBuilder.CreateICmpNE(val, z, "c2b_cast");
        } else {
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->worksAsTypeB(srcTypeId)) {
        if (getTypeTable()->worksAsTypeI(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "b2i_cast");
        else if (getTypeTable()->worksAsTypeU(dstTypeId))
            val = llvmBuilder.CreateIntCast(val, type, false, "b2u_cast");
        else {
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->worksAsTypeAnyP(srcTypeId)) {
        if (getTypeTable()->worksAsTypeI(dstTypeId))
            val = llvmBuilder.CreatePtrToInt(val, type, "p2i_cast");
        else if (getTypeTable()->worksAsTypeU(dstTypeId))
            val = llvmBuilder.CreatePtrToInt(val, type, "p2u_cast");
        else if (symbolTable->getTypeTable()->worksAsTypeAnyP(dstTypeId))
            val = llvmBuilder.CreatePointerCast(val, type, "p2p_cast");
        else if (getTypeTable()->worksAsTypeB(dstTypeId)) {
            val = llvmBuilder.CreateICmpNE(
                llvmBuilder.CreatePtrToInt(val, getType(getPrimTypeId(TypeTable::WIDEST_I))),
                llvm::ConstantInt::get(getType(getPrimTypeId(TypeTable::WIDEST_I)), 0),
                "p2b_cast");
        } else {
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->worksAsTypeArr(srcTypeId) || getTypeTable()->isDataType(srcTypeId)) {
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

bool Codegen::createCast(NodeVal &e, TypeTable::Id t) {
    if (!e.isLlvmVal()) return false;

    if (!createCast(e.llvmVal.val, e.llvmVal.type, t)) return false;
    e.llvmVal.type = t;
    return true;
}

NodeVal Codegen::codegenTerminal(const AstNode *ast) {
    const TerminalVal &term = ast->terminal.value();

    NodeVal ret;

    switch (term.kind) {
    case TerminalVal::Kind::kKeyword:
        ret = NodeVal(NodeVal::Kind::kKeyword);
        ret.keyword = term.keyword;
        break;
    case TerminalVal::Kind::kOper:
        ret = NodeVal(NodeVal::Kind::kOper);
        ret.oper = term.oper;
        break;
    case TerminalVal::Kind::kId:
        if (getTypeTable()->isType(term.id)) {
            TypeTable::Id type = getTypeTable()->getTypeId(term.id).value();
            ret = NodeVal(NodeVal::Kind::kType);
            ret.type = type;
        } else if (symbolTable->isFuncName(term.id)) {
            ret = NodeVal(NodeVal::Kind::kFuncId);
            ret.id = term.id;
        } else {
            ret = codegenVar(ast);
        }
        break;
    case TerminalVal::Kind::kAttribute:
        ret = NodeVal(NodeVal::Kind::kAttribute);
        ret.attribute = term.attribute;
        break;
    case TerminalVal::Kind::kVal:
        ret = codegenUntypedVal(ast);
        break;
    case TerminalVal::Kind::kEmpty:
        ret = NodeVal(NodeVal::Kind::kEmpty);
        break;
    };

    return ret;
}

NodeVal Codegen::codegenNode(const AstNode *ast) {
    if (ast->kind == AstNode::Kind::kTerminal) {
        return codegenTerminal(ast);
    }

    NodeVal starting = codegenNode(ast->children[0].get());

    if (starting.isKeyword()) {
        switch (starting.keyword) {
        case Token::T_IMPORT:
            return codegenImport(ast);
        case Token::T_LET:
            return codegenLet(ast);
        case Token::T_BLOCK:
            return codegenBlock(ast);
        case Token::T_IF:
            return codegenIf(ast);
        case Token::T_FOR:
            return codegenFor(ast);
        case Token::T_WHILE:
            return codegenWhile(ast);
        case Token::T_DO:
            return codegenDo(ast);
        case Token::T_BREAK:
            return codegenBreak(ast);
        case Token::T_CONTINUE:
            return codegenContinue(ast);
        case Token::T_RET:
            return codegenRet(ast);
        case Token::T_FNC:
            return codegenFunc(ast);
        case Token::T_DATA:
            return codegenData(ast);
        case Token::T_CAST:
            return codegenCast(ast);
        case Token::T_ARR:
            return codegenArr(ast);
        default:
            msgs->errorUnknown(ast->codeLoc);
            return NodeVal();
        }
    } else if (starting.isOper() || starting.isFuncId()) {
        return codegenExpr(ast, starting);
    } else if (starting.isType()) {
        return codegenType(ast, starting);
    } else {
        msgs->errorUnknown(ast->codeLoc);
        return NodeVal();
    }
}

NodeVal Codegen::codegenImport(const AstNode *ast) {
    if (!checkGlobalScope(ast->codeLoc, true) ||
        !checkExactlyChildren(ast, 2, true))
        return NodeVal();
    
    const AstNode *nodeFile = ast->children[1].get();

    optional<UntypedVal> val = getUntypedVal(nodeFile, true);
    if (!val.has_value()) return NodeVal();

    if (val.value().kind != UntypedVal::Kind::kString) {
        msgs->errorUnknown(nodeFile->codeLoc);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kImport);
    ret.file = val.value().val_str;
    return ret;
}

NodeVal Codegen::codegenType(const AstNode *ast, const NodeVal &first) {
    TypeTable::TypeDescr typeDescr(first.type);
    for (size_t i = 1; i < ast->children.size(); ++i) {
        const AstNode *nodeChild = ast->children[i].get();

        optional<Token::Type> keyw = getKeyword(nodeChild, false);
        optional<Token::Oper> op = getOper(nodeChild, false);
        optional<UntypedVal> val = getUntypedVal(nodeChild, false);

        if (op.has_value() && op == Token::O_MUL) {
            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_PTR});
        } else if (op.has_value() && op == Token::O_IND) {
            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_ARR_PTR});
        } else if (val.has_value()) {
            if (val.value().kind != UntypedVal::Kind::kSint) {
                msgs->errorUnknown(nodeChild->codeLoc);
                return NodeVal();
            }
            int64_t arrSize = val.value().val_si;
            if (arrSize <= 0) {
                msgs->errorBadArraySize(nodeChild->codeLoc, arrSize);
                return NodeVal();
            }

            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_ARR, (unsigned long) arrSize});
        } else if (keyw.has_value() && keyw == Token::T_CN) {
            typeDescr.setLastCn();
        } else {
            msgs->errorUnknown(nodeChild->codeLoc);
            return NodeVal();
        }
    }

    TypeTable::Id typeId = symbolTable->getTypeTable()->addTypeDescr(move(typeDescr));

    NodeVal ret(NodeVal::Kind::kType);
    ret.type = typeId;
    return ret;
}

NodeVal Codegen::codegenLet(const AstNode *ast) {
    if (!checkAtLeastChildren(ast, 2, true))
        return NodeVal();
    
    for (size_t i = 1; i < ast->children.size(); ++i) {
        const AstNode *childNode = ast->children[i].get();

        NameTypePair nameType;
        optional<const AstNode*> init;

        CodeLoc codeLocName, codeLocType, codeLocInit;

        if (childNode->kind == AstNode::Kind::kTerminal) {
            optional<NameTypePair> optIdType = getIdTypePair(childNode, true);
            if (!optIdType.has_value()) return NodeVal();
            nameType = optIdType.value();

            codeLocName = childNode->codeLoc;
            codeLocType = childNode->type->get()->codeLoc;
        } else {
            if (!checkExactlyChildren(childNode, 2, true)) return NodeVal();

            optional<NameTypePair> optIdType = getIdTypePair(childNode->children[0].get(), true);
            if (!optIdType.has_value()) return NodeVal();
            nameType = optIdType.value();

            init = childNode->children[1].get();
            
            codeLocName = childNode->children[0]->codeLoc;
            codeLocType = childNode->children[0]->type->get()->codeLoc;
            codeLocInit = init.value()->codeLoc;
        }

        TypeTable::Id typeId = nameType.second;
        if (!checkDefinedTypeOrError(typeId, codeLocType)) {
            return NodeVal();
        }

        llvm::Type *type = getType(typeId);
        if (type == nullptr) return NodeVal();

        if (!symbolTable->varMayTakeName(nameType.first)) {
            msgs->errorVarNameTaken(codeLocName, nameType.first);
            return NodeVal();
        }

        const string &name = namePool->get(nameType.first);

        llvm::Value *val;
        if (isGlobalScope()) {
            llvm::Constant *initConst = nullptr;

            if (init.has_value()) {
                NodeVal initPay = codegenNode(init.value());
                if (initPay.valueBroken()) {
                    return NodeVal();
                }
                if (!initPay.isUntyVal()) {
                    msgs->errorExprNotBaked(codeLocInit);
                    return NodeVal();
                }
                if (!promoteUntyped(initPay, typeId)) {
                    msgs->errorExprCannotPromote(codeLocInit, typeId);
                    return NodeVal();
                }
                initConst = (llvm::Constant*) initPay.llvmVal.val;
            } else {
                if (getTypeTable()->worksAsTypeCn(typeId)) {
                    msgs->errorCnNoInit(codeLocName, nameType.first);
                    return NodeVal();
                }
            }

            val = createGlobal(type, initConst, getTypeTable()->worksAsTypeCn(typeId), name);
        } else {
            val = createAlloca(type, name);

            if (init.has_value()) {
                NodeVal initPay = codegenNode(init.value());
                if (initPay.valueBroken())
                    return NodeVal();

                if (initPay.isUntyVal()) {
                    if (!promoteUntyped(initPay, typeId)) {
                        msgs->errorExprCannotPromote(codeLocInit, typeId);
                        return NodeVal();
                    }
                }

                llvm::Value *src = initPay.llvmVal.val;

                if (initPay.llvmVal.type != typeId) {
                    if (!getTypeTable()->isImplicitCastable(initPay.llvmVal.type, typeId)) {
                        msgs->errorExprCannotImplicitCast(codeLocInit, initPay.llvmVal.type, typeId);
                        return NodeVal();
                    }

                    createCast(src, initPay.llvmVal.type, type, typeId);
                }

                llvmBuilder.CreateStore(src, val);
            } else {
                if (getTypeTable()->worksAsTypeCn(typeId)) {
                    msgs->errorCnNoInit(codeLocName, nameType.first);
                    return NodeVal();
                }
            }
        }

        symbolTable->addVar(nameType.first, {typeId, val});
    }

    return NodeVal();
}

NodeVal Codegen::codegenIf(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 3, 4, true))
        return NodeVal();

    bool hasElse = ast->children.size() == 4;

    const AstNode *nodeCond = ast->children[1].get();
    const AstNode *nodeThen = ast->children[2].get();
    const AstNode *nodeElse = hasElse ? ast->children[3].get() : nullptr;
    
    NodeVal condExpr = codegenNode(nodeCond);
    if (!checkValueUnbroken(nodeCond->codeLoc, condExpr, true)) return NodeVal();
    if (condExpr.isUntyVal() && !promoteUntyped(condExpr, getPrimTypeId(TypeTable::P_BOOL))) {
        msgs->errorExprCannotPromote(nodeCond->codeLoc, getPrimTypeId(TypeTable::P_BOOL));
        return NodeVal();
    }
    if (!getTypeTable()->worksAsTypeB(condExpr.llvmVal.type)) {
        msgs->errorExprCannotImplicitCast(nodeCond->codeLoc, condExpr.llvmVal.type, getPrimTypeId(TypeTable::P_BOOL));
        return NodeVal();
    }

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(llvmContext, "then", func);
    llvm::BasicBlock *elseBlock = hasElse ? llvm::BasicBlock::Create(llvmContext, "else") : nullptr;
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateCondBr(condExpr.llvmVal.val, thenBlock, hasElse ? elseBlock : afterBlock);

    {
        ScopeControl thenScope(symbolTable);
        llvmBuilder.SetInsertPoint(thenBlock);
        codegenAll(nodeThen, false);
        if (msgs->isAbort()) return NodeVal();
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    if (hasElse) {
        ScopeControl elseScope(symbolTable);
        func->getBasicBlockList().push_back(elseBlock);
        llvmBuilder.SetInsertPoint(elseBlock);
        codegenAll(nodeElse, false);
        if (msgs->isAbort()) return NodeVal();
        if (!isBlockTerminated()) llvmBuilder.CreateBr(afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);

    return NodeVal();
}

NodeVal Codegen::codegenFor(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 4, 5, true))
        return NodeVal();
    
    bool hasBody = ast->children.size() == 5;
    
    const AstNode *nodeInit = ast->children[1].get();
    const AstNode *nodeCond = ast->children[2].get();
    const AstNode *nodeIter = ast->children[3].get();
    const AstNode *nodeBody = hasBody ? ast->children[4].get() : nullptr;

    bool hasCond = !checkEmptyTerminal(nodeCond, false);
    bool hasIter = !checkEmptyTerminal(nodeIter, false);
    
    ScopeControl scope(symbolTable);

    codegenNode(nodeInit);
    if (msgs->isAbort()) return NodeVal();

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
        NodeVal condExpr;
        if (hasCond) {
            condExpr = codegenNode(nodeCond);
            if (!checkValueUnbroken(nodeCond->codeLoc, condExpr, true)) return NodeVal();
            if (condExpr.isUntyVal() && !promoteUntyped(condExpr, getPrimTypeId(TypeTable::P_BOOL))) {
                msgs->errorExprCannotPromote(nodeCond->codeLoc, getPrimTypeId(TypeTable::P_BOOL));
                return NodeVal();
            }
            if (!getTypeTable()->worksAsTypeB(condExpr.llvmVal.type)) {
                msgs->errorExprCannotImplicitCast(nodeCond->codeLoc, condExpr.llvmVal.type, getPrimTypeId(TypeTable::P_BOOL));
                return NodeVal();
            }
        } else {
            condExpr = NodeVal(NodeVal::Kind::kLlvmVal);
            condExpr.llvmVal.type = getPrimTypeId(TypeTable::P_BOOL);
            condExpr.llvmVal.val = getConstB(true);
        }

        llvmBuilder.CreateCondBr(condExpr.llvmVal.val, bodyBlock, afterBlock);
    }

    {
        ScopeControl scopeBody(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);
        if (hasBody) {
            codegenAll(nodeBody, false);
            if (msgs->isAbort()) return NodeVal();
        }
        if (!isBlockTerminated()) llvmBuilder.CreateBr(iterBlock);
    }
    
    {
        func->getBasicBlockList().push_back(iterBlock);
        llvmBuilder.SetInsertPoint(iterBlock);

        if (hasIter) {
            codegenNode(nodeIter);
            if (msgs->isAbort()) return NodeVal();
        }

        if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);

    breakStack.pop();
    continueStack.pop();

    return NodeVal();
}

NodeVal Codegen::codegenWhile(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 3, true)) {
        return NodeVal();
    }

    bool hasBody = ast->children.size() == 3;

    const AstNode *nodeCond = ast->children[1].get();
    const AstNode *nodeBody = hasBody ? ast->children[2].get() : nullptr;

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *condBlock = llvm::BasicBlock::Create(llvmContext, "cond", func);
    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body");
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    continueStack.push(condBlock);
    breakStack.push(afterBlock);

    llvmBuilder.CreateBr(condBlock);
    llvmBuilder.SetInsertPoint(condBlock);

    {
        NodeVal condExpr = codegenNode(nodeCond);
        if (!checkValueUnbroken(nodeCond->codeLoc, condExpr, true)) return NodeVal();
        if (condExpr.isUntyVal() && !promoteUntyped(condExpr, getPrimTypeId(TypeTable::P_BOOL))) {
            msgs->errorExprCannotPromote(nodeCond->codeLoc, getPrimTypeId(TypeTable::P_BOOL));
            return NodeVal();
        }
        if (!getTypeTable()->worksAsTypeB(condExpr.llvmVal.type)) {
            msgs->errorExprCannotImplicitCast(nodeCond->codeLoc, condExpr.llvmVal.type, getPrimTypeId(TypeTable::P_BOOL));
            return NodeVal();
        }

        llvmBuilder.CreateCondBr(condExpr.llvmVal.val, bodyBlock, afterBlock);
    }

    {
        ScopeControl scope(symbolTable);
        func->getBasicBlockList().push_back(bodyBlock);
        llvmBuilder.SetInsertPoint(bodyBlock);
        if (hasBody) {
            codegenAll(nodeBody, false);
            if (msgs->isAbort()) return NodeVal();
        }
        if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);

    breakStack.pop();
    continueStack.pop();

    return NodeVal();
}

NodeVal Codegen::codegenDo(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 3, true)) {
        return NodeVal();
    }

    const AstNode *nodeBody = ast->children[1].get();
    const AstNode *nodeCond = ast->children[2].get();

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
        codegenAll(nodeBody, false);
        if (msgs->isAbort()) return NodeVal();
        if (!isBlockTerminated()) llvmBuilder.CreateBr(condBlock);
    }

    {
        func->getBasicBlockList().push_back(condBlock);
        llvmBuilder.SetInsertPoint(condBlock);

        NodeVal condExpr = codegenNode(nodeCond);
        if (!checkValueUnbroken(nodeCond->codeLoc, condExpr, true)) return NodeVal();
        if (condExpr.isUntyVal() && !promoteUntyped(condExpr, getPrimTypeId(TypeTable::P_BOOL))) {
            msgs->errorExprCannotPromote(nodeCond->codeLoc, getPrimTypeId(TypeTable::P_BOOL));
            return NodeVal();
        }
        if (!getTypeTable()->worksAsTypeB(condExpr.llvmVal.type)) {
            msgs->errorExprCannotImplicitCast(nodeCond->codeLoc, condExpr.llvmVal.type, getPrimTypeId(TypeTable::P_BOOL));
            return NodeVal();
        }

        llvmBuilder.CreateCondBr(condExpr.llvmVal.val, bodyBlock, afterBlock);
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);

    breakStack.pop();
    continueStack.pop();

    return NodeVal();
}

NodeVal Codegen::codegenBreak(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 1, true)) {
        return NodeVal();
    }

    if (breakStack.empty()) {
        msgs->errorBreakNowhere(ast->codeLoc);
        return NodeVal();
    }

    llvmBuilder.CreateBr(breakStack.top());

    return NodeVal();
}

NodeVal Codegen::codegenContinue(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 1, true)) {
        return NodeVal();
    }

    if (continueStack.empty()) {
        msgs->errorContinueNowhere(ast->codeLoc);
        return NodeVal();
    }

    llvmBuilder.CreateBr(continueStack.top());

    return NodeVal();
}

NodeVal Codegen::codegenRet(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 1, 2, true)) {
        return NodeVal();
    }

    bool hasVal = ast->children.size() == 2;

    const AstNode *nodeVal = hasVal ? ast->children[1].get() : nullptr;

    optional<FuncValue> currFunc = symbolTable->getCurrFunc();
    if (!currFunc.has_value()) {
        msgs->errorUnknown(ast->codeLoc);
        return NodeVal();
    }

    if (!hasVal) {
        if (currFunc.value().hasRet()) {
            msgs->errorRetNoValue(ast->codeLoc, currFunc.value().retType.value());
            return NodeVal();
        }
        llvmBuilder.CreateRetVoid();
        return NodeVal();
    }

    NodeVal retExpr = codegenNode(nodeVal);
    if (!checkValueUnbroken(nodeVal->codeLoc, retExpr, true)) return NodeVal();
    if (retExpr.isUntyVal() && !promoteUntyped(retExpr, currFunc.value().retType.value())) {
        msgs->errorExprCannotPromote(nodeVal->codeLoc, currFunc.value().retType.value());
        return NodeVal();
    }

    llvm::Value *retVal = retExpr.llvmVal.val;
    if (retExpr.llvmVal.type != currFunc.value().retType.value()) {
        if (!getTypeTable()->isImplicitCastable(retExpr.llvmVal.type, currFunc.value().retType.value())) {
            msgs->errorExprCannotImplicitCast(nodeVal->codeLoc, retExpr.llvmVal.type, currFunc.value().retType.value());
            return NodeVal();
        }
        createCast(retVal, retExpr.llvmVal.type, currFunc.value().retType.value());
    }

    llvmBuilder.CreateRet(retVal);

    return NodeVal();
}

NodeVal Codegen::codegenData(const AstNode *ast) {
    if (!checkGlobalScope(ast->codeLoc, true) ||
        !checkBetweenChildren(ast, 2, 3, true)) {
        return NodeVal();
    }

    bool isDefinition = ast->children.size() == 3;

    const AstNode *nodeName = ast->children[1].get();
    const AstNode *nodeMembers = isDefinition ? ast->children[2].get() : nullptr;

    optional<NamePool::Id> dataName = getId(nodeName, true);
    if (!dataName.has_value()) return NodeVal();

    if (!symbolTable->dataMayTakeName(dataName.value())) {
        msgs->errorDataNameTaken(nodeName->codeLoc, dataName.value());
        return NodeVal();
    }

    TypeTable::Id dataTypeId = symbolTable->getTypeTable()->addDataType(dataName.value());

    TypeTable::DataType &dataType = getTypeTable()->getDataType(dataTypeId);
    
    // declaration part
    if (getTypeTable()->getType(dataTypeId) == nullptr) {
        // this isn't done in getType as we need to ensure data types get llvm::Type* on first creation
        llvm::StructType *structType = llvm::StructType::create(llvmContext, namePool->get(dataType.name));
        getTypeTable()->setType(dataTypeId, structType);
    }

    // definition part
    if (isDefinition) {
        if (!dataType.isDecl()) {
            msgs->errorDataRedefinition(ast->codeLoc, dataType.name);
            return NodeVal();
        }

        if (!checkNotTerminal(nodeMembers, true)) {
            return NodeVal();
        }

        llvm::StructType *structType = ((llvm::StructType*) getTypeTable()->getType(dataTypeId));

        unordered_set<NamePool::Id> memberNames;

        vector<llvm::Type*> memberTypes;
        for (const unique_ptr<AstNode> &memb : nodeMembers->children) {
            optional<NameTypePair> optIdType = getIdTypePair(memb.get(), true);
            if (!optIdType.has_value()) return NodeVal();

            TypeTable::Id memberTypeId = optIdType.value().second;
            if (!checkDefinedTypeOrError(memberTypeId, memb->type.value()->codeLoc)) {
                return NodeVal();
            }

            llvm::Type *memberType = getType(memberTypeId);
            if (memberType == nullptr) return NodeVal();

            NamePool::Id memberName = optIdType.value().first;
            if (memberNames.find(memberName) != memberNames.end()) {
                msgs->errorDataMemberNameDuplicate(memb->codeLoc, memberName);
                return NodeVal();
            }
            memberNames.insert(memberName);
            
            if (getTypeTable()->worksAsTypeCn(memberTypeId)) {
                msgs->errorCnNoInit(memb->codeLoc, memberName);
                return NodeVal();
            }
                
            TypeTable::DataType::Member member;
            member.name = memberName;
            member.type = memberTypeId;
            dataType.addMember(member);

            memberTypes.push_back(memberType);
        }

        structType->setBody(memberTypes);
    }

    return NodeVal();
}

NodeVal Codegen::codegenBlock(const AstNode *ast) {
    if (!checkExactlyChildren(ast, 2, true)) {
        return NodeVal();
    }

    codegenAll(ast->children[1].get(), true);

    return NodeVal();
}

NodeVal Codegen::codegenAll(const AstNode *ast, bool makeScope) {
    if (!checkBlock(ast, true)) return NodeVal();

    ScopeControl scope(makeScope ? symbolTable : nullptr);

    for (const std::unique_ptr<AstNode> &child : ast->children) codegenNode(child.get());

    return NodeVal();
}

optional<FuncValue> Codegen::codegenFuncProto(const AstNode *ast, bool definition) {
    const AstNode *nodeName = ast->children[1].get();
    const AstNode *nodeArgs = ast->children[2].get();
    const AstNode *nodeRet = ast->children[3].get();

    // func name
    optional<NamePool::Id> name = getId(nodeName, true);
    if (!name.has_value()) return nullopt;

    if (!symbolTable->funcMayTakeName(name.value())) {
        msgs->errorFuncNameTaken(nodeName->codeLoc, name.value());
        return nullopt;
    }

    bool noNameMangle = name.value() == namePool->getMain();

    // func args
    vector<NameTypePair> args;
    bool variadic = false;
    if (!checkEmptyTerminal(nodeArgs, false)) {
        if (!checkNotTerminal(nodeArgs, true)) {
            return nullopt;
        }

        bool last = false;
        for (size_t i = 0; i < nodeArgs->children.size(); ++i) {
            const AstNode *child = nodeArgs->children[i].get();

            if (last) {
                msgs->errorNotLastParam(child->codeLoc);
                return nullopt;
            }

            if (checkEllipsis(child, false)) {
                variadic = last = true;
            } else {
                optional<NameTypePair> nameType = getIdTypePair(child, true);
                if (!nameType.has_value()) return nullopt;

                args.push_back(nameType.value());
            }
        }
    }

    // func ret
    optional<TypeTable::Id> retType;
    if (!checkEmptyTerminal(nodeRet, false)) {
        NodeVal type = codegenNode(nodeRet);
        if (!checkIsType(nodeRet->codeLoc, type, true)) return nullopt;

        retType = type.type;
    }

    // func attrs
    for (size_t i = 4; true; ++i) {
        if ((definition && i+1 >= ast->children.size()) ||
            (!definition && i >= ast->children.size()))
            break;
        
        const AstNode *child = ast->children[i].get();
        
        optional<Token::Attr> attr = getAttr(child, true);
        if (!attr.has_value()) return nullopt;

        if (attr.value() == Token::A_NO_NAME_MANGLE) {
            noNameMangle = true;
        } else {
            msgs->errorBadAttr(child->codeLoc, attr.value());
            return nullopt;
        }
    }

    FuncValue val;
    val.name = name.value();
    val.argNames = vector<NamePool::Id>(args.size());
    val.argTypes = vector<TypeTable::Id>(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        val.argNames[i] = args[i].first;
        val.argTypes[i] = args[i].second;
    }
    val.retType = retType;
    val.defined = definition;
    val.variadic = variadic;
    val.noNameMangle = noNameMangle;

    if (!symbolTable->canRegisterFunc(val)) {
        msgs->errorFuncSigConflict(ast->codeLoc);
        return nullopt;
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < args.size(); ++i) {
        for (size_t j = i+1; j < args.size(); ++j) {
            if (args[i].first == args[j].first) {
                msgs->errorFuncArgNameDuplicate(ast->codeLoc, args[j].first);
                return nullopt;
            }
        }
    }

    if (!val.noNameMangle) {
        optional<NamePool::Id> mangled = mangleName(val);
        if (!mangled) {
            msgs->errorInternal(ast->codeLoc);
            return nullopt;
        }
        name = mangled.value();
    }

    llvm::Function *func = symbolTable->getFunction(val);
    if (func == nullptr) {
        vector<llvm::Type*> argTypes(args.size());
        for (size_t i = 0; i < argTypes.size(); ++i)
            argTypes[i] = getType(args[i].second);
        llvm::Type *llvmRetType = retType.has_value() ? getType(retType.value()) : llvm::Type::getVoidTy(llvmContext);
        llvm::FunctionType *funcType = llvm::FunctionType::get(llvmRetType, argTypes, val.variadic);

        // TODO optimize on const args
        func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                namePool->get(name.value()), llvmModule.get());
        
        size_t i = 0;
        for (auto &arg : func->args()) {
            arg.setName(namePool->get(args[i].first));
            ++i;
        }
    }

    val.func = func;

    return symbolTable->registerFunc(val);
}

// TODO when 'else ret ...;' is the final instruction in a function, llvm gives a warning
//   + 'while (true) ret ...;' gives a segfault
NodeVal Codegen::codegenFunc(const AstNode *ast) {
    if (!checkGlobalScope(ast->codeLoc, true) ||
        !checkAtLeastChildren(ast, 4, true)) {
        return NodeVal();
    }

    bool definition = ast->children.size() >= 5 && checkBlock(ast->children.back().get(), false);

    optional<FuncValue> funcValRet = codegenFuncProto(ast, definition);
    if (!funcValRet.has_value()) return NodeVal();

    if (!definition) return NodeVal();

    const FuncValue *funcVal = &funcValRet.value();

    ScopeControl scope(*symbolTable, *funcVal);

    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", funcVal->func));

    llvm::BasicBlock *body = llvm::BasicBlock::Create(llvmContext, "entry", funcVal->func);
    llvmBuilder.SetInsertPoint(body);

    size_t i = 0;
    for (auto &arg : funcVal->func->args()) {
        TypeTable::Id astArgType = funcVal->argTypes[i];
        NamePool::Id astArgName = funcVal->argNames[i];
        const string &name = namePool->get(astArgName);
        
        llvm::AllocaInst *alloca = createAlloca(getType(astArgType), name);
        llvmBuilder.CreateStore(&arg, alloca);
        symbolTable->addVar(astArgName, {astArgType, alloca});

        ++i;
    }

    codegenAll(ast->children.back().get(), false);
    if (msgs->isAbort()) {
        funcVal->func->eraseFromParent();
        return NodeVal();
    }

    llvmBuilderAlloca.CreateBr(body);

    if (!funcVal->hasRet() && !isBlockTerminated())
            llvmBuilder.CreateRetVoid();

    if (llvm::verifyFunction(*funcVal->func, &llvm::errs())) cerr << endl;

    return NodeVal();
}