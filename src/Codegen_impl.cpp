#include "Codegen.h"
#include <unordered_set>
#include <iostream>
#include "Evaluator.h"
#include "llvm/IR/Verifier.h"
using namespace std;

std::unique_ptr<AstNode> Codegen::processInvoke(NamePool::Id macroName, const AstNode *ast) {
    return evaluator->processInvoke(macroName, ast);
}

NodeVal Codegen::processType(const AstNode *ast, const NodeVal &first) {
    return evaluator->processType(ast, first);
}

NodeVal Codegen::handleImplicitConversion(CodeLoc codeLoc, NodeVal val, TypeTable::Id t) {
    if (!checkValueUnbroken(codeLoc, val, true)) return NodeVal();

    if (val.isKnownVal() && !promoteKnownVal(val, t)) {
        msgs->errorExprCannotPromote(codeLoc, t);
        return NodeVal();
    }
    if (val.llvmVal.type != t) {
        if (!getTypeTable()->isImplicitCastable(val.llvmVal.type, t)) {
            msgs->errorExprCannotImplicitCast(codeLoc, val.llvmVal.type, t);
            return NodeVal();
        }
        if (!createCast(val, t)) {
            msgs->errorInternal(codeLoc);
            return NodeVal();
        }
    }
    return val;
}

NodeVal Codegen::handleImplicitConversion(const AstNode *ast, TypeTable::Id t) {
    NodeVal value = processNode(ast);
    return handleImplicitConversion(ast->codeLoc, value, t);
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
            val = llvmBuilder.CreateBitOrPointerCast(val, type, "i2p_cast");
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
            val = llvmBuilder.CreateBitOrPointerCast(val, type, "u2p_cast");
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

NodeVal Codegen::processEvalNode(const AstNode *ast) {
    return evaluator->processNode(ast);
}

NodeVal Codegen::processSym(const AstNode *ast) {
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
            if (isGlobalScope()) initVal = processEvalNode(childNode->children[1].get());
            else initVal = processNode(childNode->children[1].get());
            if (!checkValueUnbroken(codeLocInit, initVal, true))
                continue;

            if (initVal.isKnownVal()) {
                if (optType.has_value()) {
                    if (!promoteKnownVal(initVal, optType.value())) {
                        msgs->errorExprCannotPromote(codeLocInit, optType.value());
                        continue;
                    }
                } else {
                    promoteKnownVal(initVal);
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

void Codegen::handleExit(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond) {
    if (cond.isKnownVal()) {
        if (!promoteKnownVal(cond, getPrimTypeId(TypeTable::P_BOOL))) {
            msgs->errorExprCannotPromote(condCodeLoc, getPrimTypeId(TypeTable::P_BOOL));
            return;
        }
    }

    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();
    llvm::BasicBlock *blockExit = targetBlock->blockExit;

    llvm::BasicBlock *blockAfter = llvm::BasicBlock::Create(llvmContext, "after");
    llvmBuilder.CreateCondBr(cond.llvmVal.val, blockExit, blockAfter);
    func->getBasicBlockList().push_back(blockAfter);
    llvmBuilder.SetInsertPoint(blockAfter);
}

void Codegen::handleLoop(const SymbolTable::Block *targetBlock, CodeLoc condCodeLoc, NodeVal cond) {
    if (cond.isKnownVal()) {
        if (!promoteKnownVal(cond, getPrimTypeId(TypeTable::P_BOOL))) {
            msgs->errorExprCannotPromote(condCodeLoc, getPrimTypeId(TypeTable::P_BOOL));
            return;
        }
    }
    
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();
    llvm::BasicBlock *blockLoop = targetBlock->blockLoop;

    llvm::BasicBlock *blockAfter = llvm::BasicBlock::Create(llvmContext, "after");
    llvmBuilder.CreateCondBr(cond.llvmVal.val, blockLoop, blockAfter);
    func->getBasicBlockList().push_back(blockAfter);
    llvmBuilder.SetInsertPoint(blockAfter);
}

void Codegen::handlePass(const SymbolTable::Block *targetBlock, CodeLoc valCodeLoc, NodeVal val, TypeTable::Id expectedType) {
    NodeVal value = handleImplicitConversion(valCodeLoc, val, expectedType);
    if (value.isInvalid()) return;
    
    llvm::BasicBlock *blockExit = targetBlock->blockExit;
    targetBlock->phi->addIncoming(value.llvmVal.val, llvmBuilder.GetInsertBlock());
    llvmBuilder.CreateBr(blockExit);
}

NodeVal Codegen::processRet(const AstNode *ast) {
    if (!checkBetweenChildren(ast, 1, 2, true)) {
        return NodeVal();
    }

    bool hasVal = ast->children.size() == 2;

    const AstNode *nodeVal = hasVal ? ast->children[1].get() : nullptr;

    if (hasVal && checkIsEmptyTerminal(nodeVal, false)) hasVal = false;

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

    NodeVal retExpr = handleImplicitConversion(nodeVal, currFunc.value().retType.value());
    if (retExpr.isInvalid()) return NodeVal();

    llvmBuilder.CreateRet(retExpr.llvmVal.val);

    return NodeVal();
}

NodeVal Codegen::handleBlock(std::optional<NamePool::Id> name, std::optional<TypeTable::Id> type, const AstNode *nodeBody) {
    llvm::Function *func = llvmBuilder.GetInsertBlock()->getParent();

    llvm::BasicBlock *bodyBlock = llvm::BasicBlock::Create(llvmContext, "body", func);
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(llvmContext, "after");

    llvmBuilder.CreateBr(bodyBlock);

    llvm::PHINode *phi = nullptr;
    if (type.has_value()) {
        llvmBuilder.SetInsertPoint(afterBlock);
        phi = llvmBuilder.CreatePHI(getLlvmType(type.value()), 0, "block_val");
    }

    llvmBuilder.SetInsertPoint(bodyBlock);

    {
        SymbolTable::Block blockOpen;
        if (name.has_value()) blockOpen.name = name;
        if (type.has_value()) {
            blockOpen.type = type.value();
            blockOpen.phi = phi;
        }
        blockOpen.blockExit = afterBlock;
        blockOpen.blockLoop = bodyBlock;
        BlockControl blockCtrl(symbolTable, blockOpen);

        processAll(nodeBody);

        if (!isLlvmBlockTerminated()) {
            if (type.has_value()) {
                msgs->errorBlockNoPass(nodeBody->codeLoc);
                return NodeVal();
            }

            llvmBuilder.CreateBr(afterBlock);
        }
    }

    func->getBasicBlockList().push_back(afterBlock);
    llvmBuilder.SetInsertPoint(afterBlock);

    if (type.has_value()) {
        NodeVal ret(NodeVal::Kind::kLlvmVal);
        ret.llvmVal.type = type.value();
        ret.llvmVal.val = phi;
        return ret;
    }

    return NodeVal();
}

optional<FuncValue> Codegen::calculateFuncProto(const AstNode *ast, bool definition) {
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
    vector<Evaluator::NameTypePair> args;
    bool variadic = false;
    if (!checkIsEmptyTerminal(nodeArgs, false)) {
        if (!checkIsNotTerminal(nodeArgs, true)) {
            return nullopt;
        }

        bool last = false;
        for (size_t i = 0; i < nodeArgs->children.size(); ++i) {
            const AstNode *child = nodeArgs->children[i].get();

            if (last) {
                msgs->errorNotLastParam(child->codeLoc);
                return nullopt;
            }

            if (checkIsEllipsis(child, false)) {
                variadic = last = true;
            } else {
                optional<Evaluator::NameTypePair> nameType = getIdTypePair(child, true);
                if (!nameType.has_value()) return nullopt;

                args.push_back(nameType.value());
            }
        }
    }

    // func ret
    optional<TypeTable::Id> retType;
    if (!checkIsEmptyTerminal(nodeRet, false)) {
        NodeVal type = processNode(nodeRet);
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
NodeVal Codegen::processFunc(const AstNode *ast) {
    if (!checkGlobalScope(ast->codeLoc, true) ||
        !checkAtLeastChildren(ast, 4, true)) {
        return NodeVal();
    }

    bool definition = ast->children.size() >= 5 && checkIsBlock(ast->children.back().get(), false);

    optional<FuncValue> funcValRet = calculateFuncProto(ast, definition);
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

    processAll(ast->children.back().get());
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

NodeVal Codegen::processMac(AstNode *ast) {
    return evaluator->processMac(ast);
}