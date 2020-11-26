#include "Processor.h"
#include "Reserved.h"
#include "Evaluator.h"
using namespace std;

Processor::Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompilationMessages *msgs, Evaluator *evaluator)
    : namePool(namePool), stringPool(stringPool), typeTable(typeTable), symbolTable(symbolTable), msgs(msgs), evaluator(evaluator), topmost(0) {
}

NodeVal Processor::processNode(const NodeVal &node) {
    unsigned oldTopmost = topmost;
    if (topmost < 2) ++topmost;
    DeferredCallback guard([&, oldTopmost] { topmost = oldTopmost; });

    if (node.isLeaf()) return processLeaf(node);
    else return processNonLeaf(node);
}

NodeVal Processor::processLeaf(const NodeVal &node) {
    // TODO remove after escaped raw special processing
    if (node.isEmpty()) return node;

    // TODO if escaped, just return?
    NodeVal prom = node.isLiteralVal() ? promoteLiteralVal(node) : node;

    if (!prom.isEscaped() && prom.isEvalVal() && EvalVal::isId(prom.getEvalVal(), typeTable)) {
        return processId(prom);
    }

    return prom;
}

NodeVal Processor::processNonLeaf(const NodeVal &node) {
    // TODO special processing for escaped raw (+ change spec mockup)
    NodeVal starting = processNode(node.getChild(0));
    if (starting.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);

    if (starting.isEvalVal() && EvalVal::isMacro(starting.getEvalVal(), symbolTable)) {
        NodeVal invoked = processInvoke(node, starting);
        if (invoked.isInvalid()) return NodeVal();

        return processNode(invoked);
    }

    if (starting.isEvalVal() && EvalVal::isType(starting.getEvalVal(), typeTable)) {
        return processType(node, starting);
    }

    if (starting.isEvalVal() && EvalVal::isFunc(starting.getEvalVal(), symbolTable)) {
        return processCall(node, starting);
    }

    optional<NamePool::Id> callName = starting.isEvalVal() ? starting.getEvalVal().getCallableId() : nullopt;
    if (callName.has_value()) {
        optional<Keyword> keyw = getKeyword(starting.getEvalVal().id);
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
            case Keyword::FNC:
                return processFnc(node);
            case Keyword::RET:
                return processRet(node);
            case Keyword::MAC:
                return processMac(node);
            case Keyword::EVAL:
                return processEval(node);
            case Keyword::IMPORT:
                return processImport(node);
            default:
                msgs->errorUnexpectedKeyword(starting.getCodeLoc(), keyw.value());
                return NodeVal();
            }
        }

        optional<Oper> op = getOper(starting.getEvalVal().id);
        if (op.has_value()) {
            return processOper(node, op.value());
        }

        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    // TODO tup for tuple, (id) does lookup, error otherwise
    return processTuple(node, starting);
}

NodeVal Processor::processInvoke(const NodeVal &node, const NodeVal &starting) {
    return NodeVal(); // TODO
}

NodeVal Processor::processType(const NodeVal &node, const NodeVal &starting) {
    if (node.getLength() == 1)
        return NodeVal(node.getCodeLoc(), EvalVal::copyNoRef(starting.getEvalVal()));

    NodeVal second = processForTypeArg(node.getChild(1));
    if (second.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);

    EvalVal evalTy = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);

    if (second.isEvalVal() && EvalVal::isType(second.getEvalVal(), typeTable)) {
        TypeTable::Tuple tup;
        tup.members.reserve(node.getChildrenCnt());
        tup.members.push_back(starting.getEvalVal().ty);
        tup.members.push_back(second.getEvalVal().ty);
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal ty = processAndCheckIsType(node.getChild(i));
            if (ty.isInvalid()) return NodeVal();
            if (isSkippingProcessing()) return NodeVal(true);
            tup.members.push_back(ty.getEvalVal().ty);
        }

        optional<TypeTable::Id> tupTypeId = typeTable->addTuple(move(tup));
        if (!tupTypeId.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        evalTy.ty = tupTypeId.value();
    } else {
        TypeTable::TypeDescr descr(starting.getEvalVal().ty);
        if (!applyTypeDescrDecor(descr, second)) return NodeVal();
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal decor = processWithEscapeIfLeaf(node.getChild(i));
            if (decor.isInvalid()) return NodeVal();
            if (isSkippingProcessing()) return NodeVal(true);
            if (!applyTypeDescrDecor(descr, decor)) return NodeVal();
        }

        evalTy.ty = typeTable->addTypeDescr(move(descr));
    }

    return NodeVal(node.getCodeLoc(), evalTy);
}

NodeVal Processor::processId(const NodeVal &node) {
    NamePool::Id id = node.getEvalVal().id;
    
    NodeVal *value = symbolTable->getVar(id);
    
    if (value != nullptr) {
        if (checkIsEvalVal(*value, false)) {
            return evaluator->performLoad(node.getCodeLoc(), id, *value);
        } else {
            return performLoad(node.getCodeLoc(), id, *value);
        }
    } else if (symbolTable->isFuncName(id) || symbolTable->isMacroName(id) ||
        isKeyword(id) || isOper(id)) {
        // TODO these as first-class values, with ref
        EvalVal eval;
        eval.id = id;

        return NodeVal(node.getCodeLoc(), eval);
    } else if (typeTable->isType(id)) {
        // TODO! ref for types
        EvalVal eval = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
        eval.ty = typeTable->getTypeId(id).value();

        return NodeVal(node.getCodeLoc(), eval);
    } else {
        msgs->errorSymNotFound(node.getCodeLoc(), id);
        return NodeVal();
    }
}

NodeVal Processor::processSym(const NodeVal &node) {
    if (!checkAtLeastChildren(node, 2, true)) return NodeVal();

    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        const NodeVal &entry = node.getChild(i);
        // continue with the other sym entries to minimize the number of errors printed out
        if (!entry.isLeaf() && !checkBetweenChildren(entry, 1, 2, true)) continue;

        bool hasInit = checkExactlyChildren(entry, 2, false);

        const NodeVal &nodePair = entry.isLeaf() ? entry : entry.getChild(0);
        pair<NodeVal, optional<NodeVal>> pair = processForIdTypePair(nodePair);
        if (pair.first.isInvalid()) continue;
        if (isSkippingProcessing()) return NodeVal(true);
        NamePool::Id id = pair.first.getEvalVal().id;
        if (!symbolTable->nameAvailable(id, namePool, typeTable)) {
            msgs->errorSymNameTaken(nodePair.getCodeLoc(), id);
            continue;
        }
        optional<TypeTable::Id> optType;
        if (pair.second.has_value()) optType = pair.second.value().getEvalVal().ty;

        bool hasType = optType.has_value();

        if (hasInit) {
            const NodeVal &nodeInit = entry.getChild(1);
            NodeVal init = hasType ? processAndImplicitCast(nodeInit, optType.value()) : processNode(nodeInit);
            if (init.isInvalid()) continue;
            if (isSkippingProcessing()) return NodeVal(true);

            NodeVal nodeReg = performRegister(entry.getCodeLoc(), id, init);
            if (nodeReg.isInvalid()) continue;
            symbolTable->addVar(id, move(nodeReg));
        } else {
            if (!hasType) {
                msgs->errorMissingTypeAttribute(nodePair.getCodeLoc());
                continue;
            }
            if (typeTable->worksAsTypeCn(optType.value())) {
                msgs->errorUnknown(entry.getCodeLoc());
                continue;
            }

            NodeVal nodeReg = performRegister(entry.getCodeLoc(), id, optType.value());
            if (nodeReg.isInvalid()) continue;
            symbolTable->addVar(id, move(nodeReg));
        }
    }

    return NodeVal(true);
}

NodeVal Processor::processCast(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) {
        return NodeVal();
    }

    NodeVal ty = processAndCheckIsType(node.getChild(1));
    if (ty.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);

    NodeVal value = processNode(node.getChild(2));
    if (value.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);

    if (checkIsEvalVal(value, false) && EvalVal::isCastable(value.getEvalVal(), ty.getEvalVal().ty, stringPool, typeTable))
        return evaluator->performCast(node.getCodeLoc(), value, ty.getEvalVal().ty);
    else
        return performCast(node.getCodeLoc(), value, ty.getEvalVal().ty);
}

NodeVal Processor::processBlock(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 4, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 3;
    bool hasType = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indType = hasName ? 2 : 1;
    size_t indBody = hasName ? 3 : (hasType ? 2 : 1);

    const NodeVal &nodeBlock = node.getChild(indBody);
    if (!checkIsRaw(nodeBlock, true)) return NodeVal();

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscapeIfLeaf(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
        if (!nodeName.isEmpty()) {
            if (!checkIsId(nodeName, true)) return NodeVal();
            name = nodeName.getEvalVal().id;
        }
    }

    optional<TypeTable::Id> type;
    if (hasType) {
        NodeVal nodeType = processNode(node.getChild(indType));
        if (nodeType.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
        if (!nodeType.isEmpty()) {
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
        SymbolTable::Block *blockInSymbolTable = symbolTable->getLastBlock();

        if (!processChildNodes(nodeBlock)) {
            performBlockTearDown(node.getCodeLoc(), *blockInSymbolTable, false);
            return NodeVal();
        }

        if (isRepeatingProcessing(block.name)) {
            if (!performBlockReentry(node.getCodeLoc())) return NodeVal();
            continue;
        }

        NodeVal ret = performBlockTearDown(node.getCodeLoc(), *blockInSymbolTable, true);
        if (ret.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
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
        NodeVal nodeName = processWithEscapeIfLeafAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
        name = nodeName.getEvalVal().id;
    }

    NodeVal nodeCond = processNode(node.getChild(indCond));
    if (nodeCond.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);
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
    return NodeVal(true);
}

NodeVal Processor::processLoop(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 3, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indCond = hasName ? 2 : 1;

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscapeIfLeafAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
        name = nodeName.getEvalVal().id;
    }

    NodeVal nodeCond = processNode(node.getChild(indCond));
    if (nodeCond.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);
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
    return NodeVal(true);
}

NodeVal Processor::processPass(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 3, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indVal = hasName ? 2 : 1;

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscapeIfLeafAndCheckIsId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
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
    if (isSkippingProcessing()) return NodeVal(true);

    if (!performPass(node.getCodeLoc(), *targetBlock, nodeValue)) return NodeVal();
    return NodeVal(true);
}

NodeVal Processor::processCall(const NodeVal &node, const NodeVal &starting) {
    NamePool::Id name = starting.getEvalVal().getCallableId().value();
    const FuncValue *funcVal = symbolTable->getFunction(name);
    if (funcVal == nullptr) {
        msgs->errorFuncNotFound(starting.getCodeLoc(), name);
        return NodeVal();
    }
    
    size_t providedArgCnt = node.getChildrenCnt()-1;
    if (funcVal->argCnt() > providedArgCnt ||
        (funcVal->argCnt() < providedArgCnt && !funcVal->variadic)) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();    
    }

    vector<NodeVal> args;
    for (size_t i = 0; i < providedArgCnt; ++i) {
        NodeVal arg = i < funcVal->argCnt() ?
            processAndImplicitCast(node.getChild(i+1), funcVal->argTypes[i]) :
            processNode(node.getChild(i+1));
        if (arg.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);

        args.push_back(move(arg));
    }

    if (funcVal->isEval()) {
        return evaluator->performCall(node.getCodeLoc(), *funcVal, args);
    } else {
        return performCall(node.getCodeLoc(), *funcVal, args);
    }
}

// TODO allow function definition after declaration
NodeVal Processor::processFnc(const NodeVal &node) {
    if (!checkInGlobalScope(node.getCodeLoc(), true) ||
        !checkBetweenChildren(node, 4, 5, true)) {
        return NodeVal();
    }

    FuncValue val;
    val.defined = node.getChildrenCnt() == 5;

    // name
    NodeVal name = processWithEscapeIfLeafAndCheckIsId(node.getChild(1));
    if (name.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);
    if (!symbolTable->nameAvailable(name.getEvalVal().id, namePool, typeTable)) {
        msgs->errorFuncNameTaken(name.getCodeLoc(), name.getEvalVal().id);
        return NodeVal();
    }
    val.name = name.getEvalVal().id;

    // arguments
    const NodeVal &nodeArgs = node.getChild(2);
    if (!checkIsRaw(nodeArgs, true)) return NodeVal();
    val.variadic = false;
    for (size_t i = 0; i < nodeArgs.getChildrenCnt(); ++i) {
        const NodeVal &nodeArg = nodeArgs.getChild(i);

        if (val.variadic) {
            msgs->errorNotLastParam(nodeArg.getCodeLoc());
            return NodeVal();
        }

        pair<NodeVal, optional<NodeVal>> arg = processForIdTypePair(nodeArg);
        if (arg.first.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
        NamePool::Id argId = arg.first.getEvalVal().id;
        if (isMeaningful(argId, Meaningful::ELLIPSIS)) {
            val.variadic = true;
        } else {
            if (!arg.second.has_value()) {
                msgs->errorMissingTypeAttribute(nodeArg.getCodeLoc());
                return NodeVal();
            }
            val.argNames.push_back(argId);
            val.argTypes.push_back(arg.second.value().getEvalVal().ty);
        }
    }

    // check no arg name duplicates
    for (size_t i = 0; i+1 < nodeArgs.getChildrenCnt(); ++i) {
        for (size_t j = i+1; j < nodeArgs.getChildrenCnt(); ++j) {
            if (val.argNames[i] == val.argNames[j]) {
                msgs->errorArgNameDuplicate(nodeArgs.getChild(j).getCodeLoc(), val.argNames[j]);
                return NodeVal();
            }
        }
    }

    // ret type
    const NodeVal &nodeRetType = node.getChild(3);
    NodeVal ty = processNode(nodeRetType);
    if (ty.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);
    if (!ty.isEmpty()) {
        if (!checkIsType(ty, true)) return NodeVal();
        val.retType = ty.getEvalVal().ty;
    }

    // body
    const NodeVal *nodeBodyPtr = nullptr;
    if (val.defined) {
        nodeBodyPtr = &node.getChild(4);
        if (!checkIsRaw(*nodeBodyPtr, true)) {
            return NodeVal();
        }
    }

    if (!performFunctionDeclaration(node.getCodeLoc(), val)) return NodeVal();
    FuncValue *symbVal = symbolTable->registerFunc(val);

    if (val.defined) {
        if (!performFunctionDefinition(nodeArgs, *nodeBodyPtr, *symbVal)) return NodeVal();
    }

    return NodeVal(true);
}

NodeVal Processor::processRet(const NodeVal &node) {
    if (!checkBetweenChildren(node, 1, 2, true)) {
        return NodeVal();
    }

    optional<FuncValue> optFuncValue = symbolTable->getCurrFunc();
    if (!optFuncValue.has_value()) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

    bool retsVal = node.getChildrenCnt() == 2;
    if (retsVal) {
        if (!optFuncValue.value().hasRet()) {
            msgs->errorUnknown(node.getCodeLoc());
            return NodeVal();
        }

        NodeVal val = processAndImplicitCast(node.getChild(1), optFuncValue.value().retType.value());
        if (val.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);

        if (!performRet(node.getCodeLoc(), val)) return NodeVal();
    } else {
        if (optFuncValue.value().hasRet()) {
            msgs->errorRetNoValue(node.getCodeLoc(), optFuncValue.value().retType.value());
            return NodeVal();
        }

        if (!performRet(node.getCodeLoc())) return NodeVal();
    }

    return NodeVal(true);
}

NodeVal Processor::processMac(const NodeVal &node) {
    return NodeVal(); // TODO
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
    if (isSkippingProcessing()) {
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }
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

NodeVal Processor::processTuple(const NodeVal &node, const NodeVal &starting) {
    // TODO rework after allowing single value tuples
    if (node.getChildrenCnt() == 1) return starting;

    vector<NodeVal> membs;
    membs.reserve(node.getChildrenCnt());
    membs.push_back(starting);
    bool allEval = checkIsEvalVal(starting, false);
    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        NodeVal memb = processNode(node.getChild(i));
        if (memb.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
        membs.push_back(move(memb));
        if (!checkIsEvalVal(memb, false)) allEval = false;
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

    if (node.isEscaped()) prom.escape();
    
    // TODO remove id special case, but handle on calls instead?
    if (node.hasTypeAttr() && !isId) {
        NodeVal nodeTy = processAndCheckIsType(node.getTypeAttr());
        if (nodeTy.isInvalid()) return NodeVal();
        TypeTable::Id ty = nodeTy.getEvalVal().ty;

        if (!EvalVal::isImplicitCastable(eval, ty, stringPool, typeTable)) {
            msgs->errorExprCannotPromote(node.getCodeLoc(), ty);
            return NodeVal();
        }
        prom = evaluator->performCast(prom.getCodeLoc(), prom, ty);
    }

    return prom;
}

bool Processor::applyTypeDescrDecor(TypeTable::TypeDescr &descr, const NodeVal &node) {
    if (!node.isEvalVal()) {
        msgs->errorInvalidTypeDecorator(node.getCodeLoc());
        return false;
    }

    if (EvalVal::isId(node.getEvalVal(), typeTable)) {
        optional<Meaningful> mean = getMeaningful(node.getEvalVal().id);
        if (!mean.has_value() || !isTypeDescr(mean.value())) {
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
            if (arrSize.value() == 0) {
                msgs->errorBadArraySize(node.getCodeLoc(), arrSize.value());
            } else {
                optional<int64_t> integ = EvalVal::getValueI(node.getEvalVal(), typeTable);
                if (integ.has_value()) {
                    msgs->errorBadArraySize(node.getCodeLoc(), integ.value());
                } else {
                    msgs->errorInvalidTypeDecorator(node.getCodeLoc());
                }
            }
            return false;
        }

        descr.addDecor({.type=TypeTable::TypeDescr::Decor::D_ARR, .len=arrSize.value()});
    }

    return true;
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

bool Processor::processChildNodes(const NodeVal &node) {
    for (size_t i = 0; i < node.getChildrenCnt(); ++i) {
        NodeVal tmp = processNode(node.getChild(i));
        if (tmp.isInvalid()) return false;
        if (isSkippingProcessing()) return true;
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
    if (isSkippingProcessing()) return NodeVal(true);

    if (op == Oper::MUL) {
        return dispatchOperUnaryDeref(codeLoc, operProc);
    } else {
        if (checkIsEvalVal(oper, false)) {
            return evaluator->performOperUnary(codeLoc, operProc, op);
        } else {
            return performOperUnary(codeLoc, operProc, op);
        }
    }
}

NodeVal Processor::processOperComparison(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op) {
    NodeVal lhs = processAndCheckHasType(*opers[0]);
    if (lhs.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);

    // redirecting to evaluator when all operands are EvalVals is more complicated in the case of comparisons
    // the reason for is that LLVM's phi nodes need to be started up and closed appropriately
    void *evalSignal = nullptr;
    DeferredCallback evalSignalDeleteGuard([=] { delete (int*) evalSignal; });

    void *signal = nullptr;
    DeferredCallback signalDeleteGuard([=] { delete (int*) signal; });

    // do set up on the appropriate processor subclass, depending on whether first operand is EvalVal
    bool stillEval = checkIsEvalVal(lhs, false);
    if (stillEval) {
        evalSignal = evaluator->performOperComparisonSetUp(codeLoc, opers.size());
        if (evalSignal == nullptr) return NodeVal();
    } else {
        signal = performOperComparisonSetUp(codeLoc, opers.size());
        if (signal == nullptr) return NodeVal();
    }

    for (size_t i = 1; i < opers.size(); ++i) {
        NodeVal rhs = processAndCheckHasType(*opers[i]);
        if (rhs.isInvalid()) {
            if (stillEval) return evaluator->performOperComparisonTearDown(codeLoc, false, evalSignal);
            else return performOperComparisonTearDown(codeLoc, false, signal);
        }
        if (isSkippingProcessing()) {
            if (stillEval) evaluator->performOperComparisonTearDown(codeLoc, true, evalSignal);
            else performOperComparisonTearDown(codeLoc, true, signal);
            return NodeVal(true);
        }

        if (stillEval && !checkIsEvalVal(rhs, false)) {
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
    if (isSkippingProcessing()) return NodeVal(true);

    for (size_t i = opers.size()-2;; --i) {
        NodeVal lhs = processAndCheckHasType(*opers[i]);
        if (lhs.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);

        if (!lhs.hasRef()) {
            msgs->errorExprAsgnNonRef(lhs.getCodeLoc());
            return NodeVal();
        }
        if (typeTable->worksAsTypeCn(lhs.getType().value())) {
            msgs->errorExprAsgnOnCn(lhs.getCodeLoc());
            return NodeVal();
        }

        if (!implicitCastOperands(lhs, rhs, true)) return NodeVal();

        if (checkIsEvalVal(lhs, false) && checkIsEvalVal(rhs, false)) {
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
    if (isSkippingProcessing()) return NodeVal(true);

    for (size_t i = 1; i < opers.size(); ++i) {
        TypeTable::Id baseType = base.getType().value();

        optional<TypeTable::Id> elemType = typeTable->addTypeIndexOf(baseType);
        if (!elemType.has_value()) {
            msgs->errorExprIndexOnBadType(base.getCodeLoc(), baseType);
            return NodeVal();
        }

        NodeVal index = processAndCheckHasType(*opers[i]);
        if (index.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
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

        // TODO allow eval indexing of strings
        if (checkIsEvalVal(base, false) && checkIsEvalVal(index, false) &&
            !typeTable->worksAsTypeStr(base.getEvalVal().type.value())) {
            base = evaluator->performOperIndex(codeLoc, base, index, elemType.value());
        } else {
            base = performOperIndex(base.getCodeLoc(), base, index, elemType.value());
        }
        if (base.isInvalid()) return NodeVal();
    }

    return base;
}

NodeVal Processor::processOperMember(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal base = processAndCheckHasType(*opers[0]);
    if (base.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);

    for (size_t i = 1; i < opers.size(); ++i) {
        if (typeTable->worksAsTypeP(base.getType().value())) {
            base = dispatchOperUnaryDeref(base.getCodeLoc(), base);
            if (base.isInvalid()) return NodeVal();
            if (!checkHasType(base, true)) return NodeVal();
        }
        TypeTable::Id baseType = base.getType().value();

        NodeVal index = processNode(*opers[i]);
        if (index.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);
        if (!index.isEvalVal()) {
            msgs->errorMemberIndex(index.getCodeLoc());
            return NodeVal();
        }

        optional<const TypeTable::Tuple*> tupleOpt = typeTable->extractTuple(baseType);
        if (!tupleOpt.has_value()) {
            msgs->errorExprDotInvalidBase(base.getCodeLoc());
            return NodeVal();
        }
        const TypeTable::Tuple &tuple = *tupleOpt.value();

        optional<uint64_t> indexValOpt = EvalVal::getValueNonNeg(index.getEvalVal(), typeTable);
        if (!indexValOpt.has_value() || indexValOpt.value() >= tuple.members.size()) {
            msgs->errorMemberIndex(index.getCodeLoc());
            return NodeVal();
        }
        uint64_t indexVal = indexValOpt.value();

        optional<TypeTable::Id> resType = typeTable->extractMemberType(baseType, (size_t) indexVal);
        if (!resType.has_value()) {
            msgs->errorInternal(base.getCodeLoc());
            return NodeVal();
        }

        if (checkIsEvalVal(base, false)) {
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
    if (isSkippingProcessing()) return NodeVal(true);

    for (size_t i = 1; i < opers.size(); ++i) {
        NodeVal rhs = processAndCheckHasType(*opers[i]);
        if (rhs.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(true);

        if (!implicitCastOperands(lhs, rhs, false)) return NodeVal();

        if (checkIsEvalVal(lhs, false) && checkIsEvalVal(rhs, false)) {
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
    if (isSkippingProcessing()) return NodeVal(true);
    if (!checkIsType(ty, true)) return NodeVal();
    return ty;
}

NodeVal Processor::processWithEscapeIfLeaf(const NodeVal &node) {
    if (node.isLeaf()) {
        NodeVal esc = node;
        esc.escape();
        return processNode(esc);
    } else {
        return processNode(node);
    }
}

NodeVal Processor::processWithEscapeIfLeafAndCheckIsId(const NodeVal &node) {
    NodeVal id = processWithEscapeIfLeaf(node);
    if (id.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);
    if (!checkIsId(id, true)) return NodeVal();
    return id;
}

NodeVal Processor::processForTypeArg(const NodeVal &node) {
    if (!node.isLeaf() || node.isEscaped()) return processNode(node);
    
    NodeVal esc = processWithEscapeIfLeaf(node);
    if (esc.isInvalid()) return NodeVal();

    if (esc.isEvalVal() && EvalVal::isId(esc.getEvalVal(), typeTable) &&
        !isTypeDescr(esc.getEvalVal().id)) {
        esc.unescape();
        esc = processNode(esc);
    }

    return esc;
}

pair<NodeVal, optional<NodeVal>> Processor::processForIdTypePair(const NodeVal &node) {
    const pair<NodeVal, optional<NodeVal>> retInvalid = make_pair<NodeVal, optional<NodeVal>>(NodeVal(), nullopt);
    const pair<NodeVal, optional<NodeVal>> retSkipping = make_pair<NodeVal, optional<NodeVal>>(NodeVal(true), nullopt);
    
    NodeVal id = node;
    id.clearTypeAttr();
    id = processWithEscapeIfLeafAndCheckIsId(id);
    if (id.isInvalid()) return retInvalid;
    if (isSkippingProcessing()) return retSkipping;

    optional<NodeVal> ty;
    if (node.hasTypeAttr()) {
        ty = processAndCheckIsType(node.getTypeAttr());
        if (ty.value().isInvalid()) return retInvalid;
        if (isSkippingProcessing()) return retSkipping;
    }

    return make_pair<NodeVal, optional<NodeVal>>(move(id), move(ty));
}

NodeVal Processor::processAndCheckHasType(const NodeVal &node) {
    NodeVal proc = processNode(node);
    if (proc.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);
    if (!checkHasType(proc, true)) return NodeVal();
    return proc;
}

NodeVal Processor::processAndImplicitCast(const NodeVal &node, TypeTable::Id ty) {
    NodeVal proc = processNode(node);
    if (proc.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(true);

    if (!checkImplicitCastable(proc, ty, true)) return NodeVal();

    if (proc.getType().value() == ty) {
        if (checkIsEvalVal(proc, false))
            return NodeVal(proc.getCodeLoc(), EvalVal::copyNoRef(proc.getEvalVal()));
        else
            return NodeVal(proc.getCodeLoc(), LlvmVal::copyNoRef(proc.getLlvmVal()));
    }

    return dispatchCast(proc.getCodeLoc(), proc, ty);
}

NodeVal Processor::dispatchCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    if (checkIsEvalVal(node, false) && EvalVal::isCastable(node.getEvalVal(), ty, stringPool, typeTable))
        return evaluator->performCast(node.getCodeLoc(), node, ty);
    else
        return performCast(node.getCodeLoc(), node, ty);
}

NodeVal Processor::dispatchOperUnaryDeref(CodeLoc codeLoc, const NodeVal &oper) {
    if (checkIsEvalVal(oper, false)) {
        return evaluator->performOperUnaryDeref(codeLoc, oper);
    } else {
        return performOperUnaryDeref(codeLoc, oper);
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

bool Processor::checkIsRaw(const NodeVal &node, bool orError) {
    if (!node.isRaw()) {
        if (orError) msgs->errorUnexpectedIsTerminal(node.getCodeLoc());
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
    if (!node.isRaw() || node.getChildrenCnt() != n) {
        if (orError) msgs->errorChildrenNotEq(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkAtLeastChildren(const NodeVal &node, std::size_t n, bool orError) {
    if (!node.isRaw() || node.getChildrenCnt() < n) {
        if (orError) msgs->errorChildrenLessThan(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkAtMostChildren(const NodeVal &node, std::size_t n, bool orError) {
    if (!node.isRaw() || node.getChildrenCnt() > n) {
        if (orError) msgs->errorChildrenMoreThan(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkBetweenChildren(const NodeVal &node, std::size_t nLo, std::size_t nHi, bool orError) {
    if (!node.isRaw() || !between(node.getChildrenCnt(), nLo, nHi)) {
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