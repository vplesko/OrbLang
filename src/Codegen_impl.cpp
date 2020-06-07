#include "Codegen.h"
#include <unordered_set>
#include <iostream>
#include "Evaluator.h"
#include "llvm/IR/Verifier.h"
using namespace std;

NodeVal Codegen::codegenNode(const AstNode *ast) {
    NodeVal ret;

    if (ast->kind == AstNode::Kind::kTerminal) {
        ret = evaluator->evaluateTerminal(ast);

        if (ret.kind == NodeVal::Kind::kId && !ast->escaped) {
            const TerminalVal &term = ast->terminal.value();

            if (getTypeTable()->isType(term.id)) {
                TypeTable::Id type = getTypeTable()->getTypeId(term.id).value();
                ret = NodeVal(NodeVal::Kind::kType);
                ret.type = type;
            } else if (symbolTable->isVarName(term.id)) {
                ret = codegenVar(ast);
            } else if (symbolTable->isFuncName(term.id)) {
                ret = NodeVal(NodeVal::Kind::kFuncId);
                ret.id = term.id;
            } else if (symbolTable->isMacroName(term.id)) {
                ret = NodeVal(NodeVal::Kind::kMacroId);
                ret.id = term.id;
            } else {
                msgs->errorVarNotFound(ast->codeLoc, term.id);
                return NodeVal();
            }
        }
    } else {
        NodeVal starting = codegenNode(ast->children[0].get());
        if (starting.kind == NodeVal::Kind::kInvalid) return NodeVal();

        if (starting.isMacroId()) {
            unique_ptr<AstNode> expanded = evaluator->evaluateInvoke(ast);
            if (expanded == nullptr) return NodeVal();
            return codegenNode(expanded.get());
        }

        if (starting.isKeyword()) {
            switch (starting.keyword) {
            case Token::T_IMPORT:
                ret = codegenImport(ast);
                break;
            case Token::T_LET:
                ret = codegenLet(ast);
                break;
            case Token::T_BLOCK:
                ret = codegenBlock(ast);
                break;
            case Token::T_EXIT:
                ret = codegenExit(ast);
                break;
            case Token::T_PASS:
                ret = codegenPass(ast);
                break;
            case Token::T_LOOP:
                ret = codegenLoop(ast);
                break;
            case Token::T_RET:
                ret = codegenRet(ast);
                break;
            case Token::T_FNC:
                ret = codegenFunc(ast);
                break;
            case Token::T_CAST:
                ret = codegenCast(ast);
                break;
            case Token::T_ARR:
                ret = codegenArr(ast);
                break;
            default:
                msgs->errorUnexpectedKeyword(ast->children[0].get()->codeLoc, starting.keyword);
                return NodeVal();
            }
        } else if (starting.isType()) {
            ret = codegenType(ast, starting);
        } else {
            ret = codegenExpr(ast, starting);
        }
    }

    if (!ast->escaped && ast->type.has_value()) {
        const AstNode *nodeType = ast->type.value().get();

        NodeVal nodeTypeVal = codegenNode(nodeType);
        if (!checkIsType(nodeType->codeLoc, nodeTypeVal, true))
            return NodeVal();

        switch (ret.kind) {
        case NodeVal::Kind::kLlvmVal:
            if (ret.type != nodeTypeVal.type) {
                msgs->errorMismatchTypeAnnotation(nodeType->codeLoc, nodeTypeVal.type);
                return NodeVal();
            }
            break;
        case NodeVal::Kind::kUntyVal:
            if (!promoteUntyped(ret, nodeTypeVal.type)) {
                msgs->errorExprCannotPromote(ast->codeLoc, nodeTypeVal.type);
                return NodeVal();
            }
            break;
        case NodeVal::Kind::kInvalid:
            // do nothing
            break;
        default:
            msgs->errorMismatchTypeAnnotation(nodeType->codeLoc, nodeTypeVal.type);
            return NodeVal();
        }
    }

    return ret;
}

