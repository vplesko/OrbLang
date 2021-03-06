#include "Processor.h"
#include "BlockRaii.h"
#include "Evaluator.h"
#include "reserved.h"
using namespace std;

Processor::Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs)
    : namePool(namePool), stringPool(stringPool), typeTable(typeTable), symbolTable(symbolTable), msgs(msgs), compiler(nullptr), evaluator(nullptr) {
}

NodeVal Processor::processNode(const NodeVal &node, bool topmost) {
    NodeVal ret;
    if (node.hasTypeAttr() || node.hasNonTypeAttrs()) {
        bool nonIdLiteral = node.isLiteralVal() && node.getLiteralVal().kind != LiteralVal::Kind::kId;

        NodeVal procAttrs = node;
        if (!processAttributes(procAttrs, nonIdLiteral)) return NodeVal();

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
        if (!callDropFuncTmpVal(ret)) return NodeVal();
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
            case Keyword::EXPLICIT:
                return processExplicit(node, starting);
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
            case Keyword::IS_EVAL:
                return processIsEval(node);
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

    for (const auto &it : node.getEvalVal().elems()) {
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
        return NodeVal::copyNoRef(node.getCodeLoc(), starting, LifetimeInfo());

    NodeVal second = processForTypeArg(node.getChild(1));
    if (second.isInvalid()) return NodeVal();

    EvalVal evalTy = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);

    if (canBeTypeDescrDecor(second)) {
        TypeTable::TypeDescr descr(starting.getEvalVal().ty());
        if (!applyTypeDescrDecor(descr, second)) return NodeVal();
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal decor = processForTypeArg(node.getChild(i));
            if (decor.isInvalid()) return NodeVal();
            if (!applyTypeDescrDecor(descr, decor)) return NodeVal();
        }

        evalTy.ty() = typeTable->addTypeDescr(move(descr));
    } else {
        TypeTable::Tuple tup;
        tup.elements.reserve(node.getChildrenCnt());
        if (!applyTupleElem(tup, starting)) return NodeVal();
        if (!applyTupleElem(tup, second)) return NodeVal();
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal ty = processNode(node.getChild(i));
            if (ty.isInvalid()) return NodeVal();
            if (!applyTupleElem(tup, ty)) return NodeVal();
        }

        optional<TypeTable::Id> tupTypeId = typeTable->addTuple(move(tup));
        if (!tupTypeId.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        evalTy.ty() = tupTypeId.value();
    }

    return NodeVal(node.getCodeLoc(), move(evalTy));
}

NodeVal Processor::processId(const NodeVal &node) {
    NamePool::Id id = node.getEvalVal().id();

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
        eval.ty() = typeTable->getTypeId(id).value();

        return NodeVal(node.getCodeLoc(), move(eval));
    } else {
        msgs->errorSymbolNotFound(node.getCodeLoc(), id);
        return NodeVal();
    }
}

NodeVal Processor::processId(const NodeVal &node, const NodeVal &starting) {
    if (!checkExactlyChildren(node, 1, true)) return NodeVal();

    NodeVal ret = processId(starting);
    ret.setCodeLoc(node.getCodeLoc());
    return move(ret);
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
        NamePool::Id id = pair.first.getEvalVal().id();
        if (!symbolTable->nameAvailable(id, namePool, typeTable)) {
            msgs->errorNameTaken(nodePair.getCodeLoc(), id);
            return NodeVal();
        }
        optional<TypeTable::Id> optType;
        if (pair.second.has_value()) {
            optType = pair.second.value().getEvalVal().ty();
            if (!checkIsNotUndefType(pair.second.value().getCodeLoc(), optType.value(), true)) return NodeVal();
        }
        optional<bool> attrEvaluated = getAttributeForBool(pair.first, "evaluated");
        if (!attrEvaluated.has_value()) return NodeVal();

        bool hasType = optType.has_value();

        TypeTable::Id varType;
        SymbolTable::VarEntry varEntry;
        varEntry.name = id;

        if (hasInit) {
            // TODO if noDrop on sym gets allowed, ensure all ownership transfers are ok
            const NodeVal &nodeInit = entry.getChild(1);
            NodeVal init = hasType ? processAndImplicitCast(nodeInit, optType.value()) : processNode(nodeInit);
            if (init.isInvalid()) return NodeVal();
            if (!checkHasType(init, true)) return NodeVal();

            if (!checkTransferValueOk(init.getCodeLoc(), init, false, false, true)) return NodeVal();

            varType = init.getType().value();

            NodeVal reg;
            if (attrEvaluated.value()) reg = evaluator->performRegister(pair.first.getCodeLoc(), id, move(init));
            else reg = performRegister(pair.first.getCodeLoc(), id, move(init));
            if (reg.isInvalid()) return NodeVal();

            varEntry.var = move(reg);
        } else {
            if (!hasType) {
                msgs->errorMissingTypeAttribute(nodePair.getCodeLoc());
                return NodeVal();
            }
            if (typeTable->worksAsTypeCn(optType.value())) {
                msgs->errorSymCnNoInit(entry.getCodeLoc(), id);
                return NodeVal();
            }

            varType = optType.value();

            optional<bool> attrNoZero = getAttributeForBool(pair.first, "noZero");
            if (!attrNoZero.has_value()) return NodeVal();

            NodeVal nodeReg;
            if (attrNoZero.value()) {
                if (attrEvaluated.value()) nodeReg = evaluator->performRegister(pair.first.getCodeLoc(), id, pair.second.value().getCodeLoc(), optType.value());
                else nodeReg = performRegister(pair.first.getCodeLoc(), id, pair.second.value().getCodeLoc(), optType.value());
                if (nodeReg.isInvalid()) return NodeVal();
            } else {
                NodeVal nodeZero;
                if (attrEvaluated.value()) nodeZero = evaluator->performZero(pair.second.value().getCodeLoc(), optType.value());
                else nodeZero = performZero(pair.second.value().getCodeLoc(), optType.value());
                if (nodeZero.isInvalid()) return NodeVal();

                if (attrEvaluated.value()) nodeReg = evaluator->performRegister(pair.first.getCodeLoc(), id, move(nodeZero));
                else nodeReg = performRegister(pair.first.getCodeLoc(), id, move(nodeZero));
                if (nodeReg.isInvalid()) return NodeVal();
            }

            varEntry.var = move(nodeReg);
        }

        if (checkInGlobalScope(pair.first.getCodeLoc(), false) && !hasTrivialDrop(varType)) {
            msgs->errorSymGlobalOwning(pair.first.getCodeLoc(), id, varType);
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
    if (!checkIsNotUndefType(ty.getCodeLoc(), ty.getEvalVal().ty(), true)) return NodeVal();

    NodeVal value = processNode(node.getChild(2));
    if (value.isInvalid()) return NodeVal();

    NodeVal ret = castNode(node.getCodeLoc(), value, ty.getCodeLoc(), ty.getEvalVal().ty());
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

    size_t indName = 1;
    size_t indType = hasName ? 2 : 1;
    size_t indBody = hasName ? 3 : (hasType ? 2 : 1);

    const NodeVal &nodeBody = node.getChild(indBody);
    if (!checkIsRaw(nodeBody, true)) return NodeVal();

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processForIdOrEmpty(node.getChild(indName));
        if (nodeName.isInvalid()) {
            msgs->hintBlockSyntax();
            return NodeVal();
        }
        if (!checkIsEmpty(nodeName, false)) {
            name = nodeName.getEvalVal().id();
            if (attrBare.value()) {
                msgs->errorBlockBareNameType(nodeName.getCodeLoc());
                return NodeVal();
            }
            if (typeTable->isType(name.value())) {
                msgs->warnBlockNameIsType(nodeName.getCodeLoc(), name.value());
            }
        }
    }

    optional<TypeTable::Id> type;
    if (hasType) {
        NodeVal nodeType = processNode(node.getChild(indType));
        if (nodeType.isInvalid()) {
            msgs->hintBlockSyntax();
            return NodeVal();
        }
        if (!checkIsEmpty(nodeType, false)) {
            if (!checkIsType(nodeType, true)) {
                msgs->hintBlockSyntax();
                return NodeVal();
            }

            type = nodeType.getEvalVal().ty();
            if (!checkIsNotUndefType(nodeType.getCodeLoc(), type.value(), true)) return NodeVal();
            if (attrBare.value()) {
                msgs->errorBlockBareNameType(nodeType.getCodeLoc());
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
            BlockRaii blockRaii(symbolTable, block);

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
        NodeVal nodeName = processForIdValue(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        name = nodeName.getEvalVal().id();
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
            msgs->errorExitNowhere(node.getCodeLoc());
            return NodeVal();
        }
    }
    if (targetBlock.type.has_value()) {
        msgs->errorExitPassingBlock(node.getCodeLoc());
        return NodeVal();
    }

    NodeVal nodeCond = processNode(node.getChild(indCond));
    if (nodeCond.isInvalid()) return NodeVal();
    if (!checkIsBool(nodeCond, true)) return NodeVal();

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
        NodeVal nodeName = processForIdValue(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        name = nodeName.getEvalVal().id();
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
            msgs->errorLoopNowhere(node.getCodeLoc());
            return NodeVal();
        }
    }

    NodeVal nodeCond = processNode(node.getChild(indCond));
    if (nodeCond.isInvalid()) return NodeVal();
    if (!checkIsBool(nodeCond, true)) return NodeVal();

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
        NodeVal nodeName = processForIdValue(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        name = nodeName.getEvalVal().id();
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

    NodeVal moved = processForScopeResult(node.getChild(indVal), false);
    if (moved.isInvalid()) return NodeVal();

    NodeVal casted = implicitCast(moved, targetBlock.type.value());
    if (casted.isInvalid()) return NodeVal();

    if (!checkTransferValueOk(casted.getCodeLoc(), casted, false, false, true)) return NodeVal();

    if (!performPass(node.getCodeLoc(), targetBlock, move(casted))) return NodeVal();
    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processExplicit(const NodeVal &node, const NodeVal &starting) {
    if (!checkExactlyChildren(node, 3, true)) return NodeVal();

    optional<bool> attrGlobal = getAttributeForBool(starting, "global");
    if (!attrGlobal.has_value()) return NodeVal();

    if (!attrGlobal.value() && !checkInGlobalScope(starting.getCodeLoc(), true)) {
        msgs->hintAttrGlobal();
        return NodeVal();
    }

    NodeVal nodeName = processForIdValue(node.getChild(1));
    if (nodeName.isInvalid()) return NodeVal();
    NamePool::Id name = nodeName.getEvalVal().id();
    if (!symbolTable->nameAvailable(name, namePool, typeTable, true, true)) {
        msgs->errorNameTaken(nodeName.getCodeLoc(), name);
        return NodeVal();
    }

    NodeVal nodeTy = processNode(node.getChild(2));
    if (nodeTy.isInvalid()) return NodeVal();
    if (!checkIsType(nodeTy, true)) return NodeVal();
    TypeTable::Id ty = nodeTy.getEvalVal().ty();

    TypeTable::ExplicitType explicit_;
    explicit_.name = name;
    explicit_.type = ty;
    optional<TypeTable::Id> typeId = typeTable->addExplicitType(explicit_);
    if (!typeId.has_value()) {
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    return promoteType(node.getCodeLoc(), typeId.value());
}

// TODO noDrop on elements
NodeVal Processor::processData(const NodeVal &node, const NodeVal &starting) {
    if (!checkBetweenChildren(node, 2, 4, true)) return NodeVal();

    optional<bool> attrGlobal = getAttributeForBool(starting, "global");
    if (!attrGlobal.has_value()) return NodeVal();

    if (!attrGlobal.value() && !checkInGlobalScope(starting.getCodeLoc(), true)) {
        msgs->hintAttrGlobal();
        return NodeVal();
    }

    bool definition = node.getChildrenCnt() >= 3;
    bool withDrop = node.getChildrenCnt() >= 4;

    size_t indName = 1;
    size_t indElems = definition ? 2 : 0;
    size_t indDrop = withDrop ? 3 : 0;

    TypeTable::DataType dataType;
    dataType.defined = false;

    NodeVal nodeName = processForIdValue(node.getChild(indName));
    if (nodeName.isInvalid()) return NodeVal();
    dataType.name = nodeName.getEvalVal().id();
    if (!symbolTable->nameAvailable(dataType.name, namePool, typeTable, true, true)) {
        optional<TypeTable::Id> oldTy = typeTable->getTypeId(dataType.name);
        if (!oldTy.has_value() || !typeTable->isDataType(oldTy.value())) {
            msgs->errorNameTaken(nodeName.getCodeLoc(), dataType.name);
            return NodeVal();
        } else if (definition && typeTable->getDataType(oldTy.value()).defined) {
            msgs->errorDataRedefinition(nodeName.getCodeLoc(), dataType.name);
            return NodeVal();
        }
    }

    optional<TypeTable::Id> typeIdOpt = typeTable->addDataType(dataType);
    if (!typeIdOpt.has_value()) {
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    dataType.defined = definition;
    if (dataType.defined) {
        NodeVal nodeElems = processWithEscape(node.getChild(indElems));
        if (nodeElems.isInvalid()) return NodeVal();
        if (!checkIsRaw(nodeElems, true)) return NodeVal();
        if (!checkAtLeastChildren(nodeElems, 1, true)) return NodeVal();

        for (size_t i = 0; i < nodeElems.getChildrenCnt(); ++i) {
            const NodeVal &nodeElem = nodeElems.getChild(i);

            pair<NodeVal, optional<NodeVal>> elem = processForIdTypePair(nodeElem);
            if (elem.first.isInvalid()) return NodeVal();

            NamePool::Id elemName = elem.first.getEvalVal().id();

            if (!elem.second.has_value()) {
                msgs->errorMissingTypeAttribute(nodeElem.getCodeLoc());
                return NodeVal();
            }

            TypeTable::Id elemType = elem.second.value().getEvalVal().ty();
            if (typeTable->worksAsTypeCn(elemType)) {
                msgs->errorDataCnElement(elem.second.value().getCodeLoc());
                return NodeVal();
            }
            if (!checkIsNotUndefType(elem.second.value().getCodeLoc(), elemType, true)) return NodeVal();

            optional<bool> attrNoZero = getAttributeForBool(elem.first, "noZero");
            if (!attrNoZero.has_value()) return NodeVal();

            TypeTable::DataType::ElemEntry elemEntry;
            elemEntry.name = elemName;
            elemEntry.type = elemType;
            elemEntry.noZeroInit = attrNoZero.value();

            dataType.elements.push_back(elemEntry);
        }

        typeIdOpt = typeTable->addDataType(dataType);
        if (!typeIdOpt.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        if (nodeElems.hasNonTypeAttrs() && !nodeElems.getNonTypeAttrs().getAttrMap().attrMap.empty()) {
            symbolTable->registerDataAttrs(typeIdOpt.value(), move(nodeElems.getNonTypeAttrs().getAttrMap()));
        }

        if (withDrop) {
            NodeVal nodeDrop = processNode(node.getChild(indDrop));
            if (nodeDrop.isInvalid()) return NodeVal();
            if (!checkIsEmpty(nodeDrop, false)) {
                if (!checkIsDropFuncType(nodeDrop, typeIdOpt.value(), true)) return NodeVal();
                symbolTable->registerDropFunc(typeIdOpt.value(), move(nodeDrop));
            }
        }

        if (!performDataDefinition(node.getCodeLoc(), typeIdOpt.value())) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
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
    vector<BlockTmpValRaii> argTmpRaii;
    argTmpRaii.reserve(providedArgCnt);
    bool allArgsEval = true;
    for (size_t i = 0; i < providedArgCnt; ++i) {
        NodeVal arg = processNode(node.getChild(i+1));
        if (arg.isInvalid()) return NodeVal();
        if (!checkHasType(arg, true)) return NodeVal();

        if (!checkIsEvalVal(arg, false)) allArgsEval = false;

        args.push_back(move(arg));
        argTypes.push_back(args.back().getType().value());
        argTmpRaii.push_back(createTmpValRaii(args.back()));
    }
    argTmpRaii.clear(); // no longer possible to break out of arg processing

    NodeVal ret;

    TypeTable::Callable callable;

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
            bool ambiguous = false;
            for (FuncId it : funcIds) {
                if (argsFitFuncCall(args, BaseCallableValue::getCallableSig(symbolTable->getFunc(it), typeTable), true)) {
                    if (funcId.has_value()) {
                        // error due to call ambiguity
                        ambiguous = true;
                        break;
                    }
                    funcId = it;
                }
            }
            if (ambiguous) {
                vector<CodeLoc> codeLocsCand;
                for (FuncId it : funcIds) {
                    if (argsFitFuncCall(args, BaseCallableValue::getCallableSig(symbolTable->getFunc(it), typeTable), true))
                        codeLocsCand.push_back(symbolTable->getFunc(it).codeLoc);
                }
                msgs->errorFuncCallAmbiguous(starting.getCodeLoc(), move(codeLocsCand));
                return NodeVal();
            }
        }

        if (!funcId.has_value()) {
            msgs->errorFuncNotFound(starting.getCodeLoc(), move(argTypes), starting.getUndecidedCallableVal().name);
            return NodeVal();
        }

        callable = BaseCallableValue::getCallable(symbolTable->getFunc(funcId.value()), typeTable);

        if (!implicitCastArgsAndVerifyCallOk(node.getCodeLoc(), args, callable)) return NodeVal();

        ret = dispatchCall(node.getCodeLoc(), starting.getCodeLoc(), funcId.value(), args, allArgsEval);
    } else {
        if (!starting.getType().has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        if (!typeTable->worksAsCallable(starting.getType().value())) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }
        callable = *typeTable->extractCallable(starting.getType().value());

        if (!argsFitFuncCall(args, callable, true)) {
            msgs->errorFuncNotFound(starting.getCodeLoc(), move(argTypes));
            return NodeVal();
        }

        if (!implicitCastArgsAndVerifyCallOk(node.getCodeLoc(), args, callable)) return NodeVal();

        ret = dispatchCall(node.getCodeLoc(), starting.getCodeLoc(), starting, args, allArgsEval);
    }

    if (ret.isInvalid()) return NodeVal();

    for (size_t i = 0; i < args.size(); ++i) {
        size_t ind = args.size()-1-i;

        // variadic arguments cannot be marked noDrop
        bool argNoDrop = ind < callable.getArgCnt() && callable.getArgNoDrop(ind);

        // if argument was marked noDrop, it becomes the callers responsibility to drop it
        if (argNoDrop && !callDropFuncTmpVal(move(args[ind]))) return NodeVal();
    }

    return ret;
}

// TODO passing attributes on macro itself (just like can be done for special forms, eg. fnc::global)
NodeVal Processor::processInvoke(const NodeVal &node, const NodeVal &starting) {
    optional<MacroId> macroId;
    if (starting.isUndecidedCallableVal()) {
        SymbolTable::InvokeSite callSite;
        callSite.name = starting.getUndecidedCallableVal().name;
        callSite.argCnt = node.getChildrenCnt()-1;

        macroId = symbolTable->getMacroId(callSite, typeTable);
        if (!macroId.has_value()) {
            msgs->errorMacroNotFound(node.getCodeLoc(), starting.getUndecidedCallableVal().name);
            return NodeVal();
        }
    } else {
        if (!starting.getEvalVal().m().has_value()) {
            msgs->errorMacroNoValue(starting.getCodeLoc());
            return NodeVal();
        }
        macroId = starting.getEvalVal().m();
    }

    const MacroValue &macroVal = symbolTable->getMacro(macroId.value());

    TypeTable::Callable callable = BaseCallableValue::getCallable(macroVal, typeTable);

    size_t providedArgCnt = node.getChildrenCnt()-1;
    if ((callable.variadic && providedArgCnt+1 < callable.getArgCnt()) ||
        (!callable.variadic && providedArgCnt != callable.getArgCnt())) {
        msgs->errorMacroNotFound(node.getCodeLoc(), macroVal.name);
        return NodeVal();
    }

    vector<NodeVal> args;
    args.reserve(providedArgCnt);
    vector<BlockTmpValRaii> argTmpRaii;
    argTmpRaii.reserve(providedArgCnt);
    for (size_t i = 0; i < providedArgCnt; ++i) {
        // all variadic arguments have the same pre-handling
        EscapeScore escapeScore = MacroValue::toEscapeScore(macroVal.argPreHandling[min(i, callable.getArgCnt()-1)]);

        NodeVal arg = processWithEscape(node.getChild(i+1), escapeScore);
        if (arg.isInvalid()) return NodeVal();

        args.push_back(move(arg));
        argTmpRaii.push_back(createTmpValRaii(args.back()));
    }
    argTmpRaii.clear(); // no longer possible to break out of arg processing

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

        if (!attrGlobal.value() && !checkInGlobalScope(starting.getCodeLoc(), true)) {
            msgs->hintAttrGlobal();
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
        NodeVal nodeName = processForIdValue(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        nameCodeLoc = nodeName.getCodeLoc();
        name = nodeName.getEvalVal().id();
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
            CodeLoc codeLoc = (nodeName.hasNonTypeAttrs() ? nodeName.getNonTypeAttrs() : nodeName).getCodeLoc();
            msgs->errorFuncNotEvalOrCompiled(codeLoc);
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

        NamePool::Id argId = arg.first.getEvalVal().id();

        if (!arg.second.has_value()) {
            msgs->errorMissingTypeAttribute(nodeArg.getCodeLoc());
            return NodeVal();
        }

        TypeTable::Id argTy = arg.second.value().getEvalVal().ty();
        if (isDef && !checkIsNotUndefType(arg.second.value().getCodeLoc(), argTy, true)) return NodeVal();

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
        if (ty.isInvalid()) {
            msgs->hintWhileProcessingRetType(nodeRetType.getCodeLoc());
            return NodeVal();
        }

        if (!checkIsEmpty(ty, false)) {
            if (!checkIsType(ty, true)) return NodeVal();

            retType = ty.getEvalVal().ty();
            if (isDef && !checkIsNotUndefType(nodeRetType.getCodeLoc(), retType.value(), true)) return NodeVal();
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
        type = typeTable->addCallable(callable);
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

    SymbolTable::RegisterCallablePayload symbId = symbolTable->registerFunc(move(funcVal));
    if (symbId.kind != SymbolTable::RegisterCallablePayload::Kind::kSuccess) {
        switch (symbId.kind) {
        case SymbolTable::RegisterCallablePayload::Kind::kOtherCallableTypeSameName:
            msgs->errorFuncNameTaken(nameCodeLoc, name);
            break;
        case SymbolTable::RegisterCallablePayload::Kind::kNoNameMangleCollision:
            msgs->errorFuncCollisionNoNameMangle(nameCodeLoc, name, symbId.codeLocOther);
            break;
        case SymbolTable::RegisterCallablePayload::Kind::kCollision:
            msgs->errorFuncCollision(nameCodeLoc, name, symbId.codeLocOther);
            break;
        default:
            msgs->errorInternal(nameCodeLoc);
            break;
        }
        return NodeVal();
    }
    FuncValue &symbVal = symbolTable->getFunc(symbId.funcId);

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
        return evaluator->performLoad(node.getCodeLoc(), symbId.funcId);
    } else {
        return compiler->performLoad(node.getCodeLoc(), symbId.funcId);
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

        if (!attrGlobal.value() && !checkInGlobalScope(starting.getCodeLoc(), true)) {
            msgs->hintAttrGlobal();
            return NodeVal();
        }
    }

    // name
    CodeLoc nameCodeLoc;
    NamePool::Id name;
    if (isDef) {
        NodeVal nodeName = processForIdValue(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        nameCodeLoc = nodeName.getCodeLoc();
        name = nodeName.getEvalVal().id();
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
            msgs->errorMacroArgAfterVariadic(nodeArg.getCodeLoc());
            return NodeVal();
        }

        NodeVal arg = processForIdValue(nodeArg);
        if (arg.isInvalid()) return NodeVal();
        if (arg.hasTypeAttr()) msgs->warnMacroArgTyped(nodeArg.getCodeLoc());

        NamePool::Id argId = arg.getEvalVal().id();
        argNames.push_back(argId);

        optional<bool> isPreproc = getAttributeForBool(arg, "preprocess");
        if (!isPreproc.has_value()) return NodeVal();
        optional<bool> isPlusEsc = getAttributeForBool(arg, "plusEscape");
        if (!isPlusEsc.has_value()) return NodeVal();
        if (isPreproc.value() && isPlusEsc.value()) {
            msgs->errorMacroArgPreprocessAndPlusEscape(arg.getNonTypeAttrs().getCodeLoc());
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

        SymbolTable::RegisterCallablePayload symbId = symbolTable->registerMacro(move(macroVal), typeTable);
        if (symbId.kind != SymbolTable::RegisterCallablePayload::Kind::kSuccess) {
            switch (symbId.kind) {
            case SymbolTable::RegisterCallablePayload::Kind::kOtherCallableTypeSameName:
                msgs->errorMacroNameTaken(nameCodeLoc, name);
                break;
            case SymbolTable::RegisterCallablePayload::Kind::kCollision:
                msgs->errorMacroCollision(nameCodeLoc, name, symbId.codeLocOther);
                break;
            case SymbolTable::RegisterCallablePayload::Kind::kVariadicCollision:
                msgs->errorMacroCollisionVariadic(nameCodeLoc, name, symbId.codeLocOther);
                break;
            default:
                msgs->errorInternal(nameCodeLoc);
                break;
            }
            return NodeVal();
        }
        MacroValue &symbVal = symbolTable->getMacro(symbId.macroId);

        if (!evaluator->performMacroDefinition(node.getCodeLoc(), nodeArgs, *nodeBodyPtr, symbVal)) return NodeVal();

        return evaluator->performLoad(node.getCodeLoc(), symbId.macroId);
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
        msgs->errorRetNowhere(node.getCodeLoc());
        return NodeVal();
    }

    if (optCallee.value().isFunc) {
        bool retsVal = node.getChildrenCnt() == 2;
        if (retsVal) {
            if (!optCallee.value().retType.has_value()) {
                msgs->errorRetValue(node.getCodeLoc());
                return NodeVal();
            }

            NodeVal moved = processForScopeResult(node.getChild(1), true);
            if (moved.isInvalid()) return NodeVal();

            NodeVal casted = implicitCast(moved, optCallee.value().retType.value());
            if (casted.isInvalid()) return NodeVal();

            if (!checkTransferValueOk(casted.getCodeLoc(), casted, false, false, true)) return NodeVal();

            if (!performRet(node.getCodeLoc(), move(casted))) return NodeVal();
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

        NodeVal moved = processForScopeResult(node.getChild(1), true);
        if (moved.isInvalid()) return NodeVal();

        if (!performRet(node.getCodeLoc(), move(moved))) return NodeVal();

        return NodeVal(node.getCodeLoc());
    }
}

NodeVal Processor::processEval(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) {
        return NodeVal();
    }

    return evaluator->processNode(node.getChild(1));
}

// TODO allow asking if invocation was from eval code
NodeVal Processor::processIsEval(const NodeVal &node) {
    if (!checkBetweenChildren(node, 1, 2, true)) {
        return NodeVal();
    }

    if (checkExactlyChildren(node, 2, false)) {
        NodeVal operand = processAndCheckHasType(node.getChild(1));
        if (operand.isInvalid()) return NodeVal();

        bool wasEval = operand.isEvalVal();

        if (!callDropFuncTmpVal(move(operand))) return NodeVal();

        return promoteBool(node.getCodeLoc(), operand.isEvalVal());
    } else {
        return promoteBool(node.getCodeLoc(), this == evaluator);
    }
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

    return NodeVal(node.getCodeLoc(), file.getEvalVal().str().value());
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
        msgs->errorMessageMultiLevel(starting.getNonTypeAttrs().getCodeLoc());
        return NodeVal();
    }

    CompilationMessages::Status status = CompilationMessages::S_INFO;
    if (attrWarning.value()) status = CompilationMessages::S_WARNING;
    else if (attrError.value()) status = CompilationMessages::S_ERROR;

    vector<NodeVal> opers;
    opers.reserve(node.getChildrenCnt()-1);

    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        NodeVal nodeVal = processNode(node.getChild(i));
        if (nodeVal.isInvalid()) return NodeVal();

        opers.push_back(move(nodeVal));
    }

    CodeLoc codeLoc = node.getCodeLoc();
    size_t indPrintStart = 0;

    optional<NodeVal> attrLoc = getAttribute(opers.front(), "loc");
    if (attrLoc.has_value()) {
        if (!checkAtLeastChildren(node, 3, true)) return NodeVal();

        codeLoc = opers.front().getCodeLoc();
        indPrintStart = 1;
    }

    if (!msgs->userMessageStart(codeLoc, status)) {
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    for (size_t i = indPrintStart; i < opers.size(); ++i) {
        if (!checkIsEvalVal(opers[i], true)) return NodeVal();

        const EvalVal &evalVal = opers[i].getEvalVal();

        if (EvalVal::isI(evalVal, typeTable)) {
            msgs->userMessage(EvalVal::getValueI(evalVal, typeTable).value());
        } else if (EvalVal::isU(evalVal, typeTable)) {
            msgs->userMessage(EvalVal::getValueU(evalVal, typeTable).value());
        } else if (EvalVal::isF(evalVal, typeTable)) {
            msgs->userMessage(EvalVal::getValueF(evalVal, typeTable).value());
        } else if (EvalVal::isC(evalVal, typeTable)) {
            msgs->userMessage(evalVal.c8());
        } else if (EvalVal::isB(evalVal, typeTable)) {
            msgs->userMessage(evalVal.b());
        } else if (EvalVal::isId(evalVal, typeTable)) {
            msgs->userMessage(evalVal.id());
        } else if (EvalVal::isType(evalVal, typeTable)) {
            msgs->userMessage(evalVal.ty());
        } else if (EvalVal::isNull(evalVal, typeTable)) {
            msgs->userMessageNull();
        } else if (EvalVal::isNonNullStr(evalVal, typeTable)) {
            msgs->userMessage(evalVal.str().value());
        } else {
            msgs->userMessageEnd();
            msgs->errorMessageBadType(opers[i].getCodeLoc(), evalVal.getType());
            return NodeVal();
        }
    }

    msgs->userMessageEnd();

    if (attrLoc.has_value()) {
        msgs->displayCodeSegment(codeLoc);

        // none of printable types can have a drop function
        // loc may be any value
        if (!callDropFuncTmpVal(move(opers.front()))) return NodeVal();
    }

    if (attrError.value()) return NodeVal();
    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processOper(const NodeVal &node, const NodeVal &starting, Oper op) {
    if (!checkAtLeastChildren(node, 2, true)) return NodeVal();

    if (checkExactlyChildren(node, 2, false)) {
        return processOperUnary(node.getCodeLoc(), starting, node.getChild(1), op);
    }

    OperInfo operInfo = operInfos.find(op)->second;

    vector<const NodeVal*> operands;
    operands.reserve(node.getChildrenCnt()-1);
    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        operands.push_back(&node.getChild(i));
    }

    if (operInfo.comparison) {
        return processOperComparison(starting.getCodeLoc(), operands, op);
    } else if (op == Oper::ASGN) {
        return processOperAssignment(starting.getCodeLoc(), operands);
    } else if (op == Oper::IND) {
        return processOperIndex(node.getCodeLoc(), operands);
    } else {
        return processOperRegular(starting.getCodeLoc(), starting, operands, op);
    }
}

NodeVal Processor::processTypeOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal operand = processAndCheckHasType(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();

    TypeTable::Id ty = operand.getType().value();

    if (!callDropFuncTmpVal(move(operand))) return NodeVal();

    return promoteType(node.getCodeLoc(), ty);
}

NodeVal Processor::processLenOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();
    if (!checkHasType(operand, true)) return NodeVal();

    TypeTable::Id ty;
    if (typeTable->worksAsPrimitive(operand.getType().value(), TypeTable::P_TYPE)) ty = operand.getEvalVal().ty();
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
            msgs->errorUndefType(operand.getCodeLoc(), ty);
            return NodeVal();
        }

        len = dataType.elements.size();
    } else if (checkIsEvalVal(operand, false) && EvalVal::isNonNullStr(operand.getEvalVal(), typeTable)) {
        len = LiteralVal::getStringLen(stringPool->get(operand.getEvalVal().str().value()));
    } else {
        msgs->errorLenOfBadType(node.getCodeLoc(), ty);
        return NodeVal();
    }

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::WIDEST_U), typeTable);
    evalVal.getWidestU() = len;

    if (!callDropFuncTmpVal(move(operand))) return NodeVal();

    return NodeVal(node.getCodeLoc(), move(evalVal));
}

NodeVal Processor::processSizeOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();
    if (!checkHasType(operand, true)) return NodeVal();

    TypeTable::Id ty;
    if (typeTable->worksAsPrimitive(operand.getType().value(), TypeTable::P_TYPE)) ty = operand.getEvalVal().ty();
    else ty = operand.getType().value();
    if (!checkIsNotUndefType(operand.getCodeLoc(), ty, true)) return NodeVal();

    optional<uint64_t> size = compiler->performSizeOf(node.getCodeLoc(), ty);
    if (!size.has_value()) return NodeVal();

    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::WIDEST_U), typeTable);
    evalVal.getWidestU() = size.value();

    if (!callDropFuncTmpVal(move(operand))) return NodeVal();

    return NodeVal(node.getCodeLoc(), move(evalVal));
}

NodeVal Processor::processIsDef(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) return NodeVal();

    NodeVal name = processForIdValue(node.getChild(1));
    if (name.isInvalid()) return NodeVal();

    NamePool::Id id = name.getEvalVal().id();

    bool isDef = symbolTable->isVarName(id) ||
        symbolTable->isFuncName(id) ||
        symbolTable->isMacroName(id) ||
        typeTable->isType(id);

    return promoteBool(node.getCodeLoc(), isDef);
}

NodeVal Processor::processAttrOf(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();
    BlockTmpValRaii operandTmpRaii = createTmpValRaii(operand);

    NodeVal name = processForIdValue(node.getChild(2));
    if (name.isInvalid()) return NodeVal();
    NamePool::Id attrName = name.getEvalVal().id();

    optional<NodeVal> nodeAttr = getAttributeFull(operand, attrName);
    if (!nodeAttr.has_value()) {
        msgs->errorAttributeNotFound(node.getCodeLoc(), attrName);
        return NodeVal();
    }

    if (!callDropFuncTmpVal(move(operand))) return NodeVal();

    return move(nodeAttr.value());
}

NodeVal Processor::processAttrIsDef(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) return NodeVal();

    NodeVal operand = processNode(node.getChild(1));
    if (operand.isInvalid()) return NodeVal();
    BlockTmpValRaii operandTmpRaii = createTmpValRaii(operand);

    NodeVal name = processForIdValue(node.getChild(2));
    if (name.isInvalid()) return NodeVal();
    NamePool::Id attrName = name.getEvalVal().id();

    bool attrIsDef = getAttributeFull(operand, attrName).has_value();

    if (!callDropFuncTmpVal(move(operand))) return NodeVal();

    return promoteBool(node.getCodeLoc(), attrIsDef);
}

// returns nullopt if not found
// not able to fail, only to not find
// update callers if that changes
optional<NodeVal> Processor::getAttribute(const AttrMap &attrs, NamePool::Id attrName) {
    auto loc = attrs.attrMap.find(attrName);
    if (loc == attrs.attrMap.end()) return nullopt;

    return *loc->second;
}

optional<NodeVal> Processor::getAttribute(const NodeVal &node, NamePool::Id attrName) {
    NamePool::Id typeId = getMeaningfulNameId(Meaningful::TYPE);

    if (attrName == typeId) {
        if (!node.hasTypeAttr()) return nullopt;

        return node.getTypeAttr();
    } else {
        if (!node.hasNonTypeAttrs()) return nullopt;

        return getAttribute(node.getNonTypeAttrs().getAttrMap(), attrName);
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
    return attr.value().getEvalVal().b();
}

optional<bool> Processor::getAttributeForBool(const NodeVal &node, const std::string &attrStrName, bool default_) {
    return getAttributeForBool(node, namePool->add(attrStrName), default_);
}

optional<NodeVal> Processor::getAttributeFull(const NodeVal &node, NamePool::Id attrName) {
    optional<NodeVal> attr = getAttribute(node, attrName);
    if (attr.has_value()) return attr;

    if (checkIsType(node, false)) {
        TypeTable::Id baseTy = typeTable->extractExplicitTypeBaseType(node.getEvalVal().ty());

        const AttrMap *attrMap = symbolTable->getDataAttrs(baseTy);
        if (attrMap != nullptr) {
            optional<NodeVal> attr = getAttribute(*attrMap, attrName);
            if (attr.has_value()) return attr;
        }
    }

    return nullopt;
}

NodeVal Processor::promoteBool(CodeLoc codeLoc, bool b) const {
    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
    evalVal.b() = b;
    return NodeVal(codeLoc, evalVal);
}

NodeVal Processor::promoteType(CodeLoc codeLoc, TypeTable::Id ty) const {
    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
    evalVal.ty() = ty;
    return NodeVal(codeLoc, move(evalVal));
}

NodeVal Processor::promoteLiteralVal(const NodeVal &node) {
    bool isId = false;

    EvalVal eval;
    LiteralVal lit = node.getLiteralVal();
    switch (lit.kind) {
    case LiteralVal::Kind::kId:
        eval = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_ID), typeTable);
        eval.id() = lit.val_id;
        isId = true;
        break;
    case LiteralVal::Kind::kSint:
        {
            TypeTable::PrimIds fitting = typeTable->shortestFittingPrimTypeI(lit.val_si);
            TypeTable::PrimIds chosen = max(TypeTable::P_I32, fitting);
            eval = EvalVal::makeVal(typeTable->getPrimTypeId(chosen), typeTable);
            if (chosen == TypeTable::P_I32) eval.i32() = lit.val_si;
            else eval.i64() = lit.val_si;
            break;
        }
    case LiteralVal::Kind::kFloat:
        {
            TypeTable::PrimIds fitting = typeTable->shortestFittingPrimTypeF(lit.val_f);
            TypeTable::PrimIds chosen = max(TypeTable::P_F32, fitting);
            eval = EvalVal::makeVal(typeTable->getPrimTypeId(chosen), typeTable);
            if (chosen == TypeTable::P_F32) eval.f32() = lit.val_f;
            else eval.f64() = lit.val_f;
            break;
        }
    case LiteralVal::Kind::kChar:
        eval = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_C8), typeTable);
        eval.c8() = lit.val_c;
        break;
    case LiteralVal::Kind::kBool:
        eval = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_BOOL), typeTable);
        eval.b() = lit.val_b;
        break;
    case LiteralVal::Kind::kString:
        eval = EvalVal::makeVal(typeTable->getTypeIdStr(), typeTable);
        eval.str() = lit.val_str;
        break;
    case LiteralVal::Kind::kNull:
        eval = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_PTR), typeTable);
        break;
    default:
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }
    NodeVal prom(node.getCodeLoc(), eval);

    NodeVal::copyNonValFieldsLeaf(prom, node, typeTable);

    if (node.hasTypeAttr() && !isId) {
        if (!checkIsType(node.getTypeAttr(), true)) return NodeVal();

        TypeTable::Id ty = node.getTypeAttr().getEvalVal().ty();

        if (!EvalVal::isImplicitCastable(prom.getEvalVal(), ty, stringPool, typeTable)) {
            msgs->errorExprCannotPromote(node.getCodeLoc(), ty);
            return NodeVal();
        }
        prom = evaluator->performCast(prom.getCodeLoc(), prom, node.getTypeAttr().getCodeLoc(), ty);
    }

    return prom;
}

bool Processor::canBeTypeDescrDecor(const NodeVal &node) {
    if (!node.isEvalVal()) return false;

    if (EvalVal::isId(node.getEvalVal(), typeTable)) {
        return isTypeDescrDecor(node.getEvalVal().id());
    }

    return EvalVal::isI(node.getEvalVal(), typeTable) || EvalVal::isU(node.getEvalVal(), typeTable);
}

bool Processor::applyTypeDescrDecor(TypeTable::TypeDescr &descr, const NodeVal &node) {
    if (!node.isEvalVal()) {
        msgs->errorInvalidTypeDecorator(node.getCodeLoc());
        return false;
    }

    if (EvalVal::isId(node.getEvalVal(), typeTable)) {
        optional<Meaningful> mean = getMeaningful(node.getEvalVal().id());
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

bool Processor::applyTupleElem(TypeTable::Tuple &tup, const NodeVal &node) {
    if (!checkIsEvalVal(node, true) || !checkIsType(node, true)) return false;

    tup.addElement(node.getEvalVal().ty());

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

    return castNode(node.getCodeLoc(), node, node.getCodeLoc(), ty, skipCheckNeedsDrop);
}

NodeVal Processor::castNode(CodeLoc codeLoc, const NodeVal &node, CodeLoc codeLocTy, TypeTable::Id ty, bool skipCheckNeedsDrop) {
    if (node.getType().value() == ty) {
        NodeVal ret(node);
        ret.setCodeLoc(codeLoc);
        return ret;
    }

    // cast result is noDrop iff node is noDrop
    if (!skipCheckNeedsDrop && !checkTransferValueOk(node.getCodeLoc(), node, node.isNoDrop(), false, true)) return NodeVal();

    if (checkIsEvalTime(node, false) && !shouldNotDispatchCastToEval(node, ty)) {
        return evaluator->performCast(codeLoc, node, codeLocTy, ty);
    } else {
        return performCast(codeLoc, node, codeLocTy, ty);
    }
}

bool Processor::implicitCastOperands(NodeVal &lhs, NodeVal &rhs, bool oneWayOnly) {
    if (lhs.getType().value() == rhs.getType().value()) return true;

    if (checkImplicitCastable(rhs, lhs.getType().value(), false)) {
        rhs = castNode(rhs.getCodeLoc(), rhs, lhs.getCodeLoc(), lhs.getType().value());
        return !rhs.isInvalid();
    } else if (!oneWayOnly && checkImplicitCastable(lhs, rhs.getType().value(), false)) {
        lhs = castNode(lhs.getCodeLoc(), lhs, rhs.getCodeLoc(), rhs.getType().value());
        return !lhs.isInvalid();
    } else {
        if (oneWayOnly) msgs->errorExprCannotImplicitCast(rhs.getCodeLoc(), rhs.getType().value(), lhs.getType().value());
        else msgs->errorExprCannotImplicitCastEither(rhs.getCodeLoc(), lhs.getType().value(), rhs.getType().value());
        return false;
    }
}

bool Processor::shouldNotDispatchCastToEval(const NodeVal &node, TypeTable::Id dstTypeId) const {
    if (!node.isEvalVal()) return true;

    const EvalVal &val = node.getEvalVal();

    TypeTable::Id srcTypeId = val.getType();

    if (typeTable->isImplicitCastable(typeTable->extractExplicitTypeBaseType(srcTypeId), typeTable->extractExplicitTypeBaseType(dstTypeId)))
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
        if (val.str().has_value()) {
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

            if (tupSrc.elements.size() != tupDst.elements.size()) return false;

            for (size_t i = 0; i < tupSrc.elements.size(); ++i) {
                if (shouldNotDispatchCastToEval(val.elems()[i], tupDst.elements[i])) return true;
            }
        }
    }

    return false;
}

bool Processor::implicitCastArgsAndVerifyCallOk(CodeLoc codeLoc, vector<NodeVal> &args, const TypeTable::Callable &callable) {
    if (callable.retType.has_value() && !checkIsNotUndefType(codeLoc, callable.retType.value(), true)) return false;

    for (size_t i = 0; i < callable.getArgCnt(); ++i) {
        args[i] = implicitCast(args[i], callable.getArgType(i), true);
        if (args[i].isInvalid()) return false;

        if (!checkTransferValueOk(args[i].getCodeLoc(), args[i], callable.getArgNoDrop(i), false, true)) return false;
    }

    return true;
}

NodeVal Processor::dispatchCall(CodeLoc codeLoc, CodeLoc codeLocFunc, const NodeVal &func, const std::vector<NodeVal> &args, bool allArgsEval) {
    if (func.isEvalVal() && allArgsEval) {
        return evaluator->performCall(codeLoc, codeLocFunc, func, args);
    } else {
        return performCall(codeLoc, codeLocFunc, func, args);
    }
}

NodeVal Processor::dispatchCall(CodeLoc codeLoc, CodeLoc codeLocFunc, FuncId funcId, const std::vector<NodeVal> &args, bool allArgsEval) {
    const FuncValue &func = symbolTable->getFunc(funcId);

    if (func.isEval() && allArgsEval) {
        return evaluator->performCall(codeLoc, codeLocFunc, funcId, args);
    } else {
        return performCall(codeLoc, codeLocFunc, funcId, args);
    }
}

NodeVal Processor::dispatchOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) {
    if (!checkHasType(oper, true)) return NodeVal();

    optional<TypeTable::Id> resTy = typeTable->addTypeDerefOf(oper.getType().value());
    if (!resTy.has_value()) {
        msgs->errorExprDerefOnBadType(codeLoc, oper.getType().value());
        return NodeVal();
    }
    if (!checkIsNotUndefType(codeLoc, resTy.value(), true)) return NodeVal();

    if (checkIsEvalTime(oper, false)) {
        return evaluator->performOperUnaryDeref(codeLoc, oper, resTy.value());
    } else {
        return performOperUnaryDeref(codeLoc, oper, resTy.value());
    }
}

NodeVal Processor::dispatchAssignment(CodeLoc codeLoc, const NodeVal &lhs, NodeVal rhs) {
    if (checkIsEvalTime(lhs, false) && checkIsEvalTime(rhs, false)) {
        return evaluator->performOperAssignment(codeLoc, lhs, move(rhs));
    } else {
        return performOperAssignment(codeLoc, lhs, move(rhs));
    }
}

NodeVal Processor::getArrElement(CodeLoc codeLoc, NodeVal &array, size_t index) {
    EvalVal evalVal = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::WIDEST_U), typeTable);
    evalVal.getWidestU() = index;
    return getArrElement(codeLoc, array, NodeVal(codeLoc, move(evalVal)));
}

// handles both arrays and array pointers
NodeVal Processor::getArrElement(CodeLoc codeLoc, NodeVal &array, const NodeVal &index) {
    TypeTable::Id arrayType = array.getType().value();
    bool isArrP = typeTable->worksAsTypeArrP(arrayType);

    optional<TypeTable::Id> elemType = typeTable->addTypeIndexOf(arrayType);
    if (!elemType.has_value()) {
        msgs->errorExprIndexOnBadType(array.getCodeLoc(), arrayType);
        return NodeVal();
    }
    if (isArrP && !checkIsNotUndefType(codeLoc, elemType.value(), true)) return NodeVal();

    TypeTable::Id resType = elemType.value();
    if (!isArrP && typeTable->worksAsTypeCn(arrayType)) resType = typeTable->addTypeCnOf(resType);

    if (checkIsEvalTime(array, false) && checkIsEvalTime(index, false)) {
        return evaluator->performOperIndexArr(codeLoc, array, index, resType);
    } else {
        return performOperIndexArr(codeLoc, array, index, resType);
    }
}

NodeVal Processor::getRawElement(CodeLoc codeLoc, NodeVal &raw, size_t index) {
    TypeTable::Id rawType = raw.getType().value();

    TypeTable::Id resType = raw.getChild(index).getType().value();
    if (typeTable->worksAsTypeCn(rawType)) resType = typeTable->addTypeCnOf(resType);

    return evaluator->performOperIndex(codeLoc, raw, index, resType);
}

NodeVal Processor::getTupleElement(CodeLoc codeLoc, NodeVal &tuple, size_t index) {
    TypeTable::Id tupleType = tuple.getType().value();

    TypeTable::Id resType = typeTable->extractTupleElementType(tupleType, index).value();

    if (checkIsEvalTime(tuple, false)) {
        return evaluator->performOperIndex(codeLoc, tuple, index, resType);
    } else {
        return performOperIndex(codeLoc, tuple, index, resType);
    }
}

NodeVal Processor::getDataElement(CodeLoc codeLoc, NodeVal &data, size_t index) {
    TypeTable::Id dataType = data.getType().value();

    TypeTable::Id resType = typeTable->extractDataTypeElementType(dataType, index).value();

    if (checkIsEvalTime(data, false)) {
        return evaluator->performOperIndex(codeLoc, data, index, resType);
    } else {
        return performOperIndex(codeLoc, data, index, resType);
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
            if (!checkIsType(node.getTypeAttr(), true)) return NodeVal();

            TypeTable::Id ty = node.getTypeAttr().getEvalVal().ty();
            for (FuncId it : funcIds) {
                if (symbolTable->getFunc(it).getType() == ty) {
                    funcId = it;
                    break;
                }
            }
            if (!funcId.has_value()) {
                msgs->errorFuncNotFound(node.getCodeLoc(), val.getUndecidedCallableVal().name, ty);
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
            if (!checkIsType(node.getTypeAttr(), true)) return NodeVal();

            TypeTable::Id ty = node.getTypeAttr().getEvalVal().ty();
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

NodeVal Processor::moveNode(CodeLoc codeLoc, NodeVal val, bool noZero) {
    if (!checkHasType(val, true)) return NodeVal();

    if (val.isNoDrop()) {
        msgs->errorExprMoveNoDrop(codeLoc);
        return NodeVal();
    }
    if (val.isInvokeArg()) {
        msgs->errorExprMoveInvokeArg(codeLoc);
        return NodeVal();
    }

    if (!val.hasRef()) {
        val.setCodeLoc(codeLoc);
        val.setLifetimeInfo(LifetimeInfo());
        return val;
    }

    if (!noZero) {
        TypeTable::Id valTy = val.getType().value();
        if (typeTable->worksAsTypeCn(valTy)) {
            msgs->errorExprMoveCn(codeLoc);
            return NodeVal();
        }

        NodeVal zero;
        if (checkIsEvalTime(val, false)) zero = evaluator->performZero(codeLoc, valTy);
        else zero = performZero(codeLoc, valTy);
        if (zero.isInvalid()) return NodeVal();

        if (dispatchAssignment(codeLoc, val, move(zero)).isInvalid()) return NodeVal();
    }

    // val itself remained unchanged
    return NodeVal::moveNoRef(codeLoc, move(val), LifetimeInfo());
}

NodeVal Processor::invoke(CodeLoc codeLoc, MacroId macroId, vector<NodeVal> args) {
    const MacroValue &macroVal = symbolTable->getMacro(macroId);

    TypeTable::Callable callable = BaseCallableValue::getCallable(macroVal, typeTable);

    if (callable.variadic) {
        // if it's variadic, it has to have at least 1 argument (the variadic one)
        size_t nonVarArgCnt = callable.getArgCnt()-1;

        CodeLoc totalVarArgCodeLoc = args.size() == nonVarArgCnt ? codeLoc : args[nonVarArgCnt].getCodeLoc();
        NodeVal totalVarArg = NodeVal::makeEmpty(totalVarArgCodeLoc, typeTable);
        NodeVal::addChildren(totalVarArg, args.begin()+nonVarArgCnt, args.end(), typeTable);

        args.resize(nonVarArgCnt);
        args.push_back(move(totalVarArg));
    }

    NodeVal ret = evaluator->performInvoke(codeLoc, macroId, move(args));
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
            if (!hasTrivialDrop(typeTable->extractTupleElementType(ty, i).value())) return false;
        }

        return true;
    } else if (typeTable->worksAsDataType(ty)) {
        const NodeVal *dropFunc = symbolTable->getDropFunc(typeTable->extractExplicitTypeBaseType(ty));
        if (dropFunc != nullptr) return false;

        size_t len = typeTable->extractLenOfDataType(ty).value();
        for (size_t i = 0; i < len; ++i) {
            if (!hasTrivialDrop(typeTable->extractDataTypeElementType(ty, i).value())) return false;
        }

        return true;
    }

    return true;
}

bool Processor::checkNotNeedsDrop(CodeLoc codeLoc, const NodeVal &val, bool orError) {
    if (!val.isNoDrop() && checkHasType(val, false) && !hasTrivialDrop(val.getType().value())) {
        if (orError) msgs->errorOwning(codeLoc);
        return false;
    }
    return true;
}

BlockTmpValRaii Processor::createTmpValRaii(NodeVal val) {
    if (checkNotNeedsDrop(val.getCodeLoc(), val, false) || val.hasRef() || val.isInvokeArg()) return BlockTmpValRaii();

    return BlockTmpValRaii(symbolTable, move(val));
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

            NodeVal elem = getArrElement(codeLoc, val, ind);
            if (elem.isInvalid()) return false;

            if (!callDropFunc(codeLoc, move(elem))) return false;
        }
    } else if (typeTable->worksAsTuple(valTy)) {
        size_t len = typeTable->extractLenOfTuple(valTy).value();
        for (size_t i = 0; i < len; ++i) {
            size_t ind = len-1-i;

            NodeVal elem = getTupleElement(codeLoc, val, ind);
            if (elem.isInvalid()) return false;

            if (!callDropFunc(codeLoc, move(elem))) return false;
        }
    } else if (typeTable->worksAsDataType(valTy)) {
        const NodeVal *dropFunc = symbolTable->getDropFunc(typeTable->extractExplicitTypeBaseType(valTy));
        if (dropFunc != nullptr) {
            const TypeTable::Callable &dropCallable = *typeTable->extractCallable(dropFunc->getType().value());

            // explicit cast cuz could be an explicit type
            NodeVal afterCast = castNode(codeLoc, val, codeLoc, dropCallable.getArgType(0), true);
            if (afterCast.isInvalid()) return false;

            bool isEval = checkIsEvalVal(afterCast, false);

            if (dispatchCall(codeLoc, codeLoc, *dropFunc, {move(afterCast)}, isEval).isInvalid()) return false;
        }

        size_t len = typeTable->extractLenOfDataType(valTy).value();
        for (size_t i = 0; i < len; ++i) {
            size_t ind = len-1-i;

            NodeVal elem = getDataElement(codeLoc, val, ind);
            if (elem.isInvalid()) return false;

            if (!callDropFunc(codeLoc, move(elem))) return false;
        }
    }

    return true;
}

bool Processor::callDropFuncTmpVal(NodeVal val) {
    if (val.hasRef()) return true;
    return callDropFunc(val.getCodeLoc(), move(val));
}

bool Processor::callDropFuncs(CodeLoc codeLoc, vector<variant<VarId, NodeVal>> vals) {
    for (const auto &it : vals) {
        if (holds_alternative<VarId>(it)) {
            VarId varId = get<VarId>(it);

            SymbolTable::VarEntry &varEntry = symbolTable->getVar(varId);
            if (varEntry.skipDrop || varEntry.var.isNoDrop()) continue;

            NodeVal loaded = dispatchLoad(codeLoc, varId);
            if (!callDropFunc(codeLoc, move(loaded))) return false;
        } else {
            if (!callDropFunc(codeLoc, move(get<NodeVal>(it)))) return false;
        }
    }

    return true;
}

bool Processor::callDropFuncsCurrBlock(CodeLoc codeLoc) {
    return callDropFuncs(codeLoc, symbolTable->getValsForDropCurrBlock());
}

bool Processor::callDropFuncsFromBlockToCurrBlock(CodeLoc codeLoc, NamePool::Id name) {
    return callDropFuncs(codeLoc, symbolTable->getValsForDropFromBlockToCurrBlock(name));
}

bool Processor::callDropFuncsFromBlockToCurrBlock(CodeLoc codeLoc, optional<NamePool::Id> name) {
    if (name.has_value()) {
        return callDropFuncsFromBlockToCurrBlock(codeLoc, name.value());
    } else {
        return callDropFuncsCurrBlock(codeLoc);
    }
}

bool Processor::callDropFuncsCurrCallable(CodeLoc codeLoc) {
    return callDropFuncs(codeLoc, symbolTable->getValsForDropCurrCallable());
}

bool Processor::processAttributes(NodeVal &node, bool forceUnescape) {
    NamePool::Id typeNameId = getMeaningfulNameId(Meaningful::TYPE);

    if (node.hasTypeAttr()) {
        if (forceUnescape) NodeVal::unescape(node.getTypeAttr(), typeTable, true);

        NodeVal procType = processNode(node.getTypeAttr());
        if (procType.isInvalid()) return false;
        if (!checkIsEvalVal(procType, true)) return false;
        if (!hasTrivialDrop(procType.getType().value())) {
            msgs->errorAttributeOwning(procType.getCodeLoc(), typeNameId);
            return false;
        }

        procType = NodeVal::moveNoRef(move(procType), LifetimeInfo());

        node.setTypeAttr(move(procType));
    }

    if (node.hasNonTypeAttrs()) {
        if (node.getNonTypeAttrs().isAttrMap()) {
            AttrMap &attrMap = node.getNonTypeAttrs().getAttrMap();

            for (auto &it : attrMap.attrMap) {
                NodeVal &attrVal = *it.second;

                if (forceUnescape) NodeVal::unescape(attrVal, typeTable, true);

                attrVal = processNode(attrVal);
                if (attrVal.isInvalid()) return false;
                if (!checkIsEvalVal(attrVal, true)) return false;
                if (!hasTrivialDrop(attrVal.getType().value())) {
                    msgs->errorAttributeOwning(attrVal.getCodeLoc(), it.first);
                    return false;
                }
            }
        } else {
            AttrMap attrMap;

            NodeVal nodeAttrs = processWithEscape(node.getNonTypeAttrs());
            if (nodeAttrs.isInvalid()) return false;

            if (!NodeVal::isRawVal(nodeAttrs, typeTable)) {
                if (!checkIsId(nodeAttrs, true)) return false;

                NamePool::Id attrName = nodeAttrs.getEvalVal().id();
                if (attrName == typeNameId) {
                    msgs->errorNonTypeAttributeType(nodeAttrs.getCodeLoc());
                    return false;
                }

                NodeVal attrVal = promoteBool(nodeAttrs.getCodeLoc(), true);

                attrMap.attrMap.insert({attrName, make_unique<NodeVal>(move(attrVal))});
            } else {
                for (size_t i = 0; i < nodeAttrs.getChildrenCnt(); ++i) {
                    NodeVal &nodeAttrEntry = nodeAttrs.getChild(i);

                    NodeVal *nodeAttrEntryName = nullptr;
                    NodeVal *nodeAttrEntryVal = nullptr;
                    if (!NodeVal::isRawVal(nodeAttrEntry, typeTable)) {
                        nodeAttrEntryName = &nodeAttrEntry;
                    } else {
                        if (!checkExactlyChildren(nodeAttrEntry, 2, true)) return false;
                        nodeAttrEntryName = &nodeAttrEntry.getChild(0);
                        nodeAttrEntryVal = &nodeAttrEntry.getChild(1);
                    }

                    if (!checkIsId(*nodeAttrEntryName, true)) return false;

                    NamePool::Id attrName = nodeAttrEntryName->getEvalVal().id();
                    if (attrName == typeNameId) {
                        msgs->errorNonTypeAttributeType(nodeAttrEntryName->getCodeLoc());
                        return false;
                    }
                    if (attrMap.attrMap.find(attrName) != attrMap.attrMap.end()) {
                        msgs->errorAttributesSameName(nodeAttrEntryName->getCodeLoc(), attrName);
                        return false;
                    }

                    NodeVal attrVal;
                    if (nodeAttrEntryVal == nullptr) {
                        attrVal = promoteBool(nodeAttrEntryName->getCodeLoc(), true);
                    } else {
                        if (forceUnescape) NodeVal::unescape(*nodeAttrEntryVal, typeTable, true);

                        attrVal = processNode(*nodeAttrEntryVal);
                        if (attrVal.isInvalid()) return false;
                        if (!checkIsEvalVal(attrVal, true)) return false;
                        if (!hasTrivialDrop(attrVal.getType().value())) {
                            msgs->errorAttributeOwning(nodeAttrEntry.getCodeLoc(), attrName);
                            return false;
                        }

                        attrVal = NodeVal::moveNoRef(move(attrVal), LifetimeInfo());
                    }

                    attrMap.attrMap.insert({attrName, make_unique<NodeVal>(move(attrVal))});
                }
            }

            node.setNonTypeAttrs(NodeVal(node.getNonTypeAttrs().getCodeLoc(), move(attrMap)));
        }
    }

    return true;
}

bool Processor::processChildNodes(const NodeVal &node) {
    for (size_t i = 0; i < node.getChildrenCnt(); ++i) {
        NodeVal tmp = processNode(node.getChild(i));
        if (tmp.isInvalid()) return false;

        if (NodeVal::isLeaf(node.getChild(i), typeTable)) {
            if (NodeVal::isFunc(tmp, typeTable)) msgs->warnUnusedFunc(tmp.getCodeLoc());
            else if (NodeVal::isMacro(tmp, typeTable)) msgs->warnUnusedMacro(tmp.getCodeLoc());
            else if (tmp.isSpecialVal()) msgs->warnUnusedSpecial(tmp.getCodeLoc(), tmp.getSpecialVal());
        }

        if (!callDropFuncTmpVal(move(tmp))) return false;
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

        argTypes.push_back(argTy.getEvalVal().ty());
        argNoDrops.push_back(attrNoDrop.value());
    }

    // ret type
    optional<TypeTable::Id> retType;
    {
        const NodeVal &nodeRetType = node.getChild(indRet);

        NodeVal ty = processNode(nodeRetType);
        if (ty.isInvalid()) {
            msgs->hintWhileProcessingRetType(nodeRetType.getCodeLoc());
            return NodeVal();
        }

        if (!checkIsEmpty(ty, false)) {
            if (!checkIsType(ty, true)) return NodeVal();
            retType = ty.getEvalVal().ty();
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
    if (!checkIsEvalVal(nodeArgNum, true)) return NodeVal();

    optional<uint64_t> argNum = EvalVal::getValueNonNeg(nodeArgNum.getEvalVal(), typeTable);
    if (!argNum.has_value()) {
        msgs->errorMacroTypeBadArgNumber(nodeArgNum.getCodeLoc());
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

NodeVal Processor::processOperUnary(CodeLoc codeLoc, const NodeVal &starting, const NodeVal &oper, Oper op) {
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
        optional<bool> attrNoZero = getAttributeForBool(starting, "noZero");
        if (!attrNoZero.has_value()) return NodeVal();

        return moveNode(codeLoc, move(operProc), attrNoZero.value());
    } else {
        if (checkIsEvalTime(operProc, false)) {
            return evaluator->performOperUnary(codeLoc, move(operProc), op);
        } else {
            return performOperUnary(codeLoc, move(operProc), op);
        }
    }
}

NodeVal Processor::processOperComparison(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op) {
    if (op == Oper::NE && opers.size() > 2) {
        msgs->errorExprCmpNeArgNum(codeLoc);
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

NodeVal Processor::processOperAssignment(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal rhs = processAndCheckHasType(*opers.back());
    if (rhs.isInvalid()) return NodeVal();

    BlockTmpValRaii rightmostRaii = createTmpValRaii(rhs);

    for (size_t ind = 0; ind+1 < opers.size(); ++ind) {
        size_t i = opers.size()-2-ind;

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

        optional<bool> attrNoDrop = getAttributeForBool(lhs, "noDrop");
        if (!attrNoDrop.has_value()) return NodeVal();

        if (!implicitCastOperands(lhs, rhs, true)) return NodeVal();

        if (!checkTransferValueOk(codeLoc, rhs, lhs.isNoDrop(), lhs.isInvokeArg(), true)) return NodeVal();

        if (!attrNoDrop.value() && !callDropFunc(lhs.getCodeLoc(), lhs)) return NodeVal();

        rhs = dispatchAssignment(codeLoc, lhs, move(rhs));
        if (rhs.isInvalid()) return NodeVal();

        // this is only needed on first iteration
        // rightmost's value is placed in the next arg
        // that arg is ref, so will be dropped elsewhere
        BlockTmpValRaii::release(rightmostRaii);
    }

    return rhs;
}

NodeVal Processor::processOperIndex(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal lhs = processNode(*opers[0]);
    if (lhs.isInvalid()) return NodeVal();

    for (size_t i = 1; i < opers.size(); ++i) {
        if (!checkHasType(lhs, true)) return NodeVal();

        TypeTable::Id baseType = lhs.getType().value();

        bool isBaseRaw = NodeVal::isRawVal(lhs, typeTable);
        bool isBaseTup = typeTable->worksAsTuple(baseType);
        bool isBaseData = typeTable->worksAsDataType(baseType);
        bool isBaseArr = typeTable->worksAsTypeArr(baseType);
        bool isBaseArrP = typeTable->worksAsTypeArrP(baseType);
        if (!isBaseRaw && !isBaseTup && !isBaseData && !isBaseArr && !isBaseArrP) {
            msgs->errorExprIndexOnBadType(lhs.getCodeLoc(), lhs.getType().value());
            return NodeVal();
        }

        if (!lhs.hasRef() && !checkNotNeedsDrop(lhs.getCodeLoc(), lhs, true)) {
            msgs->hintIndexTempOwning();
            return NodeVal();
        }

        optional<size_t> baseLen;
        if (isBaseRaw) {
            baseLen = lhs.getChildrenCnt();
        } else if (isBaseTup) {
            const TypeTable::Tuple *tuple = typeTable->extractTuple(baseType);
            if (tuple == nullptr) {
                msgs->errorExprIndexInvalidBase(lhs.getCodeLoc());
                return NodeVal();
            }
            baseLen = tuple->elements.size();
        } else if (isBaseData) {
            baseLen = typeTable->extractLenOfDataType(baseType).value();
        } else if (isBaseArr) {
            baseLen = typeTable->extractLenOfArr(baseType).value();
        }

        NodeVal index;
        optional<uint64_t> indexVal;

        if (isBaseData) {
            index = processWithEscape(*opers[i]);
        } else {
            index = processNode(*opers[i]);
        }
        if (index.isInvalid()) return NodeVal();
        if (!checkHasType(index, true)) return NodeVal();

        if (isBaseData && index.isEvalVal() && EvalVal::isId(index.getEvalVal(), typeTable)) {
            NamePool::Id indexName = index.getEvalVal().id();
            indexVal = typeTable->extractDataType(baseType)->getElemInd(indexName);
            if (!indexVal.has_value()) {
                msgs->errorElementIndexData(index.getCodeLoc(), indexName, baseType);
                return NodeVal();
            }
        } else {
            if (!typeTable->worksAsTypeI(index.getType().value()) && !typeTable->worksAsTypeU(index.getType().value())) {
                msgs->errorExprIndexNotIntegral(index.getCodeLoc());
                return NodeVal();
            }

            if (!isBaseArrP && index.isEvalVal()) {
                indexVal = EvalVal::getValueNonNeg(index.getEvalVal(), typeTable);
                if ((!indexVal.has_value()) || (baseLen.has_value() && indexVal.value() >= baseLen)) {
                    if (EvalVal::isI(index.getEvalVal(), typeTable)) {
                        int64_t ind = EvalVal::getValueI(index.getEvalVal(), typeTable).value();
                        msgs->errorExprIndexOutOfBounds(index.getCodeLoc(), ind, baseLen);
                    } else {
                        uint64_t ind = EvalVal::getValueU(index.getEvalVal(), typeTable).value();
                        msgs->errorExprIndexOutOfBounds(index.getCodeLoc(), ind, baseLen);
                    }
                    return NodeVal();
                }
            }
        }

        if ((isBaseRaw || isBaseTup || isBaseData) && !indexVal.has_value()) {
            msgs->errorNotEvalVal(index.getCodeLoc());
            return NodeVal();
        }

        if (isBaseRaw) {
            lhs = getRawElement(lhs.getCodeLoc(), lhs, (size_t) indexVal.value());
        } else if (isBaseTup) {
            lhs = getTupleElement(lhs.getCodeLoc(), lhs, (size_t) indexVal.value());
        } else if (isBaseData) {
            lhs = getDataElement(lhs.getCodeLoc(), lhs, (size_t) indexVal.value());
        } else if (isBaseArr || isBaseArrP) {
            lhs = getArrElement(lhs.getCodeLoc(), lhs, index);
            if (lhs.isInvalid()) return NodeVal();
        } else {
            msgs->errorInternal(lhs.getCodeLoc());
            return NodeVal();
        }
    }

    lhs.setCodeLoc(codeLoc);
    return lhs;
}

NodeVal Processor::processOperRegular(CodeLoc codeLoc, const NodeVal &starting, const std::vector<const NodeVal*> &opers, Oper op) {
    optional<bool> attrNoWrap = getAttributeForBool(starting, "noWrap");
    if (!attrNoWrap.has_value()) return NodeVal();

    optional<bool> attrBare = getAttributeForBool(starting, "bare");
    if (!attrBare.has_value()) return NodeVal();

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
            msgs->errorExprBinDivByZero(rhs.getCodeLoc());
            return NodeVal();
        } else if (op == Oper::SHL || op == Oper::SHR) {
            if (op == Oper::SHL && lhs.isEvalVal()) {
                optional<int64_t> lhsVal = EvalVal::getValueI(lhs.getEvalVal(), typeTable);
                if (lhsVal.has_value() && lhsVal.value() < 0) {
                    msgs->errorExprBinLeftShiftOfNeg(lhs.getCodeLoc(), lhsVal.value());
                    return NodeVal();
                }
            }

            if (rhs.isEvalVal()) {
                optional<int64_t> rhsVal = EvalVal::getValueI(rhs.getEvalVal(), typeTable);
                if (rhsVal.has_value() && rhsVal.value() < 0) {
                    msgs->errorExprBinShiftByNeg(rhs.getCodeLoc(), rhsVal.value());
                    return NodeVal();
                }
            }
        }

        OperRegAttrs attrs;
        attrs.noWrap = attrNoWrap.value();
        attrs.bare = attrBare.value();

        NodeVal nextLhs;
        if (checkIsEvalTime(lhs, false) && checkIsEvalTime(rhs, false)) {
            nextLhs = evaluator->performOperRegular(codeLoc, lhs, rhs, op, attrs);
        } else {
            nextLhs = performOperRegular(codeLoc, lhs, rhs, op, attrs);
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

NodeVal Processor::processForIdValue(const NodeVal &node) {
    NodeVal id = processWithEscape(node);
    if (id.isInvalid()) return NodeVal();
    if (checkIsRaw(id, false)) {
        id = processNode(move(id));
        if (id.isInvalid()) return NodeVal();
    }
    if (!checkIsId(id, true)) return NodeVal();
    return id;
}

NodeVal Processor::processForIdOrEmpty(const NodeVal &node) {
    NodeVal id = processWithEscape(node);
    if (id.isInvalid()) return NodeVal();
    if (checkIsRaw(id, false)) {
        if (checkIsEmpty(id, false)) return id;
        id = processNode(move(id));
        if (id.isInvalid()) return NodeVal();
    }
    if (!checkIsId(id, true)) return NodeVal();
    return id;
}

NodeVal Processor::processForTypeArg(const NodeVal &node) {
    NodeVal esc = processWithEscape(node);
    if (esc.isInvalid()) return NodeVal();
    if (!esc.isEvalVal()) {
        msgs->errorInvalidTypeArg(esc.getCodeLoc());
        return NodeVal();
    }

    if (canBeTypeDescrDecor(esc)) {
        return esc;
    }

    return processNode(esc);
}

pair<NodeVal, optional<NodeVal>> Processor::processForIdTypePair(const NodeVal &node) {
    pair<NodeVal, optional<NodeVal>> retInvalid = make_pair<NodeVal, optional<NodeVal>>(NodeVal(), nullopt);

    NodeVal esc = processForIdValue(node);
    if (esc.isInvalid()) return retInvalid;

    NodeVal id = esc;
    id.clearTypeAttr();

    optional<NodeVal> ty;
    if (esc.hasTypeAttr()) {
        ty = processNode(esc.getTypeAttr());
        if (ty.value().isInvalid()) return retInvalid;
        if (!checkIsType(ty.value(), true)) return retInvalid;
    }

    return make_pair<NodeVal, optional<NodeVal>>(move(id), move(ty));
}

NodeVal Processor::processForScopeResult(const NodeVal &node, bool callableClosing) {
    NodeVal processed = processNode(node);
    if (processed.isInvalid()) return NodeVal();

    bool mayMoveNode = false;
    if (processed.getLifetimeInfo().has_value() && processed.getLifetimeInfo().value().nestLevel.has_value()) {
        if (callableClosing) {
            mayMoveNode = !processed.getLifetimeInfo().value().nestLevel.value().callableGreaterThan(symbolTable->currNestLevel());
        } else {
            mayMoveNode = !processed.getLifetimeInfo().value().nestLevel.value().greaterThan(symbolTable->currNestLevel());
        }
    }

    NodeVal moved;
    if (mayMoveNode) {
        optional<VarId> varId = processed.getVarId();
        moved = moveNode(processed.getCodeLoc(), move(processed), true);
        if (varId.has_value()) symbolTable->getVar(varId.value()).skipDrop = true;
    } else {
        moved = move(processed);
    }
    return moved;
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
        if (orError) msgs->errorUnexpectedNotEmpty(node.getCodeLoc());
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

bool Processor::checkIsEvalBlock(CodeLoc codeLoc, const SymbolTable::Block &block, bool orError) {
    if (!block.isEval()) {
        if (orError) msgs->errorNonEvalBlock(codeLoc);
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

bool Processor::checkIsNotUndefType(CodeLoc codeLoc, TypeTable::Id ty, bool orError) {
    if (typeTable->isUndef(ty)) {
        if (orError) msgs->errorUndefType(codeLoc, ty);
        return false;
    }
    return true;
}

bool Processor::checkTransferValueOk(CodeLoc codeLoc, const NodeVal &src, bool dstNoDrop, bool dstInvokeArg, bool orError) {
    // if non-owning, no checks necessary
    if (!hasTrivialDrop(src.getType().value()) && !dstNoDrop) {
        // cannot copy from ref owning
        // cannot copy from noDrop (non-owning) to owning
        // cannot copy from invoke args to protect from misuse
        // cannot copy to invoke arg, cuz reassign needs drop, which is skipped on invoke args
        if (src.hasRef()) {
            if (orError) msgs->errorBadTransfer(codeLoc);
            return false;
        } else if (src.isNoDrop()) {
            if (orError) msgs->errorTransferNoDrop(codeLoc);
            return false;
        } else if (src.isInvokeArg() || dstInvokeArg) {
            msgs->errorTransferInvokeArgs(codeLoc);
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
        if (orError) {
            msgs->errorDropFuncBadSig(node.getCodeLoc());
            msgs->hintDropFuncSig();
        }
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