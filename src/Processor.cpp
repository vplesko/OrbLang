#include "Processor.h"
#include "Reserved.h"
#include "Evaluator.h"
#include "BlockControl.h"
using namespace std;

Processor::Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs)
    : namePool(namePool), stringPool(stringPool), typeTable(typeTable), symbolTable(symbolTable), msgs(msgs), compiler(nullptr), evaluator(nullptr) {
}

NodeVal Processor::processNode(const NodeVal &node, bool topmost) {
    NodeVal ret;
    if (node.hasTypeAttr() || node.hasNonTypeAttrs()) {
        NodeVal procAttrs = node;
        if (!processAttributes(procAttrs)) return NodeVal();

        if (NodeVal::isLeaf(procAttrs, typeTable)) ret = processLeaf(procAttrs);
        else ret = processNonLeaf(procAttrs, topmost);
        if (ret.isInvalid()) return NodeVal();

        if (procAttrs.hasTypeAttr()) ret.setTypeAttr(move(procAttrs.getTypeAttr()));
        if (procAttrs.hasNonTypeAttrs()) ret.setNonTypeAttrs(move(procAttrs.getNonTypeAttrs()));
    } else {
        if (NodeVal::isLeaf(node, typeTable)) ret = processLeaf(node);
        else ret = processNonLeaf(node, topmost);
    }
    if (ret.isInvalid()) return NodeVal();

    if (topmost) {
        if (!callDropFuncNonRef(ret)) return NodeVal();
    }

    return ret;
}

NodeVal Processor::processLeaf(const NodeVal &node) {
    NodeVal prom;
    if (node.isLiteralVal()) prom = promoteLiteralVal(node);
    else prom = node;

    if (!prom.isEscaped() && prom.isEvalVal() && EvalVal::isId(prom.getEvalVal(), typeTable)) {
        return processId(prom);
    }

    // unescaping since the node has been processed (even if it may have otherwise been a no-op)
    NodeVal::unescape(prom, typeTable);
    return prom;
}

NodeVal Processor::processNonLeaf(const NodeVal &node, bool topmost) {
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
                return processBlock(node, starting);
            case Keyword::EXIT:
                return processExit(node);
            case Keyword::LOOP:
                return processLoop(node);
            case Keyword::PASS:
                return processPass(node);
            case Keyword::FIXED:
                return processFixed(node, starting);
            case Keyword::DATA:
                return processData(node, starting);
            case Keyword::FNC:
                return processFnc(node, starting);
            case Keyword::RET:
                return processRet(node);
            case Keyword::MAC:
                return processMac(node, starting);
            case Keyword::EVAL:
                return processEval(node);
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
                return processImport(node, topmost);
            case Keyword::MESSAGE:
                return processMessage(node, starting);
            default:
                msgs->errorUnexpectedKeyword(starting.getCodeLoc(), keyw.value());
                return NodeVal();
            }
        }

        optional<Oper> op = getOper(starting.getSpecialVal().id);
        if (op.has_value()) {
            return processOper(node, starting, op.value());
        }

        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    msgs->errorUnexpectedNonLeafStart(starting.getCodeLoc());
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
            NodeVal decor = processForTypeArg(node.getChild(i));
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

    optional<VarId> varIdOpt = symbolTable->getVarId(id);

    if (varIdOpt.has_value()) {
        SymbolTable::VarEntry &varEntry = symbolTable->getVar(varIdOpt.value());

        if (varEntry.var.isUndecidedCallableVal()) return loadUndecidedCallable(node, varEntry.var);

        return dispatchLoad(node.getCodeLoc(), varIdOpt.value(), id);
    } else if (isKeyword(id) || isOper(id)) {
        SpecialVal spec;
        spec.id = id;

        return NodeVal(node.getCodeLoc(), spec);
    } else if (typeTable->isType(id)) {
        EvalVal eval = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
        eval.ty = typeTable->getTypeId(id).value();

        return NodeVal(node.getCodeLoc(), move(eval));
    } else {
        msgs->errorSymbolNotFound(node.getCodeLoc(), id);
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
        NodeVal entry = processWithEscape(node.getChild(i));
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
        if (pair.second.has_value()) {
            optType = pair.second.value().getEvalVal().ty;
            if (typeTable->isUndef(optType.value())) {
                msgs->errorSymUndef(pair.second.value().getCodeLoc(), optType.value());
                return NodeVal();
            }
        }

        bool hasType = optType.has_value();

        TypeTable::Id varType;
        SymbolTable::VarEntry varEntry;
        varEntry.name = id;

        if (hasInit) {
            // TODO if noDrop gets allowed, ensure lifetimes on ownership transfers are ok
            const NodeVal &nodeInit = entry.getChild(1);
            NodeVal init = hasType ? processAndImplicitCast(nodeInit, optType.value()) : processNode(nodeInit);
            if (init.isInvalid()) return NodeVal();
            if (!checkHasType(init, true)) return NodeVal();

            if (!checkTransferValueOk(pair.first.getCodeLoc(), init, false, true)) return NodeVal();

            varType = init.getType().value();

            NodeVal reg = performRegister(pair.first.getCodeLoc(), id, init);
            if (reg.isInvalid()) return NodeVal();

            varEntry.var = move(reg);
        } else {
            if (!hasType) {
                msgs->errorMissingTypeAttribute(nodePair.getCodeLoc());
                return NodeVal();
            }
            if (typeTable->worksAsTypeCn(optType.value())) {
                msgs->errorUnknown(entry.getCodeLoc());
                return NodeVal();
            }

            varType = optType.value();

            optional<bool> attrNoZero = getAttributeForBool(pair.first, "noZero");
            if (!attrNoZero.has_value()) return NodeVal();

            NodeVal nodeReg;
            if (attrNoZero.value()) {
                nodeReg = performRegister(pair.first.getCodeLoc(), id, optType.value());
                if (nodeReg.isInvalid()) return NodeVal();
            } else {
                NodeVal nodeZero = performZero(entry.getCodeLoc(), optType.value());
                if (nodeZero.isInvalid()) return NodeVal();

                nodeReg = performRegister(pair.first.getCodeLoc(), id, nodeZero);
                if (nodeReg.isInvalid()) return NodeVal();
            }

            varEntry.var = move(nodeReg);
        }

        if (checkInGlobalScope(pair.first.getCodeLoc(), false) && !hasTrivialDrop(varType)) {
            msgs->errorUnknown(pair.first.getCodeLoc());
            return NodeVal();
        }

        symbolTable->addVar(move(varEntry));
    }

    return NodeVal(node.getCodeLoc());
}

// TODO allow arr to arr (by elem)?
NodeVal Processor::processCast(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) {
        return NodeVal();
    }

    NodeVal ty = processAndCheckIsType(node.getChild(1));
    if (ty.isInvalid()) return NodeVal();
    if (typeTable->isUndef(ty.getEvalVal().ty)) {
        msgs->errorUnknown(ty.getCodeLoc());
        return NodeVal();
    }

    NodeVal value = processNode(node.getChild(2));
    if (value.isInvalid()) return NodeVal();

    NodeVal ret = castNode(node.getCodeLoc(), value, ty.getEvalVal().ty);
    if (ret.isInvalid()) return NodeVal();

    return ret;
}

// TODO figure out how to detect whether all code branches return or not
NodeVal Processor::processBlock(const NodeVal &node, const NodeVal &starting) {
    if (!checkBetweenChildren(node, 2, 4, true)) return NodeVal();

    optional<bool> attrBare = getAttributeForBool(starting, "bare");
    if (!attrBare.has_value()) return NodeVal();

    bool hasName = node.getChildrenCnt() > 3;
    bool hasType = node.getChildrenCnt() > 2;

    if (attrBare.value() && (hasName || hasType)) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

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
            if (typeTable->isUndef(type.value())) {
                msgs->errorUnknown(nodeType.getCodeLoc());
                return NodeVal();
            }
        }
    }

    if (attrBare.value()) {
        if (!processChildNodes(nodeBody)) return NodeVal();

        return NodeVal(node.getCodeLoc());
    } else {
        SymbolTable::Block block;
        block.name = name;
        block.type = type;
        if (!performBlockSetUp(node.getCodeLoc(), block)) return NodeVal();

        do {
            BlockControl blockCtrl(symbolTable, block);

            optional<bool> blockSuccess = performBlockBody(node.getCodeLoc(), symbolTable->getLastBlock(), nodeBody);
            if (!blockSuccess.has_value()) {
                performBlockTearDown(node.getCodeLoc(), symbolTable->getLastBlock(), false);
                return NodeVal();
            }

            if (blockSuccess.value()) continue;

            NodeVal ret = performBlockTearDown(node.getCodeLoc(), symbolTable->getLastBlock(), true);
            if (ret.isInvalid()) return NodeVal();
            return ret;
        } while (true);
    }
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

    SymbolTable::Block targetBlock;
    if (hasName) {
        optional<SymbolTable::Block> targetBlockOpt = symbolTable->getBlock(name.value());
        if (!targetBlockOpt.has_value()) {
            msgs->errorBlockNotFound(node.getChild(indName).getCodeLoc(), name.value());
            return NodeVal();
        }
        targetBlock = targetBlockOpt.value();
    } else {
        targetBlock = symbolTable->getLastBlock();
        if (checkInGlobalScope(node.getCodeLoc(), false)) {
            msgs->errorExitNowhere(node.getCodeLoc());
            return NodeVal();
        }
    }
    if (targetBlock.type.has_value()) {
        msgs->errorExitPassingBlock(node.getCodeLoc());
        return NodeVal();
    }

    if (!performExit(node.getCodeLoc(), targetBlock, nodeCond)) return NodeVal();
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

    SymbolTable::Block targetBlock;
    if (hasName) {
        optional<SymbolTable::Block> targetBlockOpt = symbolTable->getBlock(name.value());
        if (!targetBlockOpt.has_value()) {
            msgs->errorBlockNotFound(node.getChild(indName).getCodeLoc(), name.value());
            return NodeVal();
        }
        targetBlock = targetBlockOpt.value();
    } else {
        targetBlock = symbolTable->getLastBlock();
        if (checkInGlobalScope(node.getCodeLoc(), false)) {
            msgs->errorLoopNowhere(node.getCodeLoc());
            return NodeVal();
        }
    }

    if (!performLoop(node.getCodeLoc(), targetBlock, nodeCond)) return NodeVal();
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

    SymbolTable::Block targetBlock;
    if (hasName) {
        optional<SymbolTable::Block> targetBlockOpt = symbolTable->getBlock(name.value());
        if (!targetBlockOpt.has_value()) {
            msgs->errorBlockNotFound(node.getChild(indName).getCodeLoc(), name.value());
            return NodeVal();
        }
        targetBlock = targetBlockOpt.value();
    } else {
        targetBlock = symbolTable->getLastBlock();
        if (checkInGlobalScope(node.getCodeLoc(), false)) {
            msgs->errorPassNonPassingBlock(node.getCodeLoc());
            return NodeVal();
        }
    }
    if (!targetBlock.type.has_value()) {
        msgs->errorPassNonPassingBlock(node.getCodeLoc());
        return NodeVal();
    }

    NodeVal processed = processNode(node.getChild(indVal));
    if (processed.isInvalid()) return NodeVal();

    NodeVal moved;
    if (processed.getLifetimeInfo().has_value() &&
        processed.getLifetimeInfo().value().nestLevel.has_value() &&
        !processed.getLifetimeInfo().value().nestLevel.value().greaterThan(symbolTable->currNestLevel())) {
        moved = moveNode(processed.getCodeLoc(), processed);
    } else {
        moved = move(processed);
    }
    if (moved.isInvalid()) return NodeVal();

    NodeVal casted = implicitCast(moved, targetBlock.type.value());
    if (casted.isInvalid()) return NodeVal();

    if (!checkTransferValueOk(casted.getCodeLoc(), casted, false, true)) return NodeVal();

    if (!performPass(node.getCodeLoc(), targetBlock, casted)) return NodeVal();
    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processFixed(const NodeVal &node, const NodeVal &starting) {
    if (!checkExactlyChildren(node, 3, true)) return NodeVal();

    optional<bool> attrGlobal = getAttributeForBool(starting, "global");
    if (!attrGlobal.has_value()) return NodeVal();

    if (!attrGlobal.value() && !checkInGlobalScope(node.getCodeLoc(), true)) {
        return NodeVal();
    }

    NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(1));
    if (nodeName.isInvalid()) return NodeVal();
    NamePool::Id name = nodeName.getEvalVal().id;
    if (!symbolTable->nameAvailable(name, namePool, typeTable, true)) {
        msgs->errorUnknown(nodeName.getCodeLoc());
        return NodeVal();
    }

    NodeVal nodeTy = processNode(node.getChild(2));
    if (nodeTy.isInvalid()) return NodeVal();
    if (!checkIsType(nodeTy, true)) return NodeVal();
    TypeTable::Id ty = nodeTy.getEvalVal().ty;

    TypeTable::FixedType fixed;
    fixed.name = name;
    fixed.type = ty;
    optional<TypeTable::Id> typeId = typeTable->addFixedType(fixed);
    if (!typeId.has_value()) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

    return promoteType(node.getCodeLoc(), typeId.value());
}

NodeVal Processor::processData(const NodeVal &node, const NodeVal &starting) {
    if (!checkBetweenChildren(node, 2, 4, true)) return NodeVal();

    optional<bool> attrGlobal = getAttributeForBool(starting, "global");
    if (!attrGlobal.has_value()) return NodeVal();

    if (!attrGlobal.value() && !checkInGlobalScope(node.getCodeLoc(), true)) {
        return NodeVal();
    }

    bool definition = node.getChildrenCnt() >= 3;
    bool withDrop = node.getChildrenCnt() >= 4;

    size_t indName = 1;
    size_t indMembs = definition ? 2 : 0;
    size_t indDrop = withDrop ? 3 : 0;

    TypeTable::DataType dataType;
    dataType.defined = false;

    NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(indName));
    if (nodeName.isInvalid()) return NodeVal();
    dataType.name = nodeName.getEvalVal().id;
    if (!symbolTable->nameAvailable(dataType.name, namePool, typeTable, true)) {
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
        NodeVal nodeMembs = processWithEscape(node.getChild(indMembs));
        if (nodeMembs.isInvalid()) return NodeVal();
        if (!checkIsRaw(nodeMembs, true)) return NodeVal();
        if (!checkAtLeastChildren(nodeMembs, 1, true)) return NodeVal();

        for (size_t i = 0; i < nodeMembs.getChildrenCnt(); ++i) {
            const NodeVal &nodeMemb = nodeMembs.getChild(i);

            pair<NodeVal, optional<NodeVal>> memb = processForIdTypePair(nodeMemb);
            if (memb.first.isInvalid()) return NodeVal();

            NamePool::Id membName = memb.first.getEvalVal().id;

            if (!memb.second.has_value()) {
                msgs->errorMissingTypeAttribute(nodeMemb.getCodeLoc());
                return NodeVal();
            }

            TypeTable::Id membType = memb.second.value().getEvalVal().ty;
            if (typeTable->worksAsTypeCn(membType)) {
                msgs->errorDataCnMember(memb.second.value().getCodeLoc());
                return NodeVal();
            }
            if (typeTable->isUndef(membType)) {
                msgs->errorUnknown(memb.second.value().getCodeLoc());
                return NodeVal();
            }

            optional<bool> attrNoZero = getAttributeForBool(nodeMemb, "noZero");
            if (!attrNoZero.has_value()) return NodeVal();

            TypeTable::DataType::MembEntry membEntry;
            membEntry.name = membName;
            membEntry.type = membType;
            membEntry.noZeroInit = attrNoZero.value();

            dataType.members.push_back(membEntry);
        }

        typeIdOpt = typeTable->addDataType(dataType);
        if (!typeIdOpt.has_value()) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }

        if (withDrop) {
            NodeVal nodeDrop = processNode(node.getChild(indDrop));
            if (nodeDrop.isInvalid()) return NodeVal();
            if (!checkIsDropFuncType(nodeDrop, typeIdOpt.value(), true)) return NodeVal();
            symbolTable->registerDropFunc(typeIdOpt.value(), nodeDrop);
        }
    }

    return promoteType(node.getCodeLoc(), typeIdOpt.value());
}

NodeVal Processor::processCall(const NodeVal &node, const NodeVal &starting) {
    size_t providedArgCnt = node.getChildrenCnt()-1;

    vector<NodeVal> args;
    args.reserve(providedArgCnt);
    vector<TypeTable::Id> argTypes;
    argTypes.reserve(providedArgCnt);
    bool allArgsEval = true;
    for (size_t i = 0; i < providedArgCnt; ++i) {
        NodeVal arg = processNode(node.getChild(i+1));
        if (arg.isInvalid()) return NodeVal();
        if (!checkHasType(arg, true)) return NodeVal();

        if (!checkIsEvalVal(arg, false)) allArgsEval = false;

        args.push_back(move(arg));
        argTypes.push_back(arg.getType().value());
    }

    NodeVal ret;

    const TypeTable::Callable *callable = nullptr;

    if (starting.isUndecidedCallableVal()) {
        optional<FuncId> funcId;

        vector<FuncId> funcIds = symbolTable->getFuncIds(starting.getUndecidedCallableVal().name);

        // first, try to find a func that doesn't require implicit casts
        for (FuncId it : funcIds) {
            if (argsFitFuncCall(args, BaseCallableValue::getCallableSig(symbolTable->getFunc(it), typeTable), false)) {
                funcId = it;
                break;
            }
        }

        // if not found, look through functions which do require implicit casts
        if (!funcId.has_value()) {
            for (FuncId it : funcIds) {
                if (argsFitFuncCall(args, BaseCallableValue::getCallableSig(symbolTable->getFunc(it), typeTable), true)) {
                    if (funcId.has_value()) {
                        // error due to call ambiguity
                        msgs->errorUnknown(node.getCodeLoc());
                        return NodeVal();
                    }
                    funcId = it;
                }
            }
        }

        if (!funcId.has_value()) {
            msgs->errorFuncNotFound(starting.getCodeLoc(), starting.getUndecidedCallableVal().name);
            return NodeVal();
        }

        callable = &BaseCallableValue::getCallable(symbolTable->getFunc(funcId.value()), typeTable);

        if (!implicitCastArgsAndVerifyCallOk(node.getCodeLoc(), args, *callable)) return NodeVal();

        ret = dispatchCall(node.getCodeLoc(), funcId.value(), args, allArgsEval);
    } else {
        if (!starting.getType().has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        callable = typeTable->extractCallable(starting.getType().value());
        if (callable == nullptr) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        if (!argsFitFuncCall(args, *callable, true)) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }

        if (!implicitCastArgsAndVerifyCallOk(node.getCodeLoc(), args, *callable)) return NodeVal();

        ret = dispatchCall(node.getCodeLoc(), starting, args, allArgsEval);
    }

    if (ret.isInvalid()) return NodeVal();

    for (size_t i = 0; i < args.size(); ++i) {
        size_t ind = args.size()-1-i;

        // variadic arguments cannot be marked noDrop
        bool argNoDrop = ind < callable->getArgCnt() && callable->getArgNoDrop(ind);

        // if argument was marked noDrop, it becomes the callers responsibility to drop it
        if (argNoDrop && !callDropFuncNonRef(move(args[ind]))) return NodeVal();
    }

    return ret;
}

// TODO passing attributes on macro itself
NodeVal Processor::processInvoke(const NodeVal &node, const NodeVal &starting) {
    optional<MacroId> macroId;
    if (starting.isUndecidedCallableVal()) {
        SymbolTable::MacroCallSite callSite;
        callSite.name = starting.getUndecidedCallableVal().name;
        callSite.argCnt = node.getChildrenCnt()-1;
    
        macroId = symbolTable->getMacroId(callSite, typeTable);
        if (!macroId.has_value()) {
            msgs->errorMacroNotFound(node.getCodeLoc(), starting.getUndecidedCallableVal().name);
            return NodeVal();
        }
    } else {
        if (!starting.getEvalVal().m.has_value()) {
            msgs->errorMacroNoValue(starting.getCodeLoc());
            return NodeVal();
        }
        macroId = starting.getEvalVal().m;
    }

    const MacroValue &macroVal = symbolTable->getMacro(macroId.value());

    const TypeTable::Callable &callable = BaseCallableValue::getCallable(macroVal, typeTable);

    size_t providedArgCnt = node.getChildrenCnt()-1;
    if ((callable.variadic && providedArgCnt+1 < callable.getArgCnt()) ||
        (!callable.variadic && providedArgCnt != callable.getArgCnt())) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();    
    }

    vector<NodeVal> args;
    for (size_t i = 0; i < providedArgCnt; ++i) {
        // all variadic arguments have the same pre-handling
        EscapeScore escapeScore = MacroValue::toEscapeScore(macroVal.argPreHandling[min(i, callable.getArgCnt()-1)]);

        NodeVal arg = processWithEscape(node.getChild(i+1), escapeScore);
        if (arg.isInvalid()) return NodeVal();

        args.push_back(move(arg));
    }

    NodeVal ret = invoke(node.getCodeLoc(), macroId.value(), move(args));
    if (ret.isInvalid()) return NodeVal();

    return ret;
}

NodeVal Processor::processFnc(const NodeVal &node, const NodeVal &starting) {
    if (!checkBetweenChildren(node, 3, 5, true)) return NodeVal();

    bool isType = node.getChildrenCnt() == 3;
    bool isDecl = node.getChildrenCnt() == 4;
    bool isDef = node.getChildrenCnt() == 5;

    if (isType) return processFncType(node);

    // fnc name args ret OR fnc name args ret body
    size_t indName = 1;
    size_t indArgs = 2;
    size_t indRet = 3;
    size_t indBody = isDef ? 4 : 0;

    // fnc
    {
        optional<bool> attrGlobal = getAttributeForBool(starting, "global");
        if (!attrGlobal.has_value()) return NodeVal();

        if (!attrGlobal.value() && !checkInGlobalScope(node.getCodeLoc(), true)) {
            return NodeVal();
        }
    }

    // name
    CodeLoc nameCodeLoc;
    NamePool::Id name;
    bool noNameMangle;
    bool isMain;
    bool evaluable, compilable;
    {
        NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        nameCodeLoc = nodeName.getCodeLoc();
        name = nodeName.getEvalVal().id;
        if (!symbolTable->nameAvailable(name, namePool, typeTable, true) &&
            !symbolTable->isFuncName(name)) {
            msgs->errorFuncNameTaken(nameCodeLoc, name);
            return NodeVal();
        }

        optional<bool> attrNoNameMangle = getAttributeForBool(nodeName, "noNameMangle");
        if (!attrNoNameMangle.has_value()) return NodeVal();
        isMain = isMeaningful(name, Meaningful::MAIN);
        noNameMangle = attrNoNameMangle.value();

        optional<bool> attrEvaluableOpt = getAttributeForBool(nodeName, "evaluable", this == evaluator);
        if (!attrEvaluableOpt.has_value()) return NodeVal();
        evaluable = attrEvaluableOpt.value();

        optional<bool> attrCompilableOpt = getAttributeForBool(nodeName, "compilable", this == compiler);
        if (!attrCompilableOpt.has_value()) return NodeVal();
        compilable = attrCompilableOpt.value();

        if (!evaluable && !compilable) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }
    }

    // arguments
    vector<NamePool::Id> argNames;
    vector<TypeTable::Id> argTypes;
    vector<bool> argNoDrops;
    const NodeVal &nodeArgs = processWithEscape(node.getChild(indArgs));
    if (nodeArgs.isInvalid()) return NodeVal();
    if (!checkIsRaw(nodeArgs, true)) return NodeVal();
    argNames.reserve(nodeArgs.getChildrenCnt());
    argTypes.reserve(nodeArgs.getChildrenCnt());
    argNoDrops.reserve(nodeArgs.getChildrenCnt());
    optional<bool> variadic = getAttributeForBool(nodeArgs, "variadic");
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

        TypeTable::Id argTy = arg.second.value().getEvalVal().ty;
        if (isDef && typeTable->isUndef(argTy)) {
            msgs->errorUnknown(arg.second.value().getCodeLoc());
            return NodeVal();
        }

        optional<bool> attrNoDrop = getAttributeForBool(arg.first, "noDrop");
        if (!attrNoDrop.has_value()) return NodeVal();

        argNames.push_back(argId);
        argTypes.push_back(argTy);
        argNoDrops.push_back(attrNoDrop.value());
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
            if (isDef && typeTable->isUndef(retType.value())) {
                msgs->errorUnknown(nodeRetType.getCodeLoc());
                return NodeVal();
            }
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
        callable.setArgCnt(argTypes.size());
        callable.setArgTypes(argTypes);
        callable.setArgNoDrops(argNoDrops);
        callable.retType = retType;
        callable.variadic = variadic.value();
        optional<TypeTable::Id> typeOpt = typeTable->addCallable(callable);
        if (!typeOpt.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }
        type = typeOpt.value();
    }

    FuncValue funcVal;
    funcVal.codeLoc = nameCodeLoc;
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

        SymbolTable::VarEntry varEntry;
        varEntry.name = name;
        varEntry.var = NodeVal(node.getCodeLoc(), undecidedVal);

        symbolTable->addVar(move(varEntry), true);
    }

    // funcVal is passed by mutable reference, is needed later
    if (evaluable) {
        if (!evaluator->performFunctionDeclaration(node.getCodeLoc(), funcVal)) return NodeVal();
    }
    if (compilable) {
        if (!compiler->performFunctionDeclaration(node.getCodeLoc(), funcVal)) return NodeVal();
    }

    optional<FuncId> symbId = symbolTable->registerFunc(funcVal);
    if (!symbId.has_value()) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }
    FuncValue &symbVal = symbolTable->getFunc(symbId.value());

    if (isDef) {
        if (evaluable) {
            if (!evaluator->performFunctionDefinition(node.getCodeLoc(), nodeArgs, *nodeBodyPtr, symbVal))
                return NodeVal();
        }
        if (compilable) {
            if (!compiler->performFunctionDefinition(node.getCodeLoc(), nodeArgs, *nodeBodyPtr, symbVal))
                return NodeVal();
        }
    }

    if (checkIsEvalFunc(node.getCodeLoc(), symbVal, false)) {
        return evaluator->performLoad(node.getCodeLoc(), symbId.value());
    } else {
        return compiler->performLoad(node.getCodeLoc(), symbId.value());
    }
}

NodeVal Processor::processMac(const NodeVal &node, const NodeVal &starting) {
    if (!checkExactlyChildren(node, 2, false) && !checkExactlyChildren(node, 4, true)) {
        return NodeVal();
    }

    bool isType = node.getChildrenCnt() == 2;
    bool isDef = node.getChildrenCnt() == 4;

    if (isType) return processMacType(node);

    // mac args OR mac name args body
    size_t indName = isDef ? 1 : 0;
    size_t indArgs = isType ? 1 : 2;
    size_t indBody = isDef ? 3 : 0;

    // mac
    {
        optional<bool> attrGlobal = getAttributeForBool(starting, "global");
        if (!attrGlobal.has_value()) return NodeVal();

        if (!attrGlobal.value() && !checkInGlobalScope(node.getCodeLoc(), true)) {
            return NodeVal();
        }
    }

    // name
    CodeLoc nameCodeLoc;
    NamePool::Id name;
    if (isDef) {
        NodeVal nodeName = processWithEscapeAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        nameCodeLoc = nodeName.getCodeLoc();
        name = nodeName.getEvalVal().id;
        if (!symbolTable->nameAvailable(name, namePool, typeTable, true) &&
            !symbolTable->isMacroName(name)) {
            msgs->errorMacroNameTaken(nameCodeLoc, name);
            return NodeVal();
        }
    }

    // arguments
    NodeVal nodeArgs = processWithEscape(node.getChild(indArgs));
    if (nodeArgs.isInvalid()) return NodeVal();
    if (!checkIsRaw(nodeArgs, true)) return NodeVal();
    vector<NamePool::Id> argNames;
    argNames.reserve(nodeArgs.getChildrenCnt());
    vector<MacroValue::PreHandling> argPreHandling;
    argPreHandling.reserve(nodeArgs.getChildrenCnt());
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

        optional<bool> isPreproc = getAttributeForBool(arg, "preprocess");
        if (!isPreproc.has_value()) return NodeVal();
        optional<bool> isPlusEsc = getAttributeForBool(arg, "plusEscape");
        if (!isPlusEsc.has_value()) return NodeVal();
        if (isPreproc.value() && isPlusEsc.value()) {
            msgs->errorUnknown(arg.getNonTypeAttrs().getCodeLoc());
            return NodeVal();
        }
        if (isPreproc.value()) argPreHandling.push_back(MacroValue::PREPROC);
        else if (isPlusEsc.value()) argPreHandling.push_back(MacroValue::PLUS_ESC);
        else argPreHandling.push_back(MacroValue::REGULAR);

        optional<bool> isVariadic = getAttributeForBool(arg, "variadic");
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
        callable.setArgCnt(argNames.size());
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
        macroVal.codeLoc = nameCodeLoc;
        BaseCallableValue::setType(macroVal, type, typeTable);
        macroVal.name = name;
        macroVal.argNames = move(argNames);
        macroVal.argPreHandling = move(argPreHandling);

        // register only if first macro of its name
        if (!symbolTable->isMacroName(name)) {
            UndecidedCallableVal undecidedVal;
            undecidedVal.isFunc = false;
            undecidedVal.name = name;

            SymbolTable::VarEntry varEntry;
            varEntry.name = name;
            varEntry.var = NodeVal(node.getCodeLoc(), undecidedVal);

            symbolTable->addVar(move(varEntry), true);
        }

        optional<MacroId> symbId = symbolTable->registerMacro(macroVal, typeTable);
        if (!symbId.has_value()) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }
        MacroValue &symbVal = symbolTable->getMacro(symbId.value());

        if (!evaluator->performMacroDefinition(node.getCodeLoc(), nodeArgs, *nodeBodyPtr, symbVal)) return NodeVal();

        return evaluator->performLoad(node.getCodeLoc(), symbId.value());
    } else {
        return promoteType(node.getCodeLoc(), type);
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

            NodeVal processed = processNode(node.getChild(1));
            if (processed.isInvalid()) return NodeVal();

            NodeVal moved;
            if (processed.getLifetimeInfo().has_value() &&
                processed.getLifetimeInfo().value().nestLevel.has_value() &&
                !processed.getLifetimeInfo().value().nestLevel.value().callableGreaterThan(symbolTable->currNestLevel())) {
                moved = moveNode(processed.getCodeLoc(), processed);
            } else {
                moved = move(processed);
            }
            if (moved.isInvalid()) return NodeVal();

            NodeVal casted = implicitCast(moved, optCallee.value().retType.value());
            if (casted.isInvalid()) return NodeVal();

            if (!checkTransferValueOk(casted.getCodeLoc(), casted, false, true)) return NodeVal();

            if (!performRet(node.getCodeLoc(), casted)) return NodeVal();
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

        NodeVal processed = processNode(node.getChild(1));
        if (processed.isInvalid()) return NodeVal();

        NodeVal moved;
        if (processed.getLifetimeInfo().has_value() &&
            processed.getLifetimeInfo().value().nestLevel.has_value() &&
            !processed.getLifetimeInfo().value().nestLevel.value().callableGreaterThan(symbolTable->currNestLevel())) {
            moved = moveNode(processed.getCodeLoc(), processed);
        } else {
            moved = move(processed);
        }
        if (moved.isInvalid()) return NodeVal();

        if (!performRet(node.getCodeLoc(), moved)) return NodeVal();

        return NodeVal(node.getCodeLoc());
    }
}

NodeVal Processor::processEval(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) {
        return NodeVal();
    }

    return evaluator->processNode(node.getChild(1));
}

NodeVal Processor::processImport(const NodeVal &node, bool topmost) {
    if (!topmost) {
        msgs->errorNotTopmost(node.getCodeLoc());
        return NodeVal();
    }

    if (!checkExactlyChildren(node, 2, true)) {
        return NodeVal();
    }

    NodeVal file = processNode(node.getChild(1));
    if (file.isInvalid()) return NodeVal();
    if (!file.isEvalVal() || !EvalVal::isNonNullStr(file.getEvalVal(), typeTable)) {
        msgs->errorImportNotString(file.getCodeLoc());
        return NodeVal();
    }

    return NodeVal(node.getCodeLoc(), file.getEvalVal().str.value());
}

NodeVal Processor::processMessage(const NodeVal &node, const NodeVal &starting) {
    if (!checkAtLeastChildren(node, 2, true)) {
        return NodeVal();
    }

    optional<bool> attrWarning = getAttributeForBool(starting, "warning");
    if (!attrWarning.has_value()) return NodeVal();
    optional<bool> attrError = getAttributeForBool(starting, "error");
    if (!attrError.has_value()) return NodeVal();
    if (attrWarning.value() && attrError.value()) {
        msgs->errorUnknown(starting.getCodeLoc());
        return NodeVal();
    }

    CompilationMessages::Status status = CompilationMessages::S_INFO;
    if (attrWarning.value()) status = CompilationMessages::S_WARNING;
    else if (attrError.value()) status = CompilationMessages::S_ERROR;

    optional<NodeVal> attrLoc = getAttribute(starting, "loc");
    CodeLoc codeLoc = attrLoc.has_value() ? attrLoc.value().getCodeLoc() : node.getCodeLoc();

    vector<NodeVal> opers;
    opers.reserve(node.getChildrenCnt()-1);

    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        NodeVal nodeVal = processNode(node.getChild(i));
        if (nodeVal.isInvalid()) return NodeVal();
        if (!checkIsEvalVal(nodeVal, true)) return NodeVal();

        opers.push_back(move(nodeVal));
    }

    if (!msgs->userMessageStart(codeLoc, status)) {
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    for (size_t i = 0; i < opers.size(); ++i) {
        const EvalVal &evalVal = opers[i].getEvalVal();

        if (EvalVal::isI(evalVal, typeTable)) {
            msgs->userMessage(EvalVal::getValueI(evalVal, typeTable).value());
        } else if (EvalVal::isU(evalVal, typeTable)) {
            msgs->userMessage(EvalVal::getValueU(evalVal, typeTable).value());
        } else if (EvalVal::isF(evalVal, typeTable)) {
            msgs->userMessage(EvalVal::getValueF(evalVal, typeTable).value());
        } else if (EvalVal::isC(evalVal, typeTable)) {
            msgs->userMessage(evalVal.c8);
        } else if (EvalVal::isB(evalVal, typeTable)) {
            msgs->userMessage(evalVal.b);
        } else if (EvalVal::isId(evalVal, typeTable)) {
            msgs->userMessage(evalVal.id);
        } else if (EvalVal::isType(evalVal, typeTable)) {
            msgs->userMessage(evalVal.ty);
        } else if (EvalVal::isNull(evalVal, typeTable)) {
            msgs->userMessageNull();
        } else if (EvalVal::isNonNullStr(evalVal, typeTable)) {
            msgs->userMessage(evalVal.str.value());
        } else {
            msgs->userMessageEnd();
            msgs->errorUnknown(opers[i].getCodeLoc());
            return NodeVal();
        }
    }

    msgs->userMessageEnd();

    if (attrLoc.has_value()) msgs->displayCodeSegment(codeLoc);

    if (attrError.value()) return NodeVal();
    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processOper(const NodeVal &node, const NodeVal &starting, Oper op) {
    if (!checkAtLeastChildren(node, 2, true)) return NodeVal();

    if (checkExactlyChildren(node, 2, false)) {
        return processOperUnary(node.getCodeLoc(), node.getChild(1), op);
    }

    OperInfo operInfo = operInfos.find(op)->second;

    optional<bool> attrBare = getAttributeForBool(starting, "bare");
    if (!attrBare.has_value()) return NodeVal();

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
        return processOperDot(node.getCodeLoc(), operands);
    } else {
        return processOperRegular(node.getCodeLoc(), operands, op, attrBare.value());
    }
}

NodeVal Processor::processTypeOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();
    if (!checkHasType(operand, true)) return NodeVal();

    TypeTable::Id ty = operand.getType().value();

    if (!callDropFuncNonRef(move(operand))) return NodeVal();

    return promoteType(node.getCodeLoc(), ty);
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
    if (typeTable->worksAsTypeArr(ty)){
        len = typeTable->extractLenOfArr(ty).value();
    } else if (typeTable->worksAsTuple(ty)) {
        len = typeTable->extractLenOfTuple(ty).value();
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_RAW)) {
        len = operand.getChildrenCnt();
    } else if (typeTable->worksAsDataType(ty)) {
        const TypeTable::DataType &dataType = *typeTable->extractDataType(ty);
        if (!dataType.defined) {
            msgs->errorUnknown(operand.getCodeLoc());
            return NodeVal();
        }

        len = dataType.members.size();
    } else if (checkIsEvalVal(operand, false) && EvalVal::isNonNullStr(operand.getEvalVal(), typeTable)) {
        len = LiteralVal::getStringLen(stringPool->get(operand.getEvalVal().str.value()));
    } else {
        msgs->errorLenOfBadType(node.getCodeLoc());
        return NodeVal();
    }

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::WIDEST_U), typeTable);
    evalVal.getWidestU() = len;

    if (!callDropFuncNonRef(move(operand))) return NodeVal();

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
    if (typeTable->isUndef(ty)) {
        msgs->errorUnknown(operand.getCodeLoc());
        return NodeVal();
    }

    optional<uint64_t> size = compiler->performSizeOf(node.getCodeLoc(), ty);
    if (!size.has_value()) return NodeVal();

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::WIDEST_U), typeTable);
    evalVal.getWidestU() = size.value();

    if (!callDropFuncNonRef(move(operand))) return NodeVal();

    return NodeVal(node.getCodeLoc(), move(evalVal));
}

NodeVal Processor::processIsDef(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal name = processWithEscapeAndCheckIsId(node.getChild(1));
    if (name.isInvalid()) return NodeVal();

    NamePool::Id id = name.getEvalVal().id;

    return promoteBool(node.getCodeLoc(), symbolTable->isVarName(id));
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

    if (!callDropFuncNonRef(move(operand))) return NodeVal();

    return move(nodeAttr.value());
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

            attrIsDef = nonTypeAttrs.getAttrMap().attrMap.find(attrName) != nonTypeAttrs.getAttrMap().attrMap.end();
        }
    }

    if (!callDropFuncNonRef(move(operand))) return NodeVal();

    return promoteBool(node.getCodeLoc(), attrIsDef);
}

// returns nullopt if not found
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

        auto loc = nonTypeAttrs.getAttrMap().attrMap.find(attrName);
        if (loc == nonTypeAttrs.getAttrMap().attrMap.end()) return nullopt;

        return *loc->second;
    }
}

optional<NodeVal> Processor::getAttribute(const NodeVal &node, const string &attrStrName) {
    return getAttribute(node, namePool->add(attrStrName));
}

optional<bool> Processor::getAttributeForBool(const NodeVal &node, NamePool::Id attrName, bool default_) {
    optional<NodeVal> attr = getAttribute(node, attrName);
    if (!attr.has_value()) return default_;
    if (!checkIsEvalVal(attr.value(), true)) return nullopt;
    if (!checkIsBool(attr.value(), true)) return nullopt;
    return attr.value().getEvalVal().b;
}

optional<bool> Processor::getAttributeForBool(const NodeVal &node, const std::string &attrStrName, bool default_) {
    return getAttributeForBool(node, namePool->add(attrStrName), default_);
}

NodeVal Processor::promoteBool(CodeLoc codeLoc, bool b) const {
    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
    evalVal.b = b;
    return NodeVal(codeLoc, evalVal);
}

NodeVal Processor::promoteType(CodeLoc codeLoc, TypeTable::Id ty) const {
    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
    evalVal.ty = ty;
    return NodeVal(codeLoc, move(evalVal));
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

NodeVal Processor::dispatchLoad(CodeLoc codeLoc, VarId varId, optional<NamePool::Id> id) {
    if (checkIsEvalTime(symbolTable->getVar(varId).var, false)) {
        return evaluator->performLoad(codeLoc, varId);
    } else {
        return performLoad(codeLoc, varId);
    }
}

NodeVal Processor::implicitCast(const NodeVal &node, TypeTable::Id ty, bool skipCheckNeedsDrop) {
    if (!checkImplicitCastable(node, ty, true)) return NodeVal();

    return castNode(node.getCodeLoc(), node, ty, skipCheckNeedsDrop);
}

NodeVal Processor::castNode(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty, bool skipCheckNeedsDrop) {
    if (node.getType().value() == ty) {
        NodeVal ret(node);
        ret.setCodeLoc(codeLoc);
        return ret;
    }

    if (!skipCheckNeedsDrop && !checkTransferValueOk(codeLoc, node, node.isNoDrop(), true)) return NodeVal();

    if (checkIsEvalTime(node, false) && !shouldNotDispatchCastToEval(node, ty)) {
        return evaluator->performCast(codeLoc, node, ty);
    } else {
        return performCast(codeLoc, node, ty);
    }
}

bool Processor::implicitCastOperands(NodeVal &lhs, NodeVal &rhs, bool oneWayOnly) {
    if (lhs.getType().value() == rhs.getType().value()) return true;

    if (checkImplicitCastable(rhs, lhs.getType().value(), false)) {
        rhs = castNode(rhs.getCodeLoc(), rhs, lhs.getType().value());
        return !rhs.isInvalid();
    } else if (!oneWayOnly && checkImplicitCastable(lhs, rhs.getType().value(), false)) {
        lhs = castNode(lhs.getCodeLoc(), lhs, rhs.getType().value());
        return !lhs.isInvalid();
    } else {
        if (oneWayOnly) msgs->errorExprCannotImplicitCast(rhs.getCodeLoc(), rhs.getType().value(), lhs.getType().value());
        else msgs->errorExprCannotImplicitCastEither(rhs.getCodeLoc(), rhs.getType().value(), lhs.getType().value());
        return false;
    }
}

bool Processor::shouldNotDispatchCastToEval(const NodeVal &node, TypeTable::Id dstTypeId) const {
    if (!node.isEvalVal()) return true;

    const EvalVal &val = node.getEvalVal();

    TypeTable::Id srcTypeId = val.type;

    if (typeTable->isImplicitCastable(typeTable->extractFixedTypeBaseType(srcTypeId), typeTable->extractFixedTypeBaseType(dstTypeId)))
        return false;

    if (typeTable->worksAsTypeI(srcTypeId)) {
        if (EvalVal::getValueI(val, typeTable).value() != 0) {
            return typeTable->worksAsTypeAnyP(dstTypeId);
        }
    } else if (typeTable->worksAsTypeU(srcTypeId)) {
        if (EvalVal::getValueU(val, typeTable).value() != 0) {
            return typeTable->worksAsTypeAnyP(dstTypeId);
        }
    } else if (typeTable->worksAsTypeStr(srcTypeId)) {
        if (val.str.has_value()) {
            return typeTable->worksAsTypeI(dstTypeId) ||
                typeTable->worksAsTypeU(dstTypeId) ||
                typeTable->worksAsTypeAnyP(dstTypeId);
        }
    } else if (typeTable->worksAsTypeAnyP(srcTypeId)) {
        if (!EvalVal::isNull(val, typeTable)) {
            return typeTable->worksAsTypeI(dstTypeId) ||
                typeTable->worksAsTypeU(dstTypeId) ||
                typeTable->worksAsTypeB(dstTypeId) ||
                typeTable->worksAsTypeAnyP(dstTypeId);
        }
    } else if (typeTable->worksAsCallable(srcTypeId)) {
        if (!EvalVal::isCallableNoValue(val, typeTable)) {
            return typeTable->worksAsTypeAnyP(dstTypeId);
        }
    } else if (typeTable->worksAsTuple(srcTypeId)) {
        if (typeTable->worksAsTuple(dstTypeId)) {
            const TypeTable::Tuple &tupSrc = *typeTable->extractTuple(srcTypeId);
            const TypeTable::Tuple &tupDst = *typeTable->extractTuple(dstTypeId);

            if (tupSrc.members.size() != tupDst.members.size()) return false;

            for (size_t i = 0; i < tupSrc.members.size(); ++i) {
                if (shouldNotDispatchCastToEval(val.elems[i], tupDst.members[i])) return true;
            }
        }
    }

    return false;
}

bool Processor::implicitCastArgsAndVerifyCallOk(CodeLoc codeLoc, vector<NodeVal> &args, const TypeTable::Callable &callable) {
    if (callable.retType.has_value() && typeTable->isUndef(callable.retType.value())) {
        msgs->errorUnknown(codeLoc);
        return false;
    }

    for (size_t i = 0; i < callable.getArgCnt(); ++i) {
        args[i] = implicitCast(args[i], callable.getArgType(i), true);
        if (args[i].isInvalid()) return false;

        if (!checkTransferValueOk(args[i].getCodeLoc(), args[i], callable.getArgNoDrop(i), true)) return false;
    }

    return true;
}

NodeVal Processor::dispatchCall(CodeLoc codeLoc, const NodeVal &func, const std::vector<NodeVal> &args, bool allArgsEval) {
    if (func.isEvalVal() && allArgsEval) {
        return evaluator->performCall(codeLoc, func, args);
    } else {
        return performCall(codeLoc, func, args);
    }
}

NodeVal Processor::dispatchCall(CodeLoc codeLoc, FuncId funcId, const std::vector<NodeVal> &args, bool allArgsEval) {
    const FuncValue &func = symbolTable->getFunc(funcId);

    if (func.isEval() && allArgsEval) {
        return evaluator->performCall(codeLoc, funcId, args);
    } else {
        return performCall(codeLoc, funcId, args);
    }
}

NodeVal Processor::dispatchOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) {
    if (!checkHasType(oper, true)) return NodeVal();

    optional<TypeTable::Id> resTy = typeTable->addTypeDerefOf(oper.getType().value());
    if (!resTy.has_value()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }
    if (typeTable->isUndef(resTy.value())) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    if (checkIsEvalTime(oper, false)) {
        return evaluator->performOperUnaryDeref(codeLoc, oper, resTy.value());
    } else {
        return performOperUnaryDeref(codeLoc, oper, resTy.value());
    }
}

NodeVal Processor::dispatchAssignment(CodeLoc codeLoc, NodeVal &lhs, const NodeVal &rhs) {
    if (checkIsEvalTime(lhs, false) && checkIsEvalTime(rhs, false)) {
        return evaluator->performOperAssignment(codeLoc, lhs, rhs);
    } else {
        return performOperAssignment(codeLoc, lhs, rhs);
    }
}

NodeVal Processor::getElement(CodeLoc codeLoc, NodeVal &array, size_t index) {
    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::WIDEST_U), typeTable);
    evalVal.getWidestU() = index;
    return getElement(codeLoc, array, NodeVal(codeLoc, move(evalVal)));
}

NodeVal Processor::getElement(CodeLoc codeLoc, NodeVal &array, const NodeVal &index) {
    TypeTable::Id arrayType = array.getType().value();

    optional<TypeTable::Id> elemType = typeTable->addTypeIndexOf(arrayType);
    if (!elemType.has_value()) {
        msgs->errorExprIndexOnBadType(array.getCodeLoc(), arrayType);
        return NodeVal();
    }
    if (typeTable->isUndef(elemType.value())) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    TypeTable::Id resType = elemType.value();
    if (typeTable->worksAsTypeCn(arrayType)) resType = typeTable->addTypeCnOf(resType);

    if (checkIsEvalTime(array, false) && checkIsEvalTime(index, false)) {
        return evaluator->performOperIndex(codeLoc, array, index, resType);
    } else {
        return performOperIndex(array.getCodeLoc(), array, index, resType);
    }
}

NodeVal Processor::getRawMember(CodeLoc codeLoc, NodeVal &raw, size_t index) {
    TypeTable::Id rawType = raw.getType().value();

    TypeTable::Id resType = raw.getChild(index).getType().value();
    if (typeTable->worksAsTypeCn(rawType)) resType = typeTable->addTypeCnOf(resType);

    return evaluator->performOperDot(codeLoc, raw, index, resType);
}

NodeVal Processor::getTupleMember(CodeLoc codeLoc, NodeVal &tuple, size_t index) {
    TypeTable::Id tupleType = tuple.getType().value();

    TypeTable::Id resType = typeTable->extractTupleMemberType(tupleType, index).value();

    if (checkIsEvalTime(tuple, false)) {
        return evaluator->performOperDot(codeLoc, tuple, index, resType);
    } else {
        return performOperDot(codeLoc, tuple, index, resType);
    }
}

NodeVal Processor::getDataMember(CodeLoc codeLoc, NodeVal &data, size_t index) {
    TypeTable::Id dataType = data.getType().value();

    TypeTable::Id resType = typeTable->extractDataTypeMemberType(dataType, index).value();

    if (checkIsEvalTime(data, false)) {
        return evaluator->performOperDot(codeLoc, data, index, resType);
    } else {
        return performOperDot(codeLoc, data, index, resType);
    }
}

bool Processor::argsFitFuncCall(const vector<NodeVal> &args, const TypeTable::Callable &callable, bool allowImplicitCasts) {
    if (callable.getArgCnt() != args.size() && !(callable.variadic && callable.getArgCnt() <= args.size()))
        return false;

    for (size_t i = 0; i < callable.getArgCnt(); ++i) {
        if (!checkHasType(args[i], false)) return false;

        // remove trailing cns
        TypeTable::Id typeSig = typeTable->addTypeDescrForSig(args[i].getType().value());

        bool sameType = typeSig == callable.getArgType(i);

        if (!sameType && !(allowImplicitCasts && checkImplicitCastable(args[i], callable.getArgType(i), false))) {
            return false;
        }
    }

    return true;
}

NodeVal Processor::loadUndecidedCallable(const NodeVal &node, const NodeVal &val) {
    if (val.getUndecidedCallableVal().isFunc) {
        vector<FuncId> funcIds = symbolTable->getFuncIds(val.getUndecidedCallableVal().name);
        if (funcIds.empty()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        optional<FuncId> funcId;

        if (node.hasTypeAttr()) {
            TypeTable::Id ty = node.getTypeAttr().getEvalVal().ty;
            for (FuncId it : funcIds) {
                if (symbolTable->getFunc(it).getType() == ty) {
                    funcId = it;
                    break;
                }
            }
            if (!funcId.has_value()) {
                msgs->errorFuncNotFound(node.getCodeLoc(), val.getUndecidedCallableVal().name);
                return NodeVal();
            }
        } else if (funcIds.size() == 1) {
            funcId = funcIds.front();
        } else {
            // still undecided
            return NodeVal(node.getCodeLoc(), val.getUndecidedCallableVal());
        }

        if (checkIsEvalFunc(node.getCodeLoc(), symbolTable->getFunc(funcId.value()), false))
            return evaluator->performLoad(node.getCodeLoc(), funcId.value());
        else
            return performLoad(node.getCodeLoc(), funcId.value());
    } else {
        vector<MacroId> macroIds = symbolTable->getMacros(val.getUndecidedCallableVal().name);
        if (macroIds.empty()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        optional<MacroId> macroId;

        if (node.hasTypeAttr()) {
            TypeTable::Id ty = node.getTypeAttr().getEvalVal().ty;
            for (MacroId it : macroIds) {
                if (symbolTable->getMacro(it).getType() == ty) {
                    macroId = it;
                    break;
                }
            }
            if (!macroId.has_value()) {
                msgs->errorMacroNotFound(node.getCodeLoc(), val.getUndecidedCallableVal().name);
                return NodeVal();
            }
        } else if (macroIds.size() == 1) {
            macroId = macroIds.front();
        } else {
            // still undecided
            return NodeVal(node.getCodeLoc(), val.getUndecidedCallableVal());
        }

        return evaluator->performLoad(node.getCodeLoc(), macroId.value());
    }
}

// TODO optimize on raw - don't need to copy children, can just move instead
NodeVal Processor::moveNode(CodeLoc codeLoc, NodeVal &val) {
    if (!checkHasType(val, true)) return NodeVal();

    if (val.isNoDrop() || val.isInvokeArg()) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    if (!val.hasRef()) return NodeVal::copyNoRef(codeLoc, val, LifetimeInfo());

    TypeTable::Id valTy = val.getType().value();

    if (typeTable->worksAsTypeCn(valTy)) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    NodeVal prev = NodeVal::copyNoRef(codeLoc, val);

    NodeVal zero;
    if (checkIsEvalTime(val, false)) zero = evaluator->performZero(codeLoc, valTy);
    else zero = performZero(codeLoc, valTy);
    if (zero.isInvalid()) return NodeVal();

    if (dispatchAssignment(codeLoc, val, zero).isInvalid()) return NodeVal();

    return prev;
}

NodeVal Processor::invoke(CodeLoc codeLoc, MacroId macroId, vector<NodeVal> args) {
    const MacroValue &macroVal = symbolTable->getMacro(macroId);

    const TypeTable::Callable &callable = BaseCallableValue::getCallable(macroVal, typeTable);

    if (callable.variadic) {
        // if it's variadic, it has to have at least 1 argument (the variadic one)
        size_t nonVarArgCnt = callable.getArgCnt()-1;

        CodeLoc totalVarArgCodeLoc = args.size() == nonVarArgCnt ? codeLoc : args[nonVarArgCnt].getCodeLoc();
        NodeVal totalVarArg = NodeVal::makeEmpty(totalVarArgCodeLoc, typeTable);
        NodeVal::addChildren(totalVarArg, args.begin()+nonVarArgCnt, args.end(), typeTable);

        args.resize(nonVarArgCnt);
        args.push_back(move(totalVarArg));
    }

    NodeVal ret = evaluator->performInvoke(codeLoc, macroId, args);
    if (ret.isInvalid()) return NodeVal();

    return ret;
}

bool Processor::hasTrivialDrop(TypeTable::Id ty) {
    if (typeTable->worksAsTypeArr(ty)) {
        TypeTable::Id elemTy = typeTable->addTypeIndexOf(ty).value();
        return hasTrivialDrop(elemTy);
    } else if (typeTable->worksAsTuple(ty)) {
        size_t len = typeTable->extractLenOfTuple(ty).value();
        for (size_t i = 0; i < len; ++i) {
            if (!hasTrivialDrop(typeTable->extractTupleMemberType(ty, i).value())) return false;
        }

        return true;
    } else if (typeTable->worksAsDataType(ty)) {
        const NodeVal *dropFunc = symbolTable->getDropFunc(typeTable->extractFixedTypeBaseType(ty));
        if (dropFunc != nullptr) return false;

        size_t len = typeTable->extractLenOfDataType(ty).value();
        for (size_t i = 0; i < len; ++i) {
            if (!hasTrivialDrop(typeTable->extractDataTypeMemberType(ty, i).value())) return false;
        }

        return true;
    }

    return true;
}

bool Processor::checkNotNeedsDrop(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isNoDrop() && checkHasType(val, false) && !hasTrivialDrop(val.getType().value())) {
        if (orError) msgs->errorUnknown(codeLoc);
        return false;
    }
    return true;
}

bool Processor::callDropFunc(CodeLoc codeLoc, NodeVal val) {
    if (!checkHasType(val, false) || val.isNoDrop()) return true;

    // don't drop values propagated through macro args
    if (val.isInvokeArg()) return true;

    TypeTable::Id valTy = val.getType().value();

    if (typeTable->worksAsTypeArr(valTy)) {
        size_t len = typeTable->extractLenOfArr(valTy).value();
        for (size_t i = 0; i < len; ++i) {
            size_t ind = len-1-i;

            NodeVal elem = getElement(codeLoc, val, ind);
            if (elem.isInvalid()) return false;

            if (!callDropFunc(codeLoc, move(elem))) return false;
        }
    } else if (typeTable->worksAsTuple(valTy)) {
        size_t len = typeTable->extractLenOfTuple(valTy).value();
        for (size_t i = 0; i < len; ++i) {
            size_t ind = len-1-i;

            NodeVal memb = getTupleMember(codeLoc, val, ind);
            if (memb.isInvalid()) return false;

            if (!callDropFunc(codeLoc, move(memb))) return false;
        }
    } else if (typeTable->worksAsDataType(valTy)) {
        const NodeVal *dropFunc = symbolTable->getDropFunc(typeTable->extractFixedTypeBaseType(valTy));
        if (dropFunc != nullptr) {
            const TypeTable::Callable &dropCallable = *typeTable->extractCallable(dropFunc->getType().value());

            // explicit cast cuz could be a fixed type
            NodeVal afterCast = castNode(codeLoc, val, dropCallable.getArgType(0), true);
            if (afterCast.isInvalid()) return false;

            bool isEval = checkIsEvalVal(afterCast, false);

            if (dispatchCall(codeLoc, *dropFunc, {move(afterCast)}, isEval).isInvalid()) return false;
        }

        size_t len = typeTable->extractLenOfDataType(valTy).value();
        for (size_t i = 0; i < len; ++i) {
            size_t ind = len-1-i;

            NodeVal memb = getDataMember(codeLoc, val, ind);
            if (memb.isInvalid()) return false;

            if (!callDropFunc(codeLoc, move(memb))) return false;
        }
    }

    return true;
}

bool Processor::callDropFuncNonRef(NodeVal val) {
    if (val.hasRef()) return true;
    return callDropFunc(val.getCodeLoc(), move(val));
}

bool Processor::callDropFuncs(CodeLoc codeLoc, vector<VarId> vars) {
    for (const VarId &it : vars) {
        SymbolTable::VarEntry &varEntry = symbolTable->getVar(it);

        if (varEntry.var.isNoDrop()) continue;

        NodeVal loaded = dispatchLoad(codeLoc, it);

        if (!callDropFunc(codeLoc, move(loaded))) return false;
    }

    return true;
}

bool Processor::callDropFuncsCurrBlock(CodeLoc codeLoc) {
    return callDropFuncs(codeLoc, symbolTable->getVarsInRevOrderCurrBlock());
}

bool Processor::callDropFuncsFromBlockToCurrBlock(CodeLoc codeLoc, NamePool::Id name) {
    return callDropFuncs(codeLoc, symbolTable->getVarsInRevOrderFromBlockToCurrBlock(name));
}

bool Processor::callDropFuncsCurrCallable(CodeLoc codeLoc) {
    return callDropFuncs(codeLoc, symbolTable->getVarsInRevOrderCurrCallable());
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
        AttrMap attrMap;

        NodeVal nodeAttrs = processWithEscape(node.getNonTypeAttrs());
        if (nodeAttrs.isInvalid()) return false;

        if (!NodeVal::isRawVal(nodeAttrs, typeTable)) {
            if (!checkIsId(nodeAttrs, true)) return false;

            NamePool::Id attrName = nodeAttrs.getEvalVal().id;
            if (attrName == typeId || attrMap.attrMap.find(attrName) != attrMap.attrMap.end()) {
                msgs->errorUnknown(nodeAttrs.getCodeLoc());
                return false;
            }

            NodeVal attrVal = promoteBool(nodeAttrs.getCodeLoc(), true);

            attrMap.attrMap.insert({attrName, make_unique<NodeVal>(move(attrVal))});
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
                if (attrName == typeId || attrMap.attrMap.find(attrName) != attrMap.attrMap.end()) {
                    msgs->errorUnknown(nodeAttrEntry.getCodeLoc());
                    return false;
                }

                NodeVal attrVal;
                if (nodeAttrEntryVal == nullptr) {
                    attrVal = promoteBool(nodeAttrEntryName->getCodeLoc(), true);
                } else {
                    attrVal = processNode(*nodeAttrEntryVal);
                    if (attrVal.isInvalid()) return false;
                    if (!checkHasType(attrVal, true)) return false;
                    if (!attrVal.hasRef() && !attrVal.isInvokeArg() &&
                        !checkNotNeedsDrop(attrVal.getCodeLoc(), attrVal, true)) return false;
                }

                attrMap.attrMap.insert({attrName, make_unique<NodeVal>(move(attrVal))});
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

        if (NodeVal::isFunc(tmp, typeTable)) msgs->warnUnusedFunc(tmp.getCodeLoc());
        else if (NodeVal::isMacro(tmp, typeTable)) msgs->warnUnusedMacro(tmp.getCodeLoc());
        else if (tmp.isSpecialVal()) msgs->warnUnusedSpecial(tmp.getCodeLoc(), tmp.getSpecialVal());

        if (!callDropFuncNonRef(move(tmp))) return false;
    }

    return true;
}

NodeVal Processor::processFncType(const NodeVal &node) {
    // fnc argTypes ret
    size_t indArgs = 1;
    size_t indRet = 2;

    // arguments
    vector<TypeTable::Id> argTypes;
    vector<bool> argNoDrops;
    const NodeVal &nodeArgs = processWithEscape(node.getChild(indArgs));
    if (nodeArgs.isInvalid()) return NodeVal();
    if (!checkIsRaw(nodeArgs, true)) return NodeVal();
    argTypes.reserve(nodeArgs.getChildrenCnt());
    argNoDrops.reserve(nodeArgs.getChildrenCnt());
    optional<bool> variadic = getAttributeForBool(nodeArgs, "variadic");
    if (!variadic.has_value()) return NodeVal();
    for (size_t i = 0; i < nodeArgs.getChildrenCnt(); ++i) {
        const NodeVal &nodeArg = nodeArgs.getChild(i);

        NodeVal argTy = processAndCheckIsType(nodeArg);
        if (argTy.isInvalid()) return NodeVal();

        optional<bool> attrNoDrop = getAttributeForBool(argTy, "noDrop");
        if (!attrNoDrop.has_value()) return NodeVal();

        argTypes.push_back(argTy.getEvalVal().ty);
        argNoDrops.push_back(attrNoDrop.value());
    }

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

    // function type
    TypeTable::Id type;
    {
        TypeTable::Callable callable;
        callable.isFunc = true;
        callable.setArgCnt(argTypes.size());
        callable.setArgTypes(argTypes);
        callable.setArgNoDrops(argNoDrops);
        callable.retType = retType;
        callable.variadic = variadic.value();
        optional<TypeTable::Id> typeOpt = typeTable->addCallable(callable);
        if (!typeOpt.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }
        type = typeOpt.value();
    }

    return promoteType(node.getCodeLoc(), type);
}

NodeVal Processor::processMacType(const NodeVal &node) {
    // mac argNum
    size_t indArgNum = 1;

    // argument number
    NodeVal nodeArgNum = processNode(node.getChild(indArgNum));
    if (nodeArgNum.isInvalid()) return NodeVal();
    if (!nodeArgNum.isEvalVal()) {
        msgs->errorUnknown(nodeArgNum.getCodeLoc());
        return NodeVal();
    }
    optional<uint64_t> argNum = EvalVal::getValueNonNeg(nodeArgNum.getEvalVal(), typeTable);
    if (!argNum.has_value()) {
        msgs->errorUnknown(nodeArgNum.getCodeLoc());
        return NodeVal();
    }
    optional<bool> variadic = getAttributeForBool(nodeArgNum, "variadic");
    if (!variadic.has_value()) return NodeVal();

    // macro type
    TypeTable::Id type;
    {
        TypeTable::Callable callable;
        callable.isFunc = false;
        callable.setArgCnt(argNum.value());
        callable.variadic = variadic.value();
        optional<TypeTable::Id> typeOpt = typeTable->addCallable(callable);
        if (!typeOpt.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }
        type = typeOpt.value();
    }

    return promoteType(node.getCodeLoc(), type);
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
    } else if (op == Oper::SHR) {
        return moveNode(codeLoc, operProc);
    } else {
        if (checkIsEvalTime(operProc, false)) {
            return evaluator->performOperUnary(codeLoc, operProc, op);
        } else {
            return performOperUnary(codeLoc, operProc, op);
        }
    }
}

NodeVal Processor::processOperComparison(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op) {
    if (op == Oper::NE && opers.size() > 2) {
        msgs->errorUnknown(codeLoc);
        return NodeVal();
    }

    NodeVal lhs = processAndCheckHasType(*opers[0]);
    if (lhs.isInvalid()) return NodeVal();

    // redirecting to evaluator when all operands are EvalVals is more complicated in the case of comparisons
    // the reason is that LLVM's phi nodes need to be started up and closed appropriately
    ComparisonSignal signal;

    // do set up on the appropriate processor subclass, depending on whether first operand is EvalVal
    bool stillEval = checkIsEvalTime(lhs, false);
    if (stillEval) {
        signal = evaluator->performOperComparisonSetUp(codeLoc, opers.size());
    } else {
        signal = performOperComparisonSetUp(codeLoc, opers.size());
    }

    for (size_t i = 1; i < opers.size(); ++i) {
        // note that if this is within evaluator, and it throws ExceptionEvaluatorJump, teardown will not get called
        NodeVal rhs = processAndCheckHasType(*opers[i]);
        if (rhs.isInvalid()) {
            if (stillEval) return evaluator->performOperComparisonTearDown(codeLoc, false, signal);
            else return performOperComparisonTearDown(codeLoc, false, signal);
        }

        if (stillEval && !checkIsEvalTime(rhs, false)) {
            // no longer all EvalVals, so handoff from evaluator-> to this->
            evaluator->performOperComparisonTearDown(codeLoc, true, signal);
            stillEval = false;

            signal = performOperComparisonSetUp(codeLoc, opers.size()-i);
        }

        if (!implicitCastOperands(lhs, rhs, false)) {
            if (stillEval) return evaluator->performOperComparisonTearDown(codeLoc, false, signal);
            else return performOperComparisonTearDown(codeLoc, false, signal);
        }

        if (stillEval) {
            optional<bool> compSuccess = evaluator->performOperComparison(codeLoc, lhs, rhs, op, signal);
            if (!compSuccess.has_value()) return evaluator->performOperComparisonTearDown(codeLoc, false, signal);
            if (compSuccess.value()) break;
        } else {
            optional<bool> compSuccess = performOperComparison(codeLoc, lhs, rhs, op, signal);
            if (!compSuccess.has_value()) return performOperComparisonTearDown(codeLoc, false, signal);
            if (compSuccess.value()) break;
        }

        lhs = move(rhs);
    }

    if (stillEval) {
        return evaluator->performOperComparisonTearDown(codeLoc, true, signal);
    } else {
        return performOperComparisonTearDown(codeLoc, true, signal);
    }
}

// TODO if noDrop gets allowed, ensure lifetimes on ownership transfers are ok
NodeVal Processor::processOperAssignment(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    vector<NodeVal> procOpers;
    procOpers.reserve(opers.size());
    for (const NodeVal *oper : opers) {
        NodeVal procOper = processAndCheckHasType(*oper);
        if (procOper.isInvalid()) return NodeVal();

        procOpers.push_back(move(procOper));
    }

    NodeVal rhs = move(procOpers.back());
    procOpers.pop_back();

    while (!procOpers.empty()) {
        NodeVal lhs = move(procOpers.back());
        procOpers.pop_back();

        if (!lhs.hasRef()) {
            msgs->errorExprAsgnNonRef(lhs.getCodeLoc());
            return NodeVal();
        }
        if (typeTable->worksAsTypeCn(lhs.getType().value())) {
            msgs->errorExprAsgnOnCn(lhs.getCodeLoc());
            return NodeVal();
        }

        if (!implicitCastOperands(lhs, rhs, true)) return NodeVal();

        if (!checkTransferValueOk(codeLoc, rhs, lhs.isNoDrop(), true)) return NodeVal();

        rhs = dispatchAssignment(codeLoc, lhs, rhs);
        if (rhs.isInvalid()) return NodeVal();
    }

    return rhs;
}

NodeVal Processor::processOperIndex(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    // value kept so drop can be called
    NodeVal base = processAndCheckHasType(*opers[0]);
    if (base.isInvalid()) return NodeVal();

    NodeVal lhs = base;

    for (size_t i = 1; i < opers.size(); ++i) {
        if (!lhs.hasRef() && !checkNotNeedsDrop(lhs.getCodeLoc(), lhs, true)) return NodeVal();

        TypeTable::Id baseType = lhs.getType().value();

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

        lhs = getElement(lhs.getCodeLoc(), lhs, index);
        if (lhs.isInvalid()) return NodeVal();
    }

    return lhs;
}

NodeVal Processor::processOperDot(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    // value kept so drop can be called
    NodeVal base = processAndCheckHasType(*opers[0]);
    if (base.isInvalid()) return NodeVal();

    NodeVal lhs = base;

    for (size_t i = 1; i < opers.size(); ++i) {
        if (!lhs.hasRef() && !checkNotNeedsDrop(lhs.getCodeLoc(), lhs, true)) return NodeVal();

        TypeTable::Id baseType = lhs.getType().value();
        bool isBaseRaw = NodeVal::isRawVal(lhs, typeTable);
        bool isBaseTup = typeTable->worksAsTuple(baseType);
        bool isBaseData = typeTable->worksAsDataType(baseType);

        size_t baseLen = 0;
        if (isBaseRaw) {
            baseLen = lhs.getChildrenCnt();
        } else if (isBaseTup) {
            const TypeTable::Tuple *tuple = typeTable->extractTuple(baseType);
            if (tuple == nullptr) {
                msgs->errorExprDotInvalidBase(lhs.getCodeLoc());
                return NodeVal();
            }
            baseLen = tuple->members.size();
        } else if (isBaseData) {
            baseLen = typeTable->extractLenOfDataType(baseType).value();
        } else {
            msgs->errorUnknown(lhs.getCodeLoc());
            return NodeVal();
        }

        NodeVal index;
        uint64_t indexVal;
        if (isBaseData) {
            index = processWithEscape(*opers[i]);
            if (index.isInvalid()) return NodeVal();
            if (!index.isEvalVal()) {
                msgs->errorMemberIndex(index.getCodeLoc());
                return NodeVal();
            }

            if (EvalVal::isId(index.getEvalVal(), typeTable)) {
                NamePool::Id indexName = index.getEvalVal().id;
                optional<uint64_t> indexValOpt = typeTable->extractDataType(baseType)->getMembInd(indexName);
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
        } else {
            index = processNode(*opers[i]);
            if (index.isInvalid()) return NodeVal();
            if (!index.isEvalVal()) {
                msgs->errorMemberIndex(index.getCodeLoc());
                return NodeVal();
            }

            optional<uint64_t> indexValOpt = EvalVal::getValueNonNeg(index.getEvalVal(), typeTable);
            if (!indexValOpt.has_value() || indexValOpt.value() >= baseLen) {
                msgs->errorMemberIndex(index.getCodeLoc());
                return NodeVal();
            }
            indexVal = indexValOpt.value();
        }

        if (isBaseRaw) {
            lhs = getRawMember(index.getCodeLoc(), lhs, (size_t) indexVal);
        } else if (isBaseTup) {
            lhs = getTupleMember(index.getCodeLoc(), lhs, (size_t) indexVal);
        } else {
            lhs = getDataMember(index.getCodeLoc(), lhs, (size_t) indexVal);
        }

        if (lhs.isInvalid()) return NodeVal();
    }

    return lhs;
}

NodeVal Processor::processOperRegular(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op, bool bare) {
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

        if (op == Oper::DIV && rhs.isEvalVal() && EvalVal::isZero(rhs.getEvalVal(), typeTable)) {
            msgs->errorUnknown(rhs.getCodeLoc());
            return NodeVal();
        }

        NodeVal nextLhs;
        if (checkIsEvalTime(lhs, false) && checkIsEvalTime(rhs, false)) {
            nextLhs = evaluator->performOperRegular(codeLoc, lhs, rhs, op, bare);
        } else {
            nextLhs = performOperRegular(codeLoc, lhs, rhs, op, bare);
        }
        if (nextLhs.isInvalid()) return NodeVal();

        lhs = move(nextLhs);
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

NodeVal Processor::processWithEscape(const NodeVal &node, EscapeScore amount) {
    if (amount != 0) {
        NodeVal esc = node;
        NodeVal::escape(esc, typeTable, amount);
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
    pair<NodeVal, optional<NodeVal>> retInvalid = make_pair<NodeVal, optional<NodeVal>>(NodeVal(), nullopt);

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

bool Processor::checkInGlobalScope(CodeLoc codeLoc, bool orError) {
    if (!symbolTable->inGlobalScope()) {
        if (orError) msgs->errorNotGlobalScope(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkInLocalScope(CodeLoc codeLoc, bool orError) {
    if (symbolTable->inGlobalScope()) {
        if (orError) msgs->errorNotLocalScope(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkHasType(const NodeVal &node, bool orError) {
    if (!node.getType().has_value()) {
        if (orError) msgs->errorMissingType(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsEvalTime(CodeLoc codeLoc, const NodeVal &node, bool orError) {
    return checkIsEvalVal(codeLoc, node, true);
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
        if (orError) msgs->errorNotEvalVal(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsEvalFunc(CodeLoc codeLoc, const FuncValue &func, bool orError) {
    if (!func.isEval()) {
        if (orError) msgs->errorNotEvalFunc(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsLlvmVal(CodeLoc codeLoc, const NodeVal &node, bool orError) {
    if (!node.isLlvmVal()) {
        if (orError) msgs->errorNotCompiledVal(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsLlvmFunc(CodeLoc codeLoc, const FuncValue &func, bool orError) {
    if (!func.isLlvm()) {
        if (orError) msgs->errorNotCompiledFunc(codeLoc);
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
        if (orError) msgs->errorUnexpectedNotBool(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkTransferValueOk(CodeLoc codeLoc, const NodeVal &src, bool dstNoDrop, bool orError) {
    if (!hasTrivialDrop(src.getType().value()) && !dstNoDrop) {
        if (src.hasRef() || src.isInvokeArg()) {
            if (orError) msgs->errorBadTransfer(codeLoc);
            return false;
        } else if (src.isNoDrop()) {
            if (orError) msgs->errorTransferNoDrop(codeLoc);
            return false;
        }
    }

    return true;
}

bool Processor::checkIsDropFuncType(const NodeVal &node, TypeTable::Id dropeeTy, bool orError) {
    bool check = node.getType().has_value();
    if (check) {
        TypeTable::Id nodeTy = node.getType().value();
        check = typeTable->worksAsCallable(nodeTy, true);
        if (check) {
            const TypeTable::Callable *givenCallable = typeTable->extractCallable(nodeTy);

            TypeTable::Callable wantedCallable;
            wantedCallable.isFunc = true;
            wantedCallable.setArgCnt(1);
            wantedCallable.setArgTypes({dropeeTy});

            check = typeTable->addCallableSig(wantedCallable) == typeTable->addCallableSig(*givenCallable);

            if (check) check = givenCallable->getArgNoDrop(0);
        }
    }

    if (!check) {
        if (orError) msgs->errorDropFuncBadSig(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsValue(const NodeVal &node, bool orError) {
    if (!node.isEvalVal() && !node.isLlvmVal()) {
        if (orError) msgs->errorMissingType(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkExactlyChildren(const NodeVal &node, size_t n, bool orError) {
    if (!checkIsRaw(node, orError)) return false;

    if (node.getChildrenCnt() != n) {
        if (orError) msgs->errorChildrenNotEq(node.getCodeLoc(), n, node.getChildrenCnt());
        return false;
    }
    return true;
}

bool Processor::checkAtLeastChildren(const NodeVal &node, size_t n, bool orError) {
    if (!checkIsRaw(node, orError)) return false;

    if (node.getChildrenCnt() < n) {
        if (orError) msgs->errorChildrenLessThan(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkAtMostChildren(const NodeVal &node, size_t n, bool orError) {
    if (!checkIsRaw(node, orError)) return false;

    if (node.getChildrenCnt() > n) {
        if (orError) msgs->errorChildrenMoreThan(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkBetweenChildren(const NodeVal &node, size_t nLo, size_t nHi, bool orError) {
    if (!checkIsRaw(node, orError)) return false;

    if (!between(node.getChildrenCnt(), nLo, nHi)) {
        if (orError) msgs->errorChildrenNotBetween(node.getCodeLoc(), nLo, nHi, node.getChildrenCnt());
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