NodeVal Codegen::codegenAll(const AstNode *ast) {
    if (!checkBlock(ast, true)) return NodeVal();
    if (ast->type.has_value()) {
        msgs->errorMismatchTypeAnnotation(ast->type.value()->codeLoc);
        return NodeVal();
    }

    for (const std::unique_ptr<AstNode> &child : ast->children) {
        if (!checkEmptyTerminal(child.get(), false) && !checkNotTerminal(child.get(), true)) return NodeVal();
        
        codegenNode(child.get());
    }

    return NodeVal();
}

NodeVal Codegen::codegenConversion(const AstNode *ast, TypeTable::Id t) {
    NodeVal value = codegenNode(ast);
    if (!checkValueUnbroken(ast->codeLoc, value, true)) return NodeVal();
    if (value.isUntyVal() && !promoteUntyped(value, t)) {
        msgs->errorExprCannotPromote(ast->codeLoc, t);
        return NodeVal();
    }
    if (value.llvmVal.type != t) {
        if (!getTypeTable()->isImplicitCastable(value.llvmVal.type, t)) {
            msgs->errorExprCannotImplicitCast(ast->codeLoc, value.llvmVal.type, t);
            return NodeVal();
        }
        if (!createCast(value, t)) {
            msgs->errorInternal(ast->codeLoc);
            return NodeVal();
        }
    }
    return value;
}

optional<NodeVal> Codegen::codegenTypeDescr(const AstNode *ast, const NodeVal &first) {
    if (ast->children.size() < 2) return nullopt;

    const AstNode *nodeChild = ast->children[1].get();

    optional<Token::Type> keyw = getKeyword(nodeChild, false);
    optional<Token::Oper> op = getOper(nodeChild, false);
    optional<UntypedVal> val = getUntypedVal(nodeChild, false);

    if (!keyw.has_value() && !op.has_value() && !val.has_value())
        return nullopt;
    
    TypeTable::TypeDescr typeDescr(first.type);
    for (size_t i = 1; i < ast->children.size(); ++i) {
        nodeChild = ast->children[i].get();

        keyw = getKeyword(nodeChild, false);
        op = getOper(nodeChild, false);
        val = getUntypedVal(nodeChild, false);

        if (op.has_value() && op == Token::O_MUL) {
            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_PTR});
        } else if (op.has_value() && op == Token::O_IND) {
            typeDescr.addDecor({TypeTable::TypeDescr::Decor::D_ARR_PTR});
        } else if (val.has_value()) {
            if (val.value().kind != UntypedVal::Kind::kSint) {
                msgs->errorInvalidTypeDecorator(nodeChild->codeLoc);
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
            msgs->errorInvalidTypeDecorator(nodeChild->codeLoc);
            return NodeVal();
        }
    }

    TypeTable::Id typeId = symbolTable->getTypeTable()->addTypeDescr(move(typeDescr));

    NodeVal ret(NodeVal::Kind::kType);
    ret.type = typeId;
    return ret;
}

NodeVal Codegen::codegenType(const AstNode *ast, const NodeVal &first) {
    if (ast->children.size() == 1) {
        NodeVal ret(NodeVal::Kind::kType);
        ret.type = first.type;
        return ret;
    }
    
    optional<NodeVal> typeDescr = codegenTypeDescr(ast, first);
    if (typeDescr.has_value()) return typeDescr.value();

    TypeTable::Tuple tup;
    tup.members.resize(ast->children.size());

    tup.members[0] = first.type;
    for (size_t i = 1; i < ast->children.size(); ++i) {
        const AstNode *nodeChild = ast->children[i].get();

        optional<TypeTable::Id> memb = getType(nodeChild, true);
        if (!memb.has_value()) return NodeVal();

        tup.members[i] = memb.value();
    }

    optional<TypeTable::Id> tupTypeId = getTypeTable()->addTuple(move(tup));
    if (!tupTypeId.has_value()) return NodeVal();

    NodeVal ret(NodeVal::Kind::kType);
    ret.type = tupTypeId.value();
    return ret;
}

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
                llvmBuilder.CreatePtrToInt(val, getLlvmType(getPrimTypeId(TypeTable::WIDEST_I))),
                llvm::ConstantInt::get(getLlvmType(getPrimTypeId(TypeTable::WIDEST_I)), 0),
                "p2b_cast");
        } else {
            val = nullptr;
            return false;
        }
    } else if (getTypeTable()->worksAsTypeArr(srcTypeId) || getTypeTable()->isTuple(srcTypeId)) {
        // NOTE tuples and arrs are only castable when changing constness
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
    return createCast(val, srcTypeId, getLlvmType(dstTypeId), dstTypeId);
}

bool Codegen::createCast(NodeVal &e, TypeTable::Id t) {
    if (!e.isLlvmVal()) return false;

    if (!createCast(e.llvmVal.val, e.llvmVal.type, t)) return false;
    e.llvmVal.type = t;
    return true;
}

NodeVal Codegen::codegenImport(const AstNode *ast) {
    if (!checkGlobalScope(ast->codeLoc, true) ||
        !checkExactlyChildren(ast, 2, true))
        return NodeVal();
    
    const AstNode *nodeFile = ast->children[1].get();

    optional<UntypedVal> val = getUntypedVal(nodeFile, true);
    if (!val.has_value()) return NodeVal();

    if (val.value().kind != UntypedVal::Kind::kString) {
        msgs->errorImportNotString(nodeFile->codeLoc);
        return NodeVal();
    }

    NodeVal ret(NodeVal::Kind::kImport);
    ret.file = val.value().val_str;
    return ret;
}

NodeVal Codegen::codegenLet(const AstNode *ast) {
    if (!checkAtLeastChildren(ast, 2, true))
        return NodeVal();
    
    for (size_t i = 1; i < ast->children.size(); ++i) {
        const AstNode *childNode = ast->children[i].get();

        if (!childNode->isTerminal() && !checkExactlyChildren(childNode, 2, true))
            continue;
        
        const AstNode *nameTypeNode = childNode->isTerminal() ? childNode : childNode->children[0].get();
        bool hasInit = !childNode->isTerminal();

        NamePool::Id name;
        CodeLoc codeLocName = nameTypeNode->codeLoc;
        {
            optional<NamePool::Id> optName = getId(nameTypeNode, true);
            if (!optName.has_value()) continue;

            name = optName.value();
            if (!symbolTable->varMayTakeName(name)) {
                msgs->errorVarNameTaken(codeLocName, name);
                continue;
            }
        }

        optional<TypeTable::Id> optType;
        CodeLoc codeLocType;
        if (nameTypeNode->hasType()) {
            optType = getType(nameTypeNode->type.value().get(), true);
            codeLocType = nameTypeNode->type.value()->codeLoc;
            if (!optType.has_value()) continue;

            if (!hasInit && getTypeTable()->worksAsTypeCn(optType.value())) {
                msgs->errorCnNoInit(codeLocName, name);
                continue;
            }
        } else if (!hasInit) {
            msgs->errorMissingTypeAnnotation(codeLocName);
            continue;
        }

        llvm::Value *llvmInitVal = nullptr;
        TypeTable::Id initType;
        CodeLoc codeLocInit;
        if (hasInit) {
            codeLocInit = childNode->children[1]->codeLoc;

            NodeVal initVal;
            if (isGlobalScope()) initVal = evaluator->evaluateNode(childNode->children[1].get());
            else initVal = codegenNode(childNode->children[1].get());
            if (!checkValueUnbroken(codeLocInit, initVal, true))
                continue;
            
            if (!optType.has_value() && initVal.isUntyVal()) {
                msgs->errorMissingTypeAnnotation(codeLocName);
                continue;
            }

            if (initVal.isUntyVal()) {
                if (!promoteUntyped(initVal, optType.value())) {
                    msgs->errorExprCannotPromote(codeLocInit, optType.value());
                    continue;
                }
            }

            llvmInitVal = initVal.llvmVal.val;
            initType = initVal.llvmVal.type;
            if (!optType.has_value()) optType = initType;
        }

        llvm::Type *llvmType = getLlvmType(optType.value());
        if (llvmType == nullptr) return NodeVal();


        if (hasInit && initType != optType.value()) {
            if (!getTypeTable()->isImplicitCastable(initType, optType.value())) {
                msgs->errorExprCannotImplicitCast(codeLocInit, initType, optType.value());
                continue;
            }

            createCast(llvmInitVal, initType, llvmType, optType.value());
            initType = optType.value();
        }

        llvm::Value *llvmVar;
        if (isGlobalScope()) {
            llvmVar = createGlobal(
                llvmType,
                (llvm::Constant*) llvmInitVal,
                getTypeTable()->worksAsTypeCn(optType.value()),
                namePool->get(name));
        } else {
            llvmVar = createAlloca(llvmType, namePool->get(name));
            if (hasInit) llvmBuilder.CreateStore(llvmInitVal, llvmVar);
        }

        symbolTable->addVar(name, {optType.value(), llvmVar});
    }

    return NodeVal();
}

NodeVal Codegen::codegenExit(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 3, true)) {
        return NodeVal();
    }

    bool hasName = ast->children.size() == 3;

    const AstNode *nodeName = hasName ? ast->children[1].get() : nullptr;
    const AstNode *nodeCond = ast->children[hasName ? 2 : 1].get();

    optional<NamePool::Id> name;
    if (hasName) {
        name = getId(nodeName, true);
        if (!name.has_value()) return NodeVal();
    }

    SymbolTable::Block *targetBlock;
    if (hasName) targetBlock = getBlock(nodeName->codeLoc, name.value(), true);
    else targetBlock = symbolTable->getLastBlock();
    if (isGlobalScope() || targetBlock == nullptr) {
        msgs->errorExitNowhere(ast->codeLoc);
        return NodeVal();
    }
    llvm::BasicBlock *blockExit = targetBlock->blockExit;

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    NodeVal valCond = codegenNode(nodeCond);
    if (!checkValueUnbroken(nodeCond->codeLoc, valCond, true)) return NodeVal();

    if (targetBlock->type.has_value()) {
        msgs->errorExitPassingBlock(ast->codeLoc);
        return NodeVal();
    }

    if (valCond.isUntyVal()) {
        if (valCond.untyVal.kind != UntypedVal::Kind::kBool) {
            msgs->errorExprCannotPromote(nodeCond->codeLoc, getPrimTypeId(TypeTable::P_BOOL));
            return NodeVal();
        }

        if (valCond.untyVal.val_b) {
            llvm::BasicBlock *blockAfter = llvm::BasicBlock::Create(llvmContext, "after");
            llvmBuilder.CreateBr(blockExit);
            func->getBasicBlockList().push_back(blockAfter);
            llvmBuilder.SetInsertPoint(blockAfter);
        }
    } else {
        llvm::BasicBlock *blockAfter = llvm::BasicBlock::Create(llvmContext, "after");
        llvmBuilder.CreateCondBr(valCond.llvmVal.val, blockExit, blockAfter);
        func->getBasicBlockList().push_back(blockAfter);
        llvmBuilder.SetInsertPoint(blockAfter);
    }

    return NodeVal();
}

NodeVal Codegen::codegenPass(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 3, true)) {
        return NodeVal();
    }

    bool hasName = ast->children.size() == 3;

    const AstNode *nodeName = hasName ? ast->children[1].get() : nullptr;
    const AstNode *nodeValue = ast->children[hasName ? 2 : 1].get();

    optional<NamePool::Id> name;
    if (hasName) {
        name = getId(nodeName, true);
        if (!name.has_value()) return NodeVal();
    }

    SymbolTable::Block *targetBlock;
    if (hasName) targetBlock = getBlock(nodeName->codeLoc, name.value(), true);
    else targetBlock = symbolTable->getLastBlock();
    if (isGlobalScope() || targetBlock == nullptr) {
        msgs->errorExitNowhere(ast->codeLoc);
        return NodeVal();
    }
    llvm::BasicBlock *blockExit = targetBlock->blockExit;

    if (!targetBlock->type.has_value()) {
        msgs->errorPassNonPassingBlock(nodeValue->codeLoc);
        return NodeVal();
    }

    TypeTable::Id expectedType = targetBlock->type.value();

    NodeVal value = codegenConversion(nodeValue, expectedType);
    if (value.isInvalid()) return NodeVal();

    targetBlock->phi->addIncoming(value.llvmVal.val, llvmBuilder.GetInsertBlock());
    llvmBuilder.CreateBr(blockExit);

    return NodeVal();
}

NodeVal Codegen::codegenLoop(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 3, true)) {
        return NodeVal();
    }

    bool hasName = ast->children.size() == 3;

    const AstNode *nodeName = hasName ? ast->children[1].get() : nullptr;
    const AstNode *nodeCond = ast->children[hasName ? 2 : 1].get();

    optional<NamePool::Id> name;
    if (hasName) {
        name = getId(nodeName, true);
        if (!name.has_value()) return NodeVal();
    }

    SymbolTable::Block *targetBlock;
    if (hasName) targetBlock = getBlock(nodeName->codeLoc, name.value(), true);
    else targetBlock = symbolTable->getLastBlock();
    if (isGlobalScope() || targetBlock == nullptr) {
        msgs->errorLoopNowhere(ast->codeLoc);
        return NodeVal();
    }
    llvm::BasicBlock *blockLoop = targetBlock->blockLoop;

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    NodeVal valCond = codegenNode(nodeCond);
    if (!checkValueUnbroken(nodeCond->codeLoc, valCond, true)) return NodeVal();

    if (valCond.isUntyVal()) {
        if (valCond.untyVal.kind != UntypedVal::Kind::kBool) {
            msgs->errorExprCannotPromote(nodeCond->codeLoc, getPrimTypeId(TypeTable::P_BOOL));
            return NodeVal();
        }

        if (valCond.untyVal.val_b) {
            llvm::BasicBlock *blockAfter = llvm::BasicBlock::Create(llvmContext, "after");
            llvmBuilder.CreateBr(blockLoop);
            func->getBasicBlockList().push_back(blockAfter);
            llvmBuilder.SetInsertPoint(blockAfter);
        }
    } else {
        llvm::BasicBlock *blockAfter = llvm::BasicBlock::Create(llvmContext, "after");
        llvmBuilder.CreateCondBr(valCond.llvmVal.val, blockLoop, blockAfter);
        func->getBasicBlockList().push_back(blockAfter);
        llvmBuilder.SetInsertPoint(blockAfter);
    }

    return NodeVal();
}

NodeVal Codegen::codegenRet(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 1, 2, true)) {
        return NodeVal();
    }

    bool hasVal = ast->children.size() == 2;

    const AstNode *nodeVal = hasVal ? ast->children[1].get() : nullptr;

    if (hasVal && checkEmptyTerminal(nodeVal, false)) hasVal = false;

    optional<FuncValue> currFunc = symbolTable->getCurrFunc();
    if (!currFunc.has_value()) {
        msgs->errorUnexpectedKeyword(ast->codeLoc, Token::T_RET);
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

    // TODO allow ret (); to be same as ret;
    NodeVal retExpr = codegenConversion(nodeVal, currFunc.value().retType.value());
    if (retExpr.isInvalid()) return NodeVal();

    llvmBuilder.CreateRet(retExpr.llvmVal.val);

    return NodeVal();
}

NodeVal Codegen::codegenBlock(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 2, 4, true)) {
        return NodeVal();
    }

    bool hasName = false, hasType = false;
    size_t indName, indType, indBody;
    if (ast->children.size() == 2) {
        indBody = 1;
    } else if (ast->children.size() == 3) {
        hasType = true;
        indType = 1;
        indBody = 2;
    } else {
        hasName = true;
        hasType = true;
        indName = 1;
        indType = 2;
        indBody = 3;
    }

    const AstNode *nodeName = hasName ? ast->children[indName].get() : nullptr;
    const AstNode *nodeType = hasType ? ast->children[indType].get() : nullptr;
    const AstNode *nodeBody = ast->children[indBody].get();

    optional<NamePool::Id> name;
    if (hasName) {
        name = getId(nodeName, true);
        if (!name.has_value()) return NodeVal();
    }

    optional<TypeTable::Id> type;
    if (hasType) {
        if (!checkEmptyTerminal(nodeType, false)) {
            type = getType(nodeType, true);
            if (!type.has_value()) return NodeVal();
        } else {
            hasType = false;
        }
    }

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body", func);
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(bodyBlock);

    llvm::PHINode *phi = nullptr;
    if (hasType) {
        llvmBuilder.SetInsertPoint(afterBlock);
        phi = llvmBuilder.CreatePHI(getLlvmType(type.value()), 0, "block_val");
    }

    llvmBuilder.SetInsertPoint(bodyBlock);

    {
        SymbolTable::Block blockOpen;
        if (hasName) blockOpen.name = name;
        if (hasType) {
            blockOpen.type = type.value();
            blockOpen.phi = phi;
        }
        blockOpen.blockExit = afterBlock;
        blockOpen.blockLoop = bodyBlock;
        BlockControl blockCtrl(symbolTable, blockOpen);

        codegenAll(nodeBody);

        if (!isLlvmBlockTerminated()) {
            if (hasType) {
                msgs->errorBlockNoPass(nodeBody->codeLoc);
                return NodeVal();
            }

            llvmBuilder.CreateBr(afterBlock);
        }
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);

    if (hasType) {
        NodeVal ret(NodeVal::Kind::kLlvmVal);
        ret.llvmVal.type = type.value();
        ret.llvmVal.val = phi;
        return ret;
    }

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
        msgs->errorSigConflict(ast->codeLoc);
        return nullopt;
    }

    // can't have args with same name
    for (size_t i = 0; i+1 < args.size(); ++i) {
        for (size_t j = i+1; j < args.size(); ++j) {
            if (args[i].first == args[j].first) {
                msgs->errorArgNameDuplicate(ast->codeLoc, args[j].first);
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
            argTypes[i] = getLlvmType(args[i].second);
        llvm::Type *llvmRetType = retType.has_value() ? getLlvmType(retType.value()) : llvm::Type::getVoidTy(llvmContext);
        llvm::FunctionType *funcType = llvm::FunctionType::get(llvmRetType, argTypes, val.variadic);

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

// TODO when 'while true { ret ...; }' is the final instruction in a function, llvm gives a warning
//  that would require tracking if all code paths return, and there's the same problem with pass in blocks
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

    BlockControl blockCtrl(*symbolTable, *funcVal);

    llvmBuilderAlloca.SetInsertPoint(llvm::BasicBlock::Create(llvmContext, "alloca", funcVal->func));

    llvm::BasicBlock *body = llvm::BasicBlock::Create(llvmContext, "entry", funcVal->func);
    llvmBuilder.SetInsertPoint(body);

    size_t i = 0;
    for (auto &arg : funcVal->func->args()) {
        TypeTable::Id astArgType = funcVal->argTypes[i];
        NamePool::Id astArgName = funcVal->argNames[i];
        const string &name = namePool->get(astArgName);
        
        llvm::AllocaInst *alloca = createAlloca(getLlvmType(astArgType), name);
        llvmBuilder.CreateStore(&arg, alloca);
        symbolTable->addVar(astArgName, {astArgType, alloca});

        ++i;
    }

    codegenAll(ast->children.back().get());
    if (msgs->isFail()) {
        funcVal->func->eraseFromParent();
        return NodeVal();
    }

    llvmBuilderAlloca.CreateBr(body);

    if (!funcVal->hasRet() && !isLlvmBlockTerminated())
            llvmBuilder.CreateRetVoid();

    if (llvm::verifyFunction(*funcVal->func, &llvm::errs())) cerr << endl;
    llvmFPM->run(*funcVal->func);

    return NodeVal();
}