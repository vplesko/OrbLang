#include "Processor.h"
#include "Reserved.h"
#include "Evaluator.h"
using namespace std;

Processor::Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs)
    : namePool(namePool), stringPool(stringPool), typeTable(typeTable), symbolTable(symbolTable), msgs(msgs), compiler(nullptr), evaluator(nullptr), topmost(0) {
}

NodeVal Processor::processNode(const NodeVal &node) {
    unsigned oldTopmost = topmost;
    if (topmost < 2) ++topmost;
    DeferredCallback guard([&, oldTopmost] { topmost = oldTopmost; });

    if (node.hasTypeAttr() || node.hasNonTypeAttrs()) {
        NodeVal procAttrs = node;
        if (!processAttributes(procAttrs)) return NodeVal();

        NodeVal ret;
        if (NodeVal::isLeaf(procAttrs, typeTable)) ret = processLeaf(procAttrs);
        else ret = processNonLeaf(procAttrs);
        if (ret.isInvalid()) return NodeVal();

        if (procAttrs.hasTypeAttr()) ret.setTypeAttr(move(procAttrs.getTypeAttr()));
        if (procAttrs.hasNonTypeAttrs()) ret.setNonTypeAttrs(move(procAttrs.getNonTypeAttrs()));
        return ret;
    } else {
        if (NodeVal::isLeaf(node, typeTable)) return processLeaf(node);
        else return processNonLeaf(node);
    }
}

NodeVal Processor::processLeaf(const NodeVal &node) {
    NodeVal prom = node.isLiteralVal() ? promoteLiteralVal(node) : node;

    if (!prom.isEscaped() && prom.isEvalVal() && EvalVal::isId(prom.getEvalVal(), typeTable)) {
        return processId(prom);
    }

    // unescaping since the node has been processed (even if it may have otherwise been a no-op)
    NodeVal::unescape(prom, typeTable);
    return prom;
}

NodeVal Processor::processNonLeaf(const NodeVal &node) {
    if (node.isEscaped()) {
        return processNonLeafEscaped(node);
    }

    NodeVal starting = processNode(node.getChild(0));
    if (starting.isInvalid()) return NodeVal();

    if (NodeVal::isMacro(starting, typeTable)) {
        NodeVal invoked = processInvoke(node, starting);
        if (invoked.isInvalid()) return NodeVal();

        return processNode(invoked);
    }

    if (starting.isEvalVal() && EvalVal::isId(starting.getEvalVal(), typeTable)) {
        return processId(node, starting);
    }

    if (starting.isEvalVal() && EvalVal::isType(starting.getEvalVal(), typeTable)) {
        return processType(node, starting);
    }

    if (NodeVal::isFunc(starting, typeTable)) {
        return processCall(node, starting);
    }

    if (starting.isSpecialVal()) {
        optional<Keyword> keyw = getKeyword(starting.getSpecialVal().id);
        if (keyw.has_value()) {
            switch (keyw.value()) {
            case Keyword::SYM:
                return processSym(node);
            case Keyword::CAST:
                return processCast(node);
            case Keyword::BLOCK:
                return processBlock(node);
            case Keyword::EXIT:
                return processExit(node);
            case Keyword::LOOP:
                return processLoop(node);
            case Keyword::PASS:
                return processPass(node);
            case Keyword::CUSTOM:
                return processCustom(node);
            case Keyword::DATA:
                return processData(node);
            case Keyword::FNC:
                return processFnc(node);
            case Keyword::RET:
                return processRet(node);
            case Keyword::MAC:
                return processMac(node);
            case Keyword::EVAL:
                return processEval(node);
            case Keyword::TUP:
                return processTup(node);
            case Keyword::TYPE_OF:
                return processTypeOf(node);
            case Keyword::LEN_OF:
                return processLenOf(node);
            case Keyword::SIZE_OF:
                return processSizeOf(node);
            case Keyword::IS_DEF:
                return processIsDef(node);
            case Keyword::ATTR_OF:
                return processAttrOf(node);
            case Keyword::ATTR_IS_DEF:
                return processAttrIsDef(node);
            case Keyword::IMPORT:
                return processImport(node);
            default:
                msgs->errorUnexpectedKeyword(starting.getCodeLoc(), keyw.value());
                return NodeVal();
            }
        }

        optional<Oper> op = getOper(starting.getSpecialVal().id);
        if (op.has_value()) {
            return processOper(node, op.value());
        }

        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    msgs->errorUnknown(node.getCodeLoc());
    return NodeVal();
}

NodeVal Processor::processNonLeafEscaped(const NodeVal &node) {
    NodeVal nodeValRaw = NodeVal::makeEmpty(node.getCodeLoc(), typeTable);
    NodeVal::copyNonValFieldsLeaf(nodeValRaw, node, typeTable);
    NodeVal::unescape(nodeValRaw, typeTable);

    for (const auto &it : node.getEvalVal().elems) {
        NodeVal childProc = processNode(it);
        if (childProc.isInvalid()) return NodeVal();

        /*
        raws can contain ref values, but this can result in segfaults (or worse!) if a ref is passed out of its lifetime
        this will also be a problem with eval pointers, when implemented
        TODO find a way to track lifetimes and make the compilation stop, instead of crashing
        */
        NodeVal::addChild(nodeValRaw, move(childProc), typeTable);
    }

    return nodeValRaw;
}

NodeVal Processor::processType(const NodeVal &node, const NodeVal &starting) {
    if (checkExactlyChildren(node, 1, false))
        return NodeVal::copyNoRef(node.getCodeLoc(), starting);

    NodeVal second = processForTypeArg(node.getChild(1));
    if (second.isInvalid()) return NodeVal();

    EvalVal evalTy = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);

    if (canBeTypeDescrDecor(second)) {
        TypeTable::TypeDescr descr(starting.getEvalVal().ty);
        if (!applyTypeDescrDecor(descr, second)) return NodeVal();
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal decor = processWithEscape(node.getChild(i));
            if (decor.isInvalid()) return NodeVal();
            if (!applyTypeDescrDecor(descr, decor)) return NodeVal();
        }

        evalTy.ty = typeTable->addTypeDescr(move(descr));
    } else {
        TypeTable::Tuple tup;
        tup.members.reserve(node.getChildrenCnt());
        if (!applyTupleMemb(tup, starting)) return NodeVal();
        if (!applyTupleMemb(tup, second)) return NodeVal();
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal ty = processNode(node.getChild(i));
            if (ty.isInvalid()) return NodeVal();
            if (!applyTupleMemb(tup, ty)) return NodeVal();
        }

        optional<TypeTable::Id> tupTypeId = typeTable->addTuple(move(tup));
        if (!tupTypeId.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        evalTy.ty = tupTypeId.value();
    }

    return NodeVal(node.getCodeLoc(), move(evalTy));
}

NodeVal Processor::processId(const NodeVal &node) {
    NamePool::Id id = node.getEvalVal().id;

    NodeVal *value = symbolTable->getVar(id);

    if (value != nullptr) {
        if (value->isUndecidedCallableVal()) return loadUndecidedCallable(*value);

        if (checkIsEvalTime(*value, false)) {
            return evaluator->performLoad(node.getCodeLoc(), id, *value);
        } else {
            return performLoad(node.getCodeLoc(), id, *value);
        }
    } else if (isKeyword(id) || isOper(id)) {
        SpecialVal spec;
        spec.id = id;

        return NodeVal(node.getCodeLoc(), spec);
    } else if (typeTable->isType(id)) {
        EvalVal eval = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
        eval.ty = typeTable->getTypeId(id).value();

        return NodeVal(node.getCodeLoc(), move(eval));
    } else {
        msgs->errorSymNotFound(node.getCodeLoc(), id);
        return NodeVal();
    }
}

NodeVal Processor::processId(const NodeVal &node, const NodeVal &starting) {
    if (!checkExactlyChildren(node, 1, true)) return NodeVal();

    return processId(starting);
}

NodeVal Processor::processSym(const NodeVal &node) {
    if (!checkAtLeastChildren(node, 2, true)) return NodeVal();

    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        const NodeVal &entry = processWithEscape(node.getChild(i));
        if (entry.isInvalid()) return NodeVal();

        if (!NodeVal::isLeaf(entry, typeTable) && !checkBetweenChildren(entry, 1, 2, true)) return NodeVal();

        bool hasInit = checkExactlyChildren(entry, 2, false);

        const NodeVal &nodePair = NodeVal::isLeaf(entry, typeTable) ? entry : entry.getChild(0);
        pair<NodeVal, optional<NodeVal>> pair = processForIdTypePair(nodePair);
        if (pair.first.isInvalid()) return NodeVal();
        NamePool::Id id = pair.first.getEvalVal().id;
        if (!symbolTable->nameAvailable(id, namePool, typeTable)) {
            msgs->errorSymNameTaken(nodePair.getCodeLoc(), id);
            return NodeVal();
        }
        optional<TypeTable::Id> optType;
        if (pair.second.has_value()) optType = pair.second.value().getEvalVal().ty;

        bool hasType = optType.has_value();

        if (hasInit) {
            const NodeVal &nodeInit = entry.getChild(1);
            NodeVal init = hasType ? processAndImplicitCast(nodeInit, optType.value()) : processNode(nodeInit);
            if (init.isInvalid()) return NodeVal();

            NodeVal nodeReg = performRegister(entry.getCodeLoc(), id, init);
            if (nodeReg.isInvalid()) return NodeVal();

            symbolTable->addVar(id, move(nodeReg));
        } else {
            if (!hasType) {
                msgs->errorMissingTypeAttribute(nodePair.getCodeLoc());
                return NodeVal();
            }
            if (typeTable->worksAsTypeCn(optType.value())) {
                msgs->errorUnknown(entry.getCodeLoc());
                return NodeVal();
            }

            NodeVal nodeZero = performZero(entry.getCodeLoc(), optType.value());
            if (nodeZero.isInvalid()) return NodeVal();

            NodeVal nodeReg = performRegister(entry.getCodeLoc(), id, nodeZero);
            if (nodeReg.isInvalid()) return NodeVal();

            symbolTable->addVar(id, move(nodeReg));
        }
    }

    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processCast(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) {
        return NodeVal();
    }

    NodeVal ty = processAndCheckIsType(node.getChild(1));
    if (ty.isInvalid()) return NodeVal();

    NodeVal value = processNode(node.getChild(2));
    if (value.isInvalid()) return NodeVal();

    return dispatchCast(node.getCodeLoc(), value, ty.getEvalVal().ty);
}

// TODO figure out how to detect whether all code branches return or not
NodeVal Processor::processBlock(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 4, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 3;
    bool hasType = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indType = hasName ? 2 : 1;
    size_t indBody = hasName ? 3 : (hasType ? 2 : 1);

    const NodeVal &nodeBody = node.getChild(indBody);
    if (!checkIsRaw(nodeBody, true)) return NodeVal();

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscape(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (!checkIsEmpty(nodeName, false)) {
            if (!checkIsId(nodeName, true)) return NodeVal();
            name = nodeName.getEvalVal().id;
        }
    }

    optional<TypeTable::Id> type;
    if (hasType) {
        NodeVal nodeType = processNode(node.getChild(indType));
        if (nodeType.isInvalid()) return NodeVal();
        if (!checkIsEmpty(nodeType, false)) {
            if (!checkIsType(nodeType, true)) return NodeVal();
            type = nodeType.getEvalVal().ty;
        }
    }

    SymbolTable::Block block;
    block.name = name;
    block.type = type;

    if (!performBlockSetUp(node.getCodeLoc(), block)) return NodeVal();

    do {
        BlockControl blockCtrl(symbolTable, block);

        optional<bool> blockSuccess = performBlockBody(node.getCodeLoc(), *symbolTable->getLastBlock(), nodeBody);
        if (!blockSuccess.has_value()) {
            performBlockTearDown(node.getCodeLoc(), *symbolTable->getLastBlock(), false);
            return NodeVal();
        }

        if (blockSuccess.value()) continue;

        NodeVal ret = performBlockTearDown(node.getCodeLoc(), *symbolTable->getLastBlock(), true);
        if (ret.isInvalid()) return NodeVal();
        return ret;
    } while (true);
}

NodeVal Processor::processExit(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 3, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indCond = hasName ? 2 : 1;

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        name = nodeName.getEvalVal().id;
    }

    NodeVal nodeCond = processNode(node.getChild(indCond));
    if (nodeCond.isInvalid()) return NodeVal();
    if (!checkIsBool(nodeCond, true)) return NodeVal();

    const SymbolTable::Block *targetBlock;
    if (hasName) {
        targetBlock = symbolTable->getBlock(name.value());
        if (targetBlock == nullptr) {
            msgs->errorBlockNotFound(node.getChild(indName).getCodeLoc(), name.value());
            return NodeVal();
        }
    } else {
        targetBlock = symbolTable->getLastBlock();
        if (checkInGlobalScope(node.getCodeLoc(), false) || targetBlock == nullptr) {
            msgs->errorExitNowhere(node.getCodeLoc());
            return NodeVal();
        }
    }
    if (targetBlock->type.has_value()) {
        msgs->errorExitPassingBlock(node.getCodeLoc());
        return NodeVal();
    }

    if (!performExit(node.getCodeLoc(), *targetBlock, nodeCond)) return NodeVal();
    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processLoop(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 3, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indCond = hasName ? 2 : 1;

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        name = nodeName.getEvalVal().id;
    }

    NodeVal nodeCond = processNode(node.getChild(indCond));
    if (nodeCond.isInvalid()) return NodeVal();
    if (!checkIsBool(nodeCond, true)) return NodeVal();

    const SymbolTable::Block *targetBlock;
    if (hasName) {
        targetBlock = symbolTable->getBlock(name.value());
        if (targetBlock == nullptr) {
            msgs->errorBlockNotFound(node.getChild(indName).getCodeLoc(), name.value());
            return NodeVal();
        }
    } else {
        targetBlock = symbolTable->getLastBlock();
        if (checkInGlobalScope(node.getCodeLoc(), false) || targetBlock == nullptr) {
            msgs->errorLoopNowhere(node.getCodeLoc());
            return NodeVal();
        }
    }

    if (!performLoop(node.getCodeLoc(), *targetBlock, nodeCond)) return NodeVal();
    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processPass(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 3, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indVal = hasName ? 2 : 1;

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        name = nodeName.getEvalVal().id;
    }

    SymbolTable::Block *targetBlock;
    if (hasName) {
        targetBlock = symbolTable->getBlock(name.value());
        if (targetBlock == nullptr) {
            msgs->errorBlockNotFound(node.getChild(indName).getCodeLoc(), name.value());
            return NodeVal();
        }
    } else {
        targetBlock = symbolTable->getLastBlock();
        if (checkInGlobalScope(node.getCodeLoc(), false) || targetBlock == nullptr) {
            msgs->errorPassNonPassingBlock(node.getCodeLoc());
            return NodeVal();
        }
    }
    if (!targetBlock->type.has_value()) {
        msgs->errorPassNonPassingBlock(node.getCodeLoc());
        return NodeVal();
    }

    NodeVal nodeValue = processAndImplicitCast(node.getChild(indVal), targetBlock->type.value());
    if (nodeValue.isInvalid()) return NodeVal();

    if (!performPass(node.getCodeLoc(), *targetBlock, nodeValue)) return NodeVal();
    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processCustom(const NodeVal &node) {
    if (!checkInGlobalScope(node.getCodeLoc(), true)) return NodeVal();
    if (!checkExactlyChildren(node, 3, true)) return NodeVal();

    NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(1));
    if (nodeName.isInvalid()) return NodeVal();
    NamePool::Id name = nodeName.getEvalVal().id;
    if (!symbolTable->nameAvailable(name, namePool, typeTable)) {
        msgs->errorUnknown(nodeName.getCodeLoc());
        return NodeVal();
    }

    NodeVal nodeTy = processNode(node.getChild(2));
    if (nodeTy.isInvalid()) return NodeVal();
    if (!checkIsType(nodeTy, true)) return NodeVal();
    TypeTable::Id ty = nodeTy.getEvalVal().ty;

    TypeTable::Custom custom;
    custom.name = name;
    custom.type = ty;
    optional<TypeTable::Id> typeId = typeTable->addCustom(custom);
    if (!typeId.has_value()) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processData(const NodeVal &node) {
    if (!checkInGlobalScope(node.getCodeLoc(), true)) return NodeVal();
    if (!checkBetweenChildren(node, 2, 3, true)) return NodeVal();

    bool definition = node.getChildrenCnt() == 3;

    TypeTable::DataType dataType;
    dataType.defined = false;

    NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(1));
    if (nodeName.isInvalid()) return NodeVal();
    dataType.name = nodeName.getEvalVal().id;
    if (!symbolTable->nameAvailable(dataType.name, namePool, typeTable)) {
        if (optional<TypeTable::Id> oldTy = typeTable->getTypeId(dataType.name);
            !(oldTy.has_value() && typeTable->isDataType(oldTy.value()))) {
            msgs->errorUnknown(nodeName.getCodeLoc());
            return NodeVal();
        }
    }

    optional<TypeTable::Id> typeIdOpt = typeTable->addDataType(dataType);
    if (!typeIdOpt.has_value()) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

    dataType.defined = definition;
    if (dataType.defined) {
        const NodeVal &nodeMembs = processWithEscape(node.getChild(2));
        if (nodeMembs.isInvalid()) return NodeVal();
        if (!checkIsRaw(nodeMembs, true)) return NodeVal();
        if (!checkAtLeastChildren(nodeMembs, 1, true)) return NodeVal();
        for (size_t i = 0; i < nodeMembs.getChildrenCnt(); ++i) {
            const NodeVal &nodeMemb = nodeMembs.getChild(i);

            pair<NodeVal, optional<NodeVal>> memb = processForIdTypePair(nodeMemb);
            if (memb.first.isInvalid()) return NodeVal();
            NamePool::Id membNameId = memb.first.getEvalVal().id;
            if (!memb.second.has_value()) {
                msgs->errorMissingTypeAttribute(nodeMemb.getCodeLoc());
                return NodeVal();
            }
            dataType.members.push_back(make_pair(membNameId, memb.second.value().getEvalVal().ty));
        }

        optional<TypeTable::Id> typeIdOpt = typeTable->addDataType(dataType);
        if (!typeIdOpt.has_value()) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }
    }

    return NodeVal(node.getCodeLoc());
}

// TODO! unify the two cases
NodeVal Processor::processCall(const NodeVal &node, const NodeVal &starting) {
    size_t providedArgCnt = node.getChildrenCnt()-1;

    vector<NodeVal> args;
    args.reserve(providedArgCnt);
    vector<TypeTable::Id> argTypes;
    argTypes.reserve(providedArgCnt);
    for (size_t i = 0; i < providedArgCnt; ++i) {
        NodeVal arg = processNode(node.getChild(i+1));
        if (arg.isInvalid()) return NodeVal();
        if (!checkHasType(arg, true)) return NodeVal();

        args.push_back(move(arg));
        argTypes.push_back(arg.getType().value());
    }

    if (starting.isUndecidedCallableVal()) {
        const FuncValue *funcVal = nullptr;

        vector<const FuncValue*> funcVals = symbolTable->getFuncs(starting.getUndecidedCallableVal().name);

        // first, try to find a func that doesn't require implicit casts
        for (const FuncValue *it : funcVals) {
            if (argsFitFuncCall(args, BaseCallableValue::getCallable(*it, typeTable), false)) {
                funcVal = it;
                break;
            }
        }

        // if not found, look through functions which do require implicit casts
        if (funcVal == nullptr) {
            for (const FuncValue *it : funcVals) {
                if (argsFitFuncCall(args, BaseCallableValue::getCallable(*it, typeTable), true)) {
                    if (funcVal != nullptr) {
                        // error due to call ambiguity
                        msgs->errorUnknown(node.getCodeLoc());
                        return NodeVal();
                    }
                    funcVal = it;
                }
            }
        }

        if (funcVal == nullptr) {
            msgs->errorFuncNotFound(starting.getCodeLoc(), starting.getUndecidedCallableVal().name);
            return NodeVal();
        }

        const TypeTable::Callable &callable = BaseCallableValue::getCallable(*funcVal, typeTable);
        for (size_t i = 0; i < callable.getArgCnt(); ++i) {
            args[i] = implicitCast(args[i], callable.argTypes[i]);
            if (args[i].isInvalid()) return NodeVal();
        }

        if (funcVal->isEval()) {
            return evaluator->performCall(node.getCodeLoc(), *funcVal, args);
        } else {
            return performCall(node.getCodeLoc(), *funcVal, args);
        }
    } else {
        if (!starting.getType().has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }
        const TypeTable::Callable *callable = typeTable->extractCallable(starting.getType().value());
        if (callable == nullptr) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        if (!argsFitFuncCall(args, *callable, true)) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }

        for (size_t i = 0; i < callable->getArgCnt(); ++i) {
            args[i] = implicitCast(args[i], callable->argTypes[i]);
            if (args[i].isInvalid()) return NodeVal();
        }

        if (starting.isEvalVal()) {
            return evaluator->performCall(node.getCodeLoc(), starting, args);
        } else {
            return performCall(node.getCodeLoc(), starting, args);
        }
    }
}

NodeVal Processor::processInvoke(const NodeVal &node, const NodeVal &starting) {
    const MacroValue *macroVal = nullptr;
    if (starting.isUndecidedCallableVal()) {
        SymbolTable::MacroCallSite callSite;
        callSite.name = starting.getUndecidedCallableVal().name;
        callSite.argCnt = node.getChildrenCnt()-1;
    
        macroVal = symbolTable->getMacro(callSite, typeTable);
        if (macroVal == nullptr) {
            msgs->errorMacroNotFound(node.getCodeLoc(), starting.getUndecidedCallableVal().name);
            return NodeVal();
        }
    } else {
        if (starting.getEvalVal().m == nullptr) {
            msgs->errorMacroNoValue(starting.getCodeLoc());
            return NodeVal();
        }
        macroVal = starting.getEvalVal().m;
    }

    const TypeTable::Callable &callable = BaseCallableValue::getCallable(*macroVal, typeTable);

    size_t providedArgCnt = node.getChildrenCnt()-1;
    if ((callable.variadic && providedArgCnt+1 < callable.getArgCnt()) ||
        (!callable.variadic && providedArgCnt != callable.getArgCnt())) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();    
    }

    vector<NodeVal> args;
    for (size_t i = 0; i < providedArgCnt; ++i) {
        NodeVal arg = processWithEscape(node.getChild(i+1), !macroVal->argPreproc[i]);
        if (arg.isInvalid()) return NodeVal();

        args.push_back(move(arg));
    }

    if (callable.variadic) {
        // if it's variadic, it has to have at least 1 argument (the variadic one)
        size_t nonVarArgCnt = callable.getArgCnt()-1;

        // TODO better code loc for this
        NodeVal totalVarArg = NodeVal::makeEmpty(node.getCodeLoc(), typeTable);
        NodeVal::addChildren(totalVarArg, args.begin()+nonVarArgCnt, args.end(), typeTable);

        args.resize(nonVarArgCnt);
        args.push_back(move(totalVarArg));
    }

    return evaluator->performInvoke(node.getCodeLoc(), *macroVal, args);
}

NodeVal Processor::processFnc(const NodeVal &node) {
    if (!checkBetweenChildren(node, 3, 5, true)) return NodeVal();

    bool isType = node.getChildrenCnt() == 3;
    bool isDecl = node.getChildrenCnt() == 4;
    bool isDef = node.getChildrenCnt() == 5;

    if ((isDecl || isDef) && !checkInGlobalScope(node.getCodeLoc(), true)) {
        return NodeVal();
    }

    // fnc args ret OR fnc name args ret OR fnc name args ret body
    size_t indName = isDecl || isDef ? 1 : 0;
    size_t indArgs = isType ? 1 : 2;
    size_t indRet = isType ? 2 : 3;
    size_t indBody = isDef ? 4 : 0;

    // name
    NamePool::Id name;
    bool noNameMangle;
    bool isMain;
    if (isDecl || isDef) {
        NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        name = nodeName.getEvalVal().id;
        if (!symbolTable->nameAvailable(name, namePool, typeTable) &&
            !symbolTable->isFuncName(name)) {
            msgs->errorFuncNameTaken(nodeName.getCodeLoc(), name);
            return NodeVal();
        }

        optional<bool> noNameMangleOpt = hasAttributeAndCheckIsEmpty(nodeName, "noNameMangle");
        if (!noNameMangleOpt.has_value()) return NodeVal();
        isMain = isMeaningful(name, Meaningful::MAIN);
        noNameMangle = noNameMangleOpt.value();
    }

    // arguments
    vector<NamePool::Id> argNames;
    vector<TypeTable::Id> argTypes;
    const NodeVal &nodeArgs = processWithEscape(node.getChild(indArgs));
    if (nodeArgs.isInvalid()) return NodeVal();
    if (!checkIsRaw(nodeArgs, true)) return NodeVal();
    optional<bool> variadic = hasAttributeAndCheckIsEmpty(nodeArgs, "variadic");
    if (!variadic.has_value()) return NodeVal();
    for (size_t i = 0; i < nodeArgs.getChildrenCnt(); ++i) {
        const NodeVal &nodeArg = nodeArgs.getChild(i);

        pair<NodeVal, optional<NodeVal>> arg = processForIdTypePair(nodeArg);
        if (arg.first.isInvalid()) return NodeVal();

        NamePool::Id argId = arg.first.getEvalVal().id;
        if (!arg.second.has_value()) {
            msgs->errorMissingTypeAttribute(nodeArg.getCodeLoc());
            return NodeVal();
        }

        argNames.push_back(argId);
        argTypes.push_back(arg.second.value().getEvalVal().ty);
    }

    // check no arg name duplicates
    if (!checkNoArgNameDuplicates(nodeArgs, argNames, true)) return NodeVal();

    // ret type
    optional<TypeTable::Id> retType;
    {
        const NodeVal &nodeRetType = node.getChild(indRet);
        NodeVal ty = processNode(nodeRetType);
        if (ty.isInvalid()) return NodeVal();
        if (!checkIsEmpty(ty, false)) {
            if (!checkIsType(ty, true)) return NodeVal();
            retType = ty.getEvalVal().ty;
        }
    }

    // body
    const NodeVal *nodeBodyPtr = nullptr;
    if (isDef) {
        nodeBodyPtr = &node.getChild(indBody);
        if (!checkIsRaw(*nodeBodyPtr, true)) {
            return NodeVal();
        }
    }

    // function type
    TypeTable::Id type;
    {
        TypeTable::Callable callable;
        callable.isFunc = true;
        callable.argTypes = argTypes;
        callable.retType = retType;
        callable.variadic = variadic.value();
        optional<TypeTable::Id> typeOpt = typeTable->addCallable(callable);
        if (!typeOpt.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }
        type = typeOpt.value();
    }

    if (isDecl || isDef) {
        FuncValue funcVal;
        BaseCallableValue::setType(funcVal, type, typeTable);
        funcVal.name = name;
        funcVal.argNames = argNames;
        funcVal.noNameMangle = noNameMangle || isMain || variadic.value();
        funcVal.defined = isDef;

        // register only if first func of its name
        if (!symbolTable->isFuncName(name)) {
            UndecidedCallableVal undecidedVal;
            undecidedVal.isFunc = true;
            undecidedVal.name = name;
            symbolTable->addVar(name, NodeVal(node.getCodeLoc(), undecidedVal));
        }

        // funcVal is passed by mutable reference, is needed later
        if (!performFunctionDeclaration(node.getCodeLoc(), funcVal)) return NodeVal();

        FuncValue *symbVal = symbolTable->registerFunc(funcVal);
        if (symbVal == nullptr) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }

        if (isDef) {
            if (!performFunctionDefinition(node.getCodeLoc(), nodeArgs, *nodeBodyPtr, *symbVal)) return NodeVal();
        }

        return NodeVal(node.getCodeLoc());
    } else {
        EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
        evalVal.ty = type;
        return NodeVal(node.getCodeLoc(), move(evalVal));
    }
}

NodeVal Processor::processMac(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, false) && !checkExactlyChildren(node, 4, true)) {
        return NodeVal();
    }

    bool isType = node.getChildrenCnt() == 2;
    bool isDef = node.getChildrenCnt() == 4;

    if (isDef && !checkInGlobalScope(node.getCodeLoc(), true)) {
        return NodeVal();
    }

    // mac args OR mac name args body
    size_t indName = isDef ? 1 : 0;
    size_t indArgs = isType ? 1 : 2;
    size_t indBody = isDef ? 3 : 0;

    // name
    NamePool::Id name;
    if (isDef) {
        NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (!symbolTable->nameAvailable(nodeName.getEvalVal().id, namePool, typeTable) &&
            !symbolTable->isMacroName(nodeName.getEvalVal().id)) {
            msgs->errorMacroNameTaken(nodeName.getCodeLoc(), nodeName.getEvalVal().id);
            return NodeVal();
        }
        name = nodeName.getEvalVal().id;
    }

    // arguments
    const NodeVal &nodeArgs = processWithEscape(node.getChild(indArgs));
    if (nodeArgs.isInvalid()) return NodeVal();
    if (!checkIsRaw(nodeArgs, true)) return NodeVal();
    vector<NamePool::Id> argNames;
    argNames.reserve(nodeArgs.getChildrenCnt());
    vector<bool> argPreproc;
    argPreproc.reserve(nodeArgs.getChildrenCnt());
    bool variadic = false;
    for (size_t i = 0; i < nodeArgs.getChildrenCnt(); ++i) {
        const NodeVal &nodeArg = nodeArgs.getChild(i);

        if (variadic) {
            msgs->errorNotLastParam(nodeArg.getCodeLoc());
            return NodeVal();
        }

        NodeVal arg = processWithEscapeAndCheckIsId(nodeArg);
        if (arg.isInvalid()) return NodeVal();

        NamePool::Id argId = arg.getEvalVal().id;
        argNames.push_back(argId);

        optional<bool> isPreproc = hasAttributeAndCheckIsEmpty(arg, "preprocess");
        if (!isPreproc.has_value()) return NodeVal();
        argPreproc.push_back(isPreproc.value());

        optional<bool> isVariadic = hasAttributeAndCheckIsEmpty(arg, "variadic");
        if (!isVariadic.has_value()) return NodeVal();
        variadic = isVariadic.value();
    }
    // check no arg name duplicates
    if (!checkNoArgNameDuplicates(nodeArgs, argNames, true)) return NodeVal();

    // body
    const NodeVal *nodeBodyPtr = nullptr;
    if (isDef) {
        nodeBodyPtr = &node.getChild(indBody);
        if (!checkIsRaw(*nodeBodyPtr, true)) {
            return NodeVal();
        }
    }

    // macro type
    TypeTable::Id type;
    {
        TypeTable::Callable callable;
        callable.isFunc = false;
        callable.makeFitArgCnt(argNames.size());
        callable.variadic = variadic;
        optional<TypeTable::Id> typeOpt = typeTable->addCallable(callable);
        if (!typeOpt.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }
        type = typeOpt.value();
    }

    if (isDef) {
        MacroValue macroVal;
        BaseCallableValue::setType(macroVal, type, typeTable);
        macroVal.name = name;
        macroVal.argNames = move(argNames);
        macroVal.argPreproc = move(argPreproc);

        // register only if first macro of its name
        if (!symbolTable->isMacroName(name)) {
            UndecidedCallableVal undecidedVal;
            undecidedVal.isFunc = false;
            undecidedVal.name = name;
            symbolTable->addVar(name, NodeVal(node.getCodeLoc(), undecidedVal));
        }

        MacroValue *symbVal = symbolTable->registerMacro(macroVal, typeTable);
        if (symbVal == nullptr) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }

        if (!evaluator->performMacroDefinition(node.getCodeLoc(), nodeArgs, *nodeBodyPtr, *symbVal)) return NodeVal();

        return NodeVal(node.getCodeLoc());
    } else {
        EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
        evalVal.ty = type;
        return NodeVal(node.getCodeLoc(), move(evalVal));
    }
}

NodeVal Processor::processRet(const NodeVal &node) {
    if (!checkBetweenChildren(node, 1, 2, true)) {
        return NodeVal();
    }

    optional<SymbolTable::CalleeValueInfo> optCallee = symbolTable->getCurrCallee();
    if (!optCallee.has_value()) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

    if (optCallee.value().isFunc) {
        bool retsVal = node.getChildrenCnt() == 2;
        if (retsVal) {
            if (!optCallee.value().retType.has_value()) {
                msgs->errorUnknown(node.getCodeLoc());
                return NodeVal();
            }

            NodeVal val = processAndImplicitCast(node.getChild(1), optCallee.value().retType.value());
            if (val.isInvalid()) return NodeVal();

            if (!performRet(node.getCodeLoc(), val)) return NodeVal();
        } else {
            if (optCallee.value().retType.has_value()) {
                msgs->errorRetNoValue(node.getCodeLoc(), optCallee.value().retType.value());
                return NodeVal();
            }

            if (!performRet(node.getCodeLoc())) return NodeVal();
        }

        return NodeVal(node.getCodeLoc());
    } else {
        if (!checkExactlyChildren(node, 2, true)) return NodeVal();

        NodeVal val = processNode(node.getChild(1));
        if (val.isInvalid()) return NodeVal();

        if (!performRet(node.getCodeLoc(), val)) return NodeVal();

        return NodeVal(node.getCodeLoc());
    }
}

NodeVal Processor::processEval(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) {
        return NodeVal();
    }

    return evaluator->processNode(node.getChild(1));
}

NodeVal Processor::processImport(const NodeVal &node) {
    if (!checkIsTopmost(node.getCodeLoc(), true) ||
        !checkExactlyChildren(node, 2, true)) {
        return NodeVal();
    }

    NodeVal file = processNode(node.getChild(1));
    if (file.isInvalid()) return NodeVal();
    if (!file.isEvalVal() || !EvalVal::isStr(file.getEvalVal(), typeTable) ||
        EvalVal::isNull(file.getEvalVal(), typeTable)) {
        msgs->errorImportNotString(file.getCodeLoc());
        return NodeVal();
    }

    return NodeVal(node.getCodeLoc(), file.getEvalVal().str.value());
}

NodeVal Processor::processOper(const NodeVal &node, Oper op) {
    if (!checkAtLeastChildren(node, 2, true)) return NodeVal();

    if (checkExactlyChildren(node, 2, false)) {
        return processOperUnary(node.getCodeLoc(), node.getChild(1), op);
    }

    OperInfo operInfo = operInfos.find(op)->second;

    vector<const NodeVal*> operands;
    operands.reserve(node.getChildrenCnt()-1);
    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        operands.push_back(&node.getChild(i));
    }
    
    if (operInfo.comparison) {
        return processOperComparison(node.getCodeLoc(), operands, op);
    } else if (op == Oper::ASGN) {
        return processOperAssignment(node.getCodeLoc(), operands);
    } else if (op == Oper::IND) {
        return processOperIndex(node.getCodeLoc(), operands);
    } else if (op == Oper::DOT) {
        return processOperMember(node.getCodeLoc(), operands);
    } else {
        return processOperRegular(node.getCodeLoc(), operands, op);
    }
}

// TODO remove tup after replacing with macro
NodeVal Processor::processTup(const NodeVal &node) {
    if (!checkAtLeastChildren(node, 3, true)) return NodeVal();

    vector<NodeVal> membs;
    membs.reserve(node.getChildrenCnt()-1);
    bool allEval = true;
    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        NodeVal memb = processNode(node.getChild(i));
        if (memb.isInvalid()) return NodeVal();
        membs.push_back(move(memb));
        if (allEval && !checkIsEvalTime(memb, false)) allEval = false;
    }

    TypeTable::Tuple tup;
    tup.members.reserve(membs.size());
    for (const NodeVal &memb : membs) {
        optional<TypeTable::Id> membType = memb.getType();
        if (!membType.has_value()) {
            msgs->errorUnknown(memb.getCodeLoc());
            return NodeVal();
        }
        tup.addMember(membType.value());
    }
    optional<TypeTable::Id> tupTypeIdOpt = typeTable->addTuple(move(tup));
    if (!tupTypeIdOpt.has_value()) {
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    if (allEval)
        return evaluator->performTuple(node.getCodeLoc(), tupTypeIdOpt.value(), membs);
    else
        return performTuple(node.getCodeLoc(), tupTypeIdOpt.value(), membs);
}

NodeVal Processor::processTypeOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();
    if (!checkHasType(operand, true)) return NodeVal();

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
    evalVal.ty = operand.getType().value();
    return NodeVal(node.getCodeLoc(), move(evalVal));
}

NodeVal Processor::processLenOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();
    if (!checkHasType(operand, true)) return NodeVal();

    TypeTable::Id ty;
    if (typeTable->worksAsPrimitive(operand.getType().value(), TypeTable::P_TYPE)) ty = operand.getEvalVal().ty;
    else ty = operand.getType().value();

    uint64_t len;
    if (typeTable->worksAsTypeArr(ty)) len = typeTable->extractLenOfArr(ty).value();
    else if (typeTable->worksAsTuple(ty)) len = typeTable->extractLenOfTuple(ty).value();
    else if (typeTable->worksAsPrimitive(ty, TypeTable::P_RAW)) len = operand.getChildrenCnt();
    else {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_U64), typeTable);
    evalVal.u64 = len;
    return NodeVal(node.getCodeLoc(), move(evalVal));
}

NodeVal Processor::processSizeOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();
    if (!checkHasType(operand, true)) return NodeVal();

    TypeTable::Id ty;
    if (typeTable->worksAsPrimitive(operand.getType().value(), TypeTable::P_TYPE)) ty = operand.getEvalVal().ty;
    else ty = operand.getType().value();

    optional<uint64_t> size = compiler->performSizeOf(node.getCodeLoc(), ty);
    if (!size.has_value()) return NodeVal();

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_U64), typeTable);
    evalVal.u64 = size.value();
    return NodeVal(node.getCodeLoc(), move(evalVal));
}

NodeVal Processor::processIsDef(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal name = processWithEscapeAndCheckIsId(node.getChild(1));
    if (name.isInvalid()) return NodeVal();

    NamePool::Id id = name.getEvalVal().id;

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
    evalVal.b = symbolTable->isVarName(id);
    return NodeVal(node.getCodeLoc(), move(evalVal));
}

NodeVal Processor::processAttrOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();

    NodeVal name = processWithEscapeAndCheckIsId(node.getChild(2));
    if (name.isInvalid()) return NodeVal();
    NamePool::Id attrName = name.getEvalVal().id;

    optional<NodeVal> nodeAttr = getAttribute(operand, attrName);
    if (!nodeAttr.has_value()) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

    return nodeAttr.value();
}

NodeVal Processor::processAttrIsDef(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();

    NodeVal name = processWithEscapeAndCheckIsId(node.getChild(2));
    if (name.isInvalid()) return NodeVal();
    NamePool::Id attrName = name.getEvalVal().id;

    NamePool::Id typeId = getMeaningfulNameId(Meaningful::TYPE);

    bool attrIsDef;
    if (attrName == typeId) {
        attrIsDef = operand.hasTypeAttr();
    } else {
        if (!operand.hasNonTypeAttrs()) {
            attrIsDef = false;
        } else {
            const NodeVal &nonTypeAttrs = operand.getNonTypeAttrs();

            attrIsDef = nonTypeAttrs.getAttrMap().find(attrName) != nonTypeAttrs.getAttrMap().end();
        }
    }

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
    evalVal.b = attrIsDef;
    return NodeVal(node.getCodeLoc(), move(evalVal));
}

// not able to fail, only to not find
// update callers if that changes
optional<NodeVal> Processor::getAttribute(const NodeVal &node, NamePool::Id attrName) {
    NamePool::Id typeId = getMeaningfulNameId(Meaningful::TYPE);

    if (attrName == typeId) {
        if (!node.hasTypeAttr()) return nullopt;

        return node.getTypeAttr();
    } else {
        if (!node.hasNonTypeAttrs()) return nullopt;

        const NodeVal &nonTypeAttrs = node.getNonTypeAttrs();

        auto loc = nonTypeAttrs.getAttrMap().find(attrName);
        if (loc == nonTypeAttrs.getAttrMap().end()) return nullopt;

        return *loc->second;
    }
}

optional<NodeVal> Processor::getAttribute(const NodeVal &node, const string &attrStrName) {
    return getAttribute(node, namePool->add(attrStrName));
}

optional<bool> Processor::hasAttributeAndCheckIsEmpty(const NodeVal &node, NamePool::Id attrName) {
    optional<NodeVal> attr = getAttribute(node, attrName);
    if (!attr.has_value()) return false;
    if (!checkIsEmpty(attr.value(), true)) return nullopt;
    return true;
}

optional<bool> Processor::hasAttributeAndCheckIsEmpty(const NodeVal &node, const std::string &attrStrName) {
    return hasAttributeAndCheckIsEmpty(node, namePool->add(attrStrName));
}

NodeVal Processor::promoteLiteralVal(const NodeVal &node) {
    bool isId = false;

    EvalVal eval;
    LiteralVal lit = node.getLiteralVal();
    switch (lit.kind) {
    case LiteralVal::Kind::kId:
        eval.type = typeTable->getPrimTypeId(TypeTable::P_ID);
        eval.id = lit.val_id;
        isId = true;
        break;
    case LiteralVal::Kind::kSint:
        {
            TypeTable::PrimIds fitting = typeTable->shortestFittingPrimTypeI(lit.val_si);
            TypeTable::PrimIds chosen = max(TypeTable::P_I32, fitting);
            eval.type = typeTable->getPrimTypeId(chosen);
            if (chosen == TypeTable::P_I32) eval.i32 = lit.val_si;
            else eval.i64 = lit.val_si;
            break;
        }
    case LiteralVal::Kind::kFloat:
        {
            TypeTable::PrimIds fitting = typeTable->shortestFittingPrimTypeF(lit.val_f);
            TypeTable::PrimIds chosen = max(TypeTable::P_F32, fitting);
            eval.type = typeTable->getPrimTypeId(chosen);
            if (chosen == TypeTable::P_F32) eval.f32 = lit.val_f;
            else eval.f64 = lit.val_f;
            break;
        }
    case LiteralVal::Kind::kChar:
        eval.type = typeTable->getPrimTypeId(TypeTable::P_C8);
        eval.c8 = lit.val_c;
        break;
    case LiteralVal::Kind::kBool:
        eval.type = typeTable->getPrimTypeId(TypeTable::P_BOOL);
        eval.b = lit.val_b;
        break;
    case LiteralVal::Kind::kString:
        eval.type = typeTable->getTypeIdStr();
        eval.str = lit.val_str;
        break;
    case LiteralVal::Kind::kNull:
        eval.type = typeTable->getPrimTypeId(TypeTable::P_PTR);
        break;
    default:
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }
    NodeVal prom(node.getCodeLoc(), eval);

    if (node.isEscaped())
        NodeVal::copyNonValFieldsLeaf(prom, node, typeTable);

    if (node.hasTypeAttr() && !isId) {
        TypeTable::Id ty = node.getTypeAttr().getEvalVal().ty;

        if (!EvalVal::isImplicitCastable(prom.getEvalVal(), ty, stringPool, typeTable)) {
            msgs->errorExprCannotPromote(node.getCodeLoc(), ty);
            return NodeVal();
        }
        prom = evaluator->performCast(prom.getCodeLoc(), prom, ty);
    }

    return prom;
}

bool Processor::canBeTypeDescrDecor(const NodeVal &node) {
    if (!node.isEvalVal()) return false;

    if (EvalVal::isId(node.getEvalVal(), typeTable)) {
        return isTypeDescrDecor(node.getEvalVal().id);
    }

    return EvalVal::isI(node.getEvalVal(), typeTable) || EvalVal::isU(node.getEvalVal(), typeTable);
}

bool Processor::applyTypeDescrDecor(TypeTable::TypeDescr &descr, const NodeVal &node) {
    if (!node.isEvalVal()) {
        msgs->errorInvalidTypeDecorator(node.getCodeLoc());
        return false;
    }

    if (EvalVal::isId(node.getEvalVal(), typeTable)) {
        optional<Meaningful> mean = getMeaningful(node.getEvalVal().id);
        if (!mean.has_value() || !isTypeDescrDecor(mean.value())) {
            msgs->errorInvalidTypeDecorator(node.getCodeLoc());
            return false;
        }

        if (mean == Meaningful::CN) {
            descr.setLastCn();
        } else if (mean == Meaningful::ASTERISK) {
            descr.addDecor({.type=TypeTable::TypeDescr::Decor::D_PTR});
        } else if (mean == Meaningful::SQUARE) {
            descr.addDecor({.type=TypeTable::TypeDescr::Decor::D_ARR_PTR});
        } else {
            msgs->errorInvalidTypeDecorator(node.getCodeLoc());
            return false;
        }
    } else {
        optional<uint64_t> arrSize = EvalVal::getValueNonNeg(node.getEvalVal(), typeTable);
        if (!arrSize.has_value() || arrSize.value() == 0) {
            if (!arrSize.has_value()) {
                optional<int64_t> integ = EvalVal::getValueI(node.getEvalVal(), typeTable);
                if (integ.has_value()) {
                    msgs->errorBadArraySize(node.getCodeLoc(), integ.value());
                } else {
                    msgs->errorInvalidTypeDecorator(node.getCodeLoc());
                }
            } else {
                msgs->errorBadArraySize(node.getCodeLoc(), arrSize.value());
            }
            return false;
        }

        descr.addDecor({.type=TypeTable::TypeDescr::Decor::D_ARR, .len=arrSize.value()});
    }

    return true;
}

bool Processor::applyTupleMemb(TypeTable::Tuple &tup, const NodeVal &node) {
    if (!checkIsEvalVal(node, true) || !checkIsType(node, true)) return false;

    tup.addMember(node.getEvalVal().ty);

    return true;
}

NodeVal Processor::implicitCast(const NodeVal &node, TypeTable::Id ty) {
    if (!checkImplicitCastable(node, ty, true)) return NodeVal();

    if (node.getType().value() == ty)
        return NodeVal::copyNoRef(node.getCodeLoc(), node);

    return dispatchCast(node.getCodeLoc(), node, ty);
}

bool Processor::implicitCastOperands(NodeVal &lhs, NodeVal &rhs, bool oneWayOnly) {
    if (lhs.getType().value() == rhs.getType().value()) return true;

    if (checkImplicitCastable(rhs, lhs.getType().value(), false)) {
        rhs = dispatchCast(rhs.getCodeLoc(), rhs, lhs.getType().value());
        return !rhs.isInvalid();
    } else if (!oneWayOnly && checkImplicitCastable(lhs, rhs.getType().value(), false)) {
        lhs = dispatchCast(lhs.getCodeLoc(), lhs, rhs.getType().value());
        return !lhs.isInvalid();
    } else {
        if (oneWayOnly) msgs->errorExprCannotImplicitCast(rhs.getCodeLoc(), rhs.getType().value(), lhs.getType().value());
        else msgs->errorExprCannotImplicitCastEither(rhs.getCodeLoc(), rhs.getType().value(), lhs.getType().value());
        return false;
    }
}

bool Processor::argsFitFuncCall(const vector<NodeVal> &args, const TypeTable::Callable &callable, bool allowImplicitCasts) {
    if (callable.getArgCnt() != args.size() && !(callable.variadic && callable.getArgCnt() <= args.size()))
        return false;

    for (size_t i = 0; i < callable.getArgCnt(); ++i) {
        if (!checkHasType(args[i], false)) return false;

        bool sameType = args[i].getType().value() == callable.argTypes[i];

        if (!sameType && !(allowImplicitCasts && checkImplicitCastable(args[i], callable.argTypes[i], false)))
            return false;
    }

    return true;
}

NodeVal Processor::loadUndecidedCallable(const NodeVal &node) {
    if (node.getUndecidedCallableVal().isFunc) {
        vector<const FuncValue*> funcVals = symbolTable->getFuncs(node.getUndecidedCallableVal().name);
        if (funcVals.empty()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        const FuncValue *funcVal = nullptr;

        if (node.hasTypeAttr()) {
            TypeTable::Id ty = node.getTypeAttr().getEvalVal().ty;
            for (const FuncValue *it : funcVals) {
                if (it->type == ty) {
                    funcVal = it;
                    break;
                }
            }
            if (funcVal == nullptr) {
                msgs->errorFuncNotFound(node.getCodeLoc(), node.getUndecidedCallableVal().name);
                return NodeVal();
            }
        } else if (funcVals.size() == 1) {
            funcVal = funcVals.front();
        } else {
            // still undecided
            return node;
        }

        if (checkIsEvalFunc(node.getCodeLoc(), *funcVal, false))
            return evaluator->performLoad(node.getCodeLoc(), *funcVal);
        else
            return performLoad(node.getCodeLoc(), *funcVal);
    } else {
        vector<const MacroValue*> macroVals = symbolTable->getMacros(node.getUndecidedCallableVal().name);
        if (macroVals.empty()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        const MacroValue *macroVal = nullptr;

        if (node.hasTypeAttr()) {
            TypeTable::Id ty = node.getTypeAttr().getEvalVal().ty;
            for (const MacroValue *it : macroVals) {
                if (it->type == ty) {
                    macroVal = it;
                    break;
                }
            }
            if (macroVal == nullptr) {
                msgs->errorMacroNotFound(node.getCodeLoc(), node.getUndecidedCallableVal().name);
                return NodeVal();
            }
        } else if (macroVals.size() == 1) {
            macroVal = macroVals.front();
        } else {
            // still undecided
            return node;
        }

        return evaluator->performLoad(node.getCodeLoc(), *macroVal);
    }
}

bool Processor::processAttributes(NodeVal &node) {
    NamePool::Id typeId = getMeaningfulNameId(Meaningful::TYPE);

    if (node.hasTypeAttr()) {
        NodeVal procType = processNode(node.getTypeAttr());
        if (procType.isInvalid()) return false;
        if (!checkIsType(procType, true)) return false;

        node.setTypeAttr(move(procType));
    }

    if (node.hasNonTypeAttrs() && !node.getNonTypeAttrs().isAttrMap()) {
        NodeVal::AttrMap attrMap;

        NodeVal nodeAttrs = processWithEscape(node.getNonTypeAttrs());
        if (nodeAttrs.isInvalid()) return false;

        if (!NodeVal::isRawVal(nodeAttrs, typeTable)) {
            if (!checkIsId(nodeAttrs, true)) return false;

            NamePool::Id attrName = nodeAttrs.getEvalVal().id;
            if (attrName == typeId || attrMap.find(attrName) != attrMap.end()) {
                msgs->errorUnknown(nodeAttrs.getCodeLoc());
                return false;
            }

            NodeVal attrVal = NodeVal::makeEmpty(nodeAttrs.getCodeLoc(), typeTable);

            attrMap.insert({attrName, make_unique<NodeVal>(move(attrVal))});
        } else {
            for (size_t i = 0; i < nodeAttrs.getChildrenCnt(); ++i) {
                const NodeVal &nodeAttrEntry = nodeAttrs.getChild(i);

                const NodeVal *nodeAttrEntryName = nullptr;
                const NodeVal *nodeAttrEntryVal = nullptr;
                if (!NodeVal::isRawVal(nodeAttrEntry, typeTable)) {
                    nodeAttrEntryName = &nodeAttrEntry;
                } else {
                    if (!checkExactlyChildren(nodeAttrEntry, 2, true)) return false;
                    nodeAttrEntryName = &nodeAttrEntry.getChild(0);
                    nodeAttrEntryVal = &nodeAttrEntry.getChild(1);
                }

                if (!checkIsId(*nodeAttrEntryName, true)) return false;

                NamePool::Id attrName = nodeAttrEntryName->getEvalVal().id;
                if (attrName == typeId || attrMap.find(attrName) != attrMap.end()) {
                    msgs->errorUnknown(nodeAttrEntry.getCodeLoc());
                    return false;
                }

                NodeVal attrVal;
                if (nodeAttrEntryVal == nullptr) {
                    attrVal = NodeVal::makeEmpty(nodeAttrEntryName->getCodeLoc(), typeTable);
                } else {
                    attrVal = processNode(*nodeAttrEntryVal);
                    if (attrVal.isInvalid()) return false;
                }

                attrMap.insert({attrName, make_unique<NodeVal>(move(attrVal))});
            }
        }

        node.setNonTypeAttrs(NodeVal(node.getNonTypeAttrs().getCodeLoc(), move(attrMap)));
    }

    return true;
}

bool Processor::processChildNodes(const NodeVal &node) {
    for (size_t i = 0; i < node.getChildrenCnt(); ++i) {
        NodeVal tmp = processNode(node.getChild(i));
        if (tmp.isInvalid()) return false;
    }

    return true;
}

NodeVal Processor::processOperUnary(CodeLoc codeLoc, const NodeVal &oper, Oper op) {
    OperInfo operInfo = operInfos.find(op)->second;
    if (!operInfo.unary) {
        msgs->errorNonUnOp(codeLoc, op);
        return NodeVal();
    }

    NodeVal operProc = processNode(oper);
    if (operProc.isInvalid()) return NodeVal();

    if (op == Oper::MUL) {
        return dispatchOperUnaryDeref(codeLoc, operProc);
    } else {
        if (checkIsEvalTime(operProc, false)) {
            return evaluator->performOperUnary(codeLoc, operProc, op);
        } else {
            return performOperUnary(codeLoc, operProc, op);
        }
    }
}

NodeVal Processor::processOperComparison(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op) {
    NodeVal lhs = processAndCheckHasType(*opers[0]);
    if (lhs.isInvalid()) return NodeVal();

    // redirecting to evaluator when all operands are EvalVals is more complicated in the case of comparisons
    // the reason for is that LLVM's phi nodes need to be started up and closed appropriately
    void *evalSignal = nullptr;
    DeferredCallback evalSignalDeleteGuard([=] { delete (int*) evalSignal; });

    void *signal = nullptr;
    DeferredCallback signalDeleteGuard([=] { delete (int*) signal; });

    // do set up on the appropriate processor subclass, depending on whether first operand is EvalVal
    bool stillEval = checkIsEvalTime(lhs, false);
    if (stillEval) {
        evalSignal = evaluator->performOperComparisonSetUp(codeLoc, opers.size());
        if (evalSignal == nullptr) return NodeVal();
    } else {
        signal = performOperComparisonSetUp(codeLoc, opers.size());
        if (signal == nullptr) return NodeVal();
    }

    for (size_t i = 1; i < opers.size(); ++i) {
        // note that if this is within evaluator, and it throws ExceptionEvaluatorJump, teardown will not get called
        NodeVal rhs = processAndCheckHasType(*opers[i]);
        if (rhs.isInvalid()) {
            if (stillEval) return evaluator->performOperComparisonTearDown(codeLoc, false, evalSignal);
            else return performOperComparisonTearDown(codeLoc, false, signal);
        }

        if (stillEval && !checkIsEvalTime(rhs, false)) {
            // no longer all EvalVals, so handoff from evaluator-> to this->
            evaluator->performOperComparisonTearDown(codeLoc, true, evalSignal);
            stillEval = false;

            signal = performOperComparisonSetUp(codeLoc, opers.size()-i);
            if (signal == nullptr) return NodeVal();
        }

        if (!implicitCastOperands(lhs, rhs, false)) {
            if (stillEval) return evaluator->performOperComparisonTearDown(codeLoc, false, evalSignal);
            else return performOperComparisonTearDown(codeLoc, false, signal);
        }

        if (stillEval) {
            optional<bool> compSuccess = evaluator->performOperComparison(codeLoc, lhs, rhs, op, evalSignal);
            if (!compSuccess.has_value()) return evaluator->performOperComparisonTearDown(codeLoc, false, evalSignal);
            if (compSuccess.value()) break;
        } else {
            optional<bool> compSuccess = performOperComparison(codeLoc, lhs, rhs, op, signal);
            if (!compSuccess.has_value()) return performOperComparisonTearDown(codeLoc, false, signal);
            if (compSuccess.value()) break;
        }

        lhs = move(rhs);
    }

    if (stillEval) {
        return evaluator->performOperComparisonTearDown(codeLoc, true, evalSignal);
    } else {
        return performOperComparisonTearDown(codeLoc, true, signal);
    }
}

NodeVal Processor::processOperAssignment(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal rhs = processAndCheckHasType(*opers.back());
    if (rhs.isInvalid()) return NodeVal();

    for (size_t i = opers.size()-2;; --i) {
        NodeVal lhs = processAndCheckHasType(*opers[i]);
        if (lhs.isInvalid()) return NodeVal();

        if (!lhs.hasRef()) {
            msgs->errorExprAsgnNonRef(lhs.getCodeLoc());
            return NodeVal();
        }
        if (typeTable->worksAsTypeCn(lhs.getType().value())) {
            msgs->errorExprAsgnOnCn(lhs.getCodeLoc());
            return NodeVal();
        }

        if (!implicitCastOperands(lhs, rhs, true)) return NodeVal();

        if (checkIsEvalTime(lhs, false) && checkIsEvalTime(rhs, false)) {
            rhs = evaluator->performOperAssignment(codeLoc, lhs, rhs);
        } else {
            rhs = performOperAssignment(codeLoc, lhs, rhs);
        }
        if (rhs.isInvalid()) return NodeVal();

        if (i == 0) break;
    }

    return rhs;
}

NodeVal Processor::processOperIndex(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal base = processAndCheckHasType(*opers[0]);
    if (base.isInvalid()) return NodeVal();

    for (size_t i = 1; i < opers.size(); ++i) {
        TypeTable::Id baseType = base.getType().value();

        optional<TypeTable::Id> elemType = typeTable->addTypeIndexOf(baseType);
        if (!elemType.has_value()) {
            msgs->errorExprIndexOnBadType(base.getCodeLoc(), baseType);
            return NodeVal();
        }

        NodeVal index = processAndCheckHasType(*opers[i]);
        if (index.isInvalid()) return NodeVal();
        if (!typeTable->worksAsTypeI(index.getType().value()) && !typeTable->worksAsTypeU(index.getType().value())) {
            msgs->errorExprIndexNotIntegral(index.getCodeLoc());
            return NodeVal();
        }

        if (index.isEvalVal() && typeTable->worksAsTypeArr(baseType)) {
            size_t len = typeTable->extractLenOfArr(baseType).value();
            optional<size_t> ind = EvalVal::getValueNonNeg(index.getEvalVal(), typeTable);
            if (!ind.has_value() || ind.value() >= len) {
                msgs->errorExprIndexOutOfBounds(index.getCodeLoc());
                return NodeVal();
            }
        }

        TypeTable::Id resType = elemType.value();
        if (typeTable->worksAsTypeCn(baseType)) resType = typeTable->addTypeCnOf(resType);

        if (checkIsEvalTime(base, false) && checkIsEvalTime(index, false)) {
            base = evaluator->performOperIndex(codeLoc, base, index, resType);
        } else {
            base = performOperIndex(base.getCodeLoc(), base, index, resType);
        }
        if (base.isInvalid()) return NodeVal();
    }

    return base;
}

NodeVal Processor::processOperMember(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal base = processAndCheckHasType(*opers[0]);
    if (base.isInvalid()) return NodeVal();

    for (size_t i = 1; i < opers.size(); ++i) {
        TypeTable::Id baseType = base.getType().value();
        bool isBaseRaw = NodeVal::isRawVal(base, typeTable);
        bool isBaseTup = typeTable->worksAsTuple(baseType);
        bool isBaseData = typeTable->worksAsDataType(baseType);

        NodeVal index = processWithEscape(*opers[i]);
        if (index.isInvalid()) return NodeVal();
        if (!index.isEvalVal()) {
            msgs->errorMemberIndex(index.getCodeLoc());
            return NodeVal();
        }

        size_t baseLen = 0;
        if (isBaseRaw) {
            baseLen = base.getChildrenCnt();
        } else if (isBaseTup) {
            optional<const TypeTable::Tuple*> tupleOpt = typeTable->extractTuple(baseType);
            if (!tupleOpt.has_value()) {
                msgs->errorExprDotInvalidBase(base.getCodeLoc());
                return NodeVal();
            }
            baseLen = tupleOpt.value()->members.size();
        } else if (isBaseData) {
            // base len not needed
        } else {
            msgs->errorUnknown(base.getCodeLoc());
            return NodeVal();
        }

        uint64_t indexVal;
        if (isBaseData) {
            if (!EvalVal::isId(index.getEvalVal(), typeTable)) {
                msgs->errorUnknown(index.getCodeLoc());
                return NodeVal();
            }
            NamePool::Id indexName = index.getEvalVal().id;
            optional<uint64_t> indexValOpt = typeTable->extractDataType(baseType).value()->getMembInd(indexName);
            if (!indexValOpt.has_value()) {
                msgs->errorUnknown(index.getCodeLoc());
                return NodeVal();
            }
            indexVal = indexValOpt.value();
        } else {
            optional<uint64_t> indexValOpt = EvalVal::getValueNonNeg(index.getEvalVal(), typeTable);
            if (!indexValOpt.has_value() || indexValOpt.value() >= baseLen) {
                msgs->errorMemberIndex(index.getCodeLoc());
                return NodeVal();
            }
            indexVal = indexValOpt.value();
        }

        optional<TypeTable::Id> resType;
        if (isBaseRaw) {
            resType = base.getChild(indexVal).getType();
            if (typeTable->worksAsPrimitive(resType.value(), TypeTable::P_RAW) && typeTable->worksAsTypeCn(baseType)) {
                resType = typeTable->addTypeCnOf(resType.value());
            }
        } else if (isBaseTup) {
            resType = typeTable->extractTupleMemberType(baseType, (size_t) indexVal);
        } else {
            resType = typeTable->extractDataTypeMemberType(baseType, index.getEvalVal().id);
        }
        if (!resType.has_value()) {
            msgs->errorInternal(base.getCodeLoc());
            return NodeVal();
        }

        if (checkIsEvalTime(base, false)) {
            base = evaluator->performOperMember(index.getCodeLoc(), base, indexVal, resType.value());
        } else {
            base = performOperMember(index.getCodeLoc(), base, indexVal, resType.value());
        }
        if (base.isInvalid()) return NodeVal();
    }

    return base;
}

NodeVal Processor::processOperRegular(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op) {
    OperInfo operInfo = operInfos.find(op)->second;
    if (!operInfo.binary) {
        msgs->errorNonBinOp(codeLoc, op);
        return NodeVal();
    }

    NodeVal lhs = processAndCheckHasType(*opers[0]);
    if (lhs.isInvalid()) return NodeVal();

    for (size_t i = 1; i < opers.size(); ++i) {
        NodeVal rhs = processAndCheckHasType(*opers[i]);
        if (rhs.isInvalid()) return NodeVal();

        if (!implicitCastOperands(lhs, rhs, false)) return NodeVal();

        if (checkIsEvalTime(lhs, false) && checkIsEvalTime(rhs, false)) {
            lhs = evaluator->performOperRegular(codeLoc, lhs, rhs, op);
        } else {
            lhs = performOperRegular(codeLoc, lhs, rhs, op);
        }
        if (lhs.isInvalid()) return NodeVal();
    }

    return lhs;
}

NodeVal Processor::processAndCheckIsType(const NodeVal &node) {
    NodeVal ty = processNode(node);
    if (ty.isInvalid()) return NodeVal();
    if (!checkIsType(ty, true)) return NodeVal();
    return ty;
}

NodeVal Processor::processAndCheckHasType(const NodeVal &node) {
    NodeVal proc = processNode(node);
    if (proc.isInvalid()) return NodeVal();
    if (!checkHasType(proc, true)) return NodeVal();
    return proc;
}

NodeVal Processor::processWithEscape(const NodeVal &node, bool escape) {
    if (escape) {
        NodeVal esc = node;
        NodeVal::escape(esc, typeTable);
        return processNode(esc);
    } else {
        return processNode(node);
    }
}

NodeVal Processor::processWithEscapeAndCheckIsId(const NodeVal &node) {
    NodeVal id = processWithEscape(node);
    if (id.isInvalid()) return NodeVal();
    if (!checkIsId(id, true)) return NodeVal();
    return id;
}

NodeVal Processor::processForTypeArg(const NodeVal &node) {
    NodeVal esc = processWithEscape(node);
    if (esc.isInvalid()) return NodeVal();
    if (!esc.isEvalVal()) {
        msgs->errorEvaluationNotSupported(esc.getCodeLoc());
        return NodeVal();
    }

    if (canBeTypeDescrDecor(esc)) {
        return esc;
    }

    return processNode(esc);
}

pair<NodeVal, optional<NodeVal>> Processor::processForIdTypePair(const NodeVal &node) {
    const pair<NodeVal, optional<NodeVal>> retInvalid = make_pair<NodeVal, optional<NodeVal>>(NodeVal(), nullopt);

    NodeVal esc = processWithEscape(node);
    if (esc.isInvalid()) return retInvalid;

    if (!checkIsId(esc, true)) return retInvalid;
    NodeVal id = esc;
    id.clearTypeAttr();

    optional<NodeVal> ty;
    if (esc.hasTypeAttr()) ty = esc.getTypeAttr();

    return make_pair<NodeVal, optional<NodeVal>>(move(id), move(ty));
}

NodeVal Processor::processAndImplicitCast(const NodeVal &node, TypeTable::Id ty) {
    NodeVal proc = processNode(node);
    if (proc.isInvalid()) return NodeVal();

    return implicitCast(proc, ty);
}

NodeVal Processor::dispatchCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    if (checkIsEvalTime(node, false) && EvalVal::isCastable(node.getEvalVal(), ty, stringPool, typeTable))
        return evaluator->performCast(node.getCodeLoc(), node, ty);
    else
        return performCast(node.getCodeLoc(), node, ty);
}

NodeVal Processor::dispatchOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) {
    if (!checkHasType(oper, true)) return NodeVal();

    optional<TypeTable::Id> resTy = typeTable->addTypeDerefOf(oper.getType().value());
    if (!resTy.has_value()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    if (checkIsEvalTime(oper, false)) {
        return evaluator->performOperUnaryDeref(codeLoc, oper, resTy.value());
    } else {
        return performOperUnaryDeref(codeLoc, oper, resTy.value());
    }
}

bool Processor::checkInGlobalScope(CodeLoc codeLoc, bool orError) {
    if (!symbolTable->inGlobalScope()) {
        if (orError) msgs->errorNotGlobalScope(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkInLocalScope(CodeLoc codeLoc, bool orError) {
    if (symbolTable->inGlobalScope()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkHasType(const NodeVal &node, bool orError) {
    if (!node.getType().has_value()) {
        if (orError) msgs->errorUnknown(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsEvalTime(CodeLoc codeLoc, const NodeVal &node, bool orError) {
    return checkIsEvalVal(codeLoc, node, true);
}

bool Processor::checkIsEvalFunc(CodeLoc codeLoc, const FuncValue &func, bool orError) {
    if (!func.isEval()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsRaw(const NodeVal &node, bool orError) {
    if (!NodeVal::isRawVal(node, typeTable)) {
        if (orError) msgs->errorUnexpectedIsTerminal(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsEmpty(const NodeVal &node, bool orError) {
    if (!NodeVal::isEmpty(node, typeTable)) {
        if (orError) msgs->errorUnknown(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsEvalVal(CodeLoc codeLoc, const NodeVal &node, bool orError) {
    if (!node.isEvalVal()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsLlvmVal(CodeLoc codeLoc, const NodeVal &node, bool orError) {
    if (!node.isLlvmVal()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsLlvmFunc(CodeLoc codeLoc, const FuncValue &func, bool orError) {
    if (func.isEval()) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsTopmost(CodeLoc codeLoc, bool orError) {
    if (topmost != 1) {
        if (orError) msgs->errorNotTopmost(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsId(const NodeVal &node, bool orError) {
    if (!node.isEvalVal() || !EvalVal::isId(node.getEvalVal(), typeTable)) {
        if (orError) msgs->errorUnexpectedNotId(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsType(const NodeVal &node, bool orError) {
    if (!node.isEvalVal() || !EvalVal::isType(node.getEvalVal(), typeTable)) {
        if (orError) msgs->errorUnexpectedNotType(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsBool(const NodeVal &node, bool orError) {
    if (!node.getType().has_value() || !typeTable->worksAsTypeB(node.getType().value())) {
        if (orError) msgs->errorUnknown(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsValue(const NodeVal &node, bool orError) {
    if (!node.isEvalVal() && !node.isLlvmVal()) {
        if (orError) msgs->errorUnknown(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkExactlyChildren(const NodeVal &node, std::size_t n, bool orError) {
    if (!NodeVal::isRawVal(node, typeTable) || node.getChildrenCnt() != n) {
        if (orError) msgs->errorChildrenNotEq(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkAtLeastChildren(const NodeVal &node, std::size_t n, bool orError) {
    if (!NodeVal::isRawVal(node, typeTable) || node.getChildrenCnt() < n) {
        if (orError) msgs->errorChildrenLessThan(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkAtMostChildren(const NodeVal &node, std::size_t n, bool orError) {
    if (!NodeVal::isRawVal(node, typeTable) || node.getChildrenCnt() > n) {
        if (orError) msgs->errorChildrenMoreThan(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkBetweenChildren(const NodeVal &node, std::size_t nLo, std::size_t nHi, bool orError) {
    if (!NodeVal::isRawVal(node, typeTable) || !between(node.getChildrenCnt(), nLo, nHi)) {
        if (orError) msgs->errorChildrenNotBetween(node.getCodeLoc(), nLo, nHi);
        return false;
    }
    return true;
}

bool Processor::checkImplicitCastable(const NodeVal &node, TypeTable::Id ty, bool orError) {
    if (!checkHasType(node, true)) return false;
    TypeTable::Id nodeTy = node.getType().value();
    if (node.isEvalVal()) {
        if (!EvalVal::isImplicitCastable(node.getEvalVal(), ty, stringPool, typeTable)) {
            if (orError) msgs->errorExprCannotImplicitCast(node.getCodeLoc(), nodeTy, ty);
            return false;
        }
    } else {
        if (!typeTable->isImplicitCastable(nodeTy, ty)) {
            if (orError) msgs->errorExprCannotImplicitCast(node.getCodeLoc(), nodeTy, ty);
            return false;
        }
    }
    return true;
}

bool Processor::checkNoArgNameDuplicates(const NodeVal &nodeArgs, const std::vector<NamePool::Id> &argNames, bool orError) {
    for (size_t i = 0; i+1 < nodeArgs.getChildrenCnt(); ++i) {
        for (size_t j = i+1; j < nodeArgs.getChildrenCnt(); ++j) {
            if (argNames[i] == argNames[j]) {
                if (orError) msgs->errorArgNameDuplicate(nodeArgs.getChild(j).getCodeLoc(), argNames[j]);
                return false;
            }
        }
    }
    return true;
}