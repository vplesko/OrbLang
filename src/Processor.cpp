#include "Processor.h"
#include "Reserved.h"
#include "Evaluator.h"
using namespace std;

Processor::Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs, Evaluator *evaluator)
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
    if (node.isEmpty()) return node;

    NodeVal prom = node.isLiteralVal() ? promoteLiteralVal(node) : node;

    if (!prom.isEscaped() && prom.isKnownVal() && KnownVal::isId(prom.getKnownVal(), typeTable)) {
        return processId(prom);
    }

    return prom;
}

NodeVal Processor::processNonLeaf(const NodeVal &node) {
    NodeVal starting = processNode(node.getChild(0));
    if (starting.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

    if (starting.isKnownVal() && KnownVal::isMacro(starting.getKnownVal(), symbolTable)) {
        NodeVal invoked = processInvoke(node, starting);
        if (invoked.isInvalid()) return NodeVal();

        return processNode(invoked);
    }

    if (starting.isKnownVal() && KnownVal::isType(starting.getKnownVal(), typeTable)) {
        return processType(node, starting);
    }

    if (starting.isKnownVal() && KnownVal::isFunc(starting.getKnownVal(), symbolTable)) {
        return processCall(node, starting);
    }

    optional<NamePool::Id> callName = starting.isKnownVal() ? starting.getKnownVal().getCallableId() : nullopt;
    if (callName.has_value()) {
        optional<Keyword> keyw = getKeyword(starting.getKnownVal().id);
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

        optional<Oper> op = getOper(starting.getKnownVal().id);
        if (op.has_value()) {
            return processOper(node, op.value());
        }

        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }

    return processTuple(node, starting);
}

NodeVal Processor::processInvoke(const NodeVal &node, const NodeVal &starting) {
    return NodeVal(); // TODO
}

NodeVal Processor::processType(const NodeVal &node, const NodeVal &starting) {
    if (node.getLength() == 1) return starting;

    NodeVal second = processWithEscapeIfLeafUnlessType(node.getChild(1));
    if (second.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

    KnownVal knownTy = KnownVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);

    if (second.isKnownVal() && KnownVal::isType(second.getKnownVal(), typeTable)) {
        TypeTable::Tuple tup;
        tup.members.reserve(node.getChildrenCnt());
        tup.members.push_back(starting.getKnownVal().ty);
        tup.members.push_back(second.getKnownVal().ty);
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal ty = processAndExpectType(node.getChild(i));
            if (ty.isInvalid()) return NodeVal();
            if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
            tup.members.push_back(ty.getKnownVal().ty);
        }

        optional<TypeTable::Id> tupTypeId = typeTable->addTuple(move(tup));
        if (!tupTypeId.has_value()) {
            msgs->errorInternal(node.getCodeLoc());
            return NodeVal();
        }

        knownTy.ty = tupTypeId.value();
    } else {
        TypeTable::TypeDescr descr(starting.getKnownVal().ty);
        if (!applyTypeDescrDecor(descr, second)) return NodeVal();
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal decor = processWithEscapeIfLeaf(node.getChild(i));
            if (decor.isInvalid()) return NodeVal();
            if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
            if (!applyTypeDescrDecor(descr, decor)) return NodeVal();
        }

        knownTy.ty = typeTable->addTypeDescr(move(descr));
    }

    return NodeVal(node.getCodeLoc(), knownTy);
}

NodeVal Processor::processId(const NodeVal &node) {
    NamePool::Id id = node.getKnownVal().id;
    
    NodeVal *value = symbolTable->getVar(id);
    
    if (value != nullptr) {
        return performLoad(node.getCodeLoc(), id, *value);
    } else if (symbolTable->isFuncName(id) || symbolTable->isMacroName(id) ||
        isKeyword(id) || isOper(id)) {
        // TODO these as first-class values, with ref
        KnownVal known;
        known.id = id;

        return NodeVal(node.getCodeLoc(), known);
    } else if (typeTable->isType(id)) {
        // TODO ref for types
        KnownVal known = KnownVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_TYPE), typeTable);
        known.ty = typeTable->getTypeId(id).value();

        return NodeVal(node.getCodeLoc(), known);
    } else {
        msgs->errorVarNotFound(node.getCodeLoc(), id);
        return NodeVal();
    }
}

NodeVal Processor::processSym(const NodeVal &node) {
    if (!checkAtLeastChildren(node, 2, true)) return NodeVal();

    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        const NodeVal &entry = node.getChild(i);
        // continue with the other sym entries to minimize the number of errors printed out
        if (!entry.isLeaf() && !checkExactlyChildren(entry, 2, true)) continue;

        bool hasInit = !entry.isLeaf();

        const NodeVal &nodePair = hasInit ? entry.getChild(0) : entry;
        pair<NodeVal, optional<NodeVal>> pair = processForIdTypePair(nodePair);
        if (pair.first.isInvalid()) continue;
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        NamePool::Id id = pair.first.getKnownVal().id;
        if (!symbolTable->nameAvailable(id, namePool, typeTable)) {
            msgs->errorVarNameTaken(nodePair.getCodeLoc(), id);
            continue;
        }
        optional<TypeTable::Id> optType;
        if (pair.second.has_value()) optType = pair.second.value().getKnownVal().ty;

        bool hasType = optType.has_value();

        if (hasInit) {
            const NodeVal &nodeInit = entry.getChild(1);
            NodeVal init = hasType ? processAndImplicitCast(nodeInit, optType.value()) : processNode(nodeInit);
            if (init.isInvalid()) continue;
            if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

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

    return NodeVal(node.getCodeLoc());
}

NodeVal Processor::processCast(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) {
        return NodeVal();
    }

    NodeVal ty = processAndExpectType(node.getChild(1));
    if (ty.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

    NodeVal value = processNode(node.getChild(2));
    if (value.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

    return performCast(node.getCodeLoc(), value, ty.getKnownVal().ty);
}

NodeVal Processor::processBlock(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 4, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 3;
    bool hasType = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indType = hasName ? 2 : 1;
    size_t indBody = hasName ? 3 : (hasType ? 2 : 1);

    const NodeVal &nodeBlock = node.getChild(indBody);
    if (!checkIsComposite(nodeBlock, true)) return NodeVal();

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscapeIfLeafAndExpectId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        name = nodeName.getKnownVal().id;
    }

    optional<TypeTable::Id> type;
    if (hasType) {
        NodeVal nodeType = processNode(node.getChild(indType));
        if (nodeType.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        if (!nodeType.isEmpty()) {
            if (!checkIsType(nodeType, true)) return NodeVal();
            type = nodeType.getKnownVal().ty;
        }
    }

    SymbolTable::Block block;
    block.name = name;
    block.type = type;
    if (!performBlockSetUp(node.getCodeLoc(), block)) return NodeVal();

    {
        BlockControl blockCtrl(symbolTable, block);
        if (!processChildNodes(nodeBlock)) {
            performBlockTearDown(node.getCodeLoc(), block, false);
            return NodeVal();
        }
    }

    NodeVal ret = performBlockTearDown(node.getCodeLoc(), block, true);
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
    return ret;
}

NodeVal Processor::processExit(const NodeVal &node) {
    if (!checkBetweenChildren(node, 2, 3, true)) return NodeVal();

    bool hasName = node.getChildrenCnt() > 2;

    size_t indName = 1;
    size_t indCond = hasName ? 2 : 1;

    optional<NamePool::Id> name;
    if (hasName) {
        NodeVal nodeName = processWithEscapeIfLeafAndExpectId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        name = nodeName.getKnownVal().id;
    }

    NodeVal nodeCond = processNode(node.getChild(indCond));
    if (nodeCond.isInvalid() || !checkIsBool(nodeCond, true)) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

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
        NodeVal nodeName = processWithEscapeIfLeafAndExpectId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        name = nodeName.getKnownVal().id;
    }

    NodeVal nodeCond = processNode(node.getChild(indCond));
    if (nodeCond.isInvalid() || !checkIsBool(nodeCond, true)) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

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
        NodeVal nodeName = processWithEscapeIfLeafAndExpectId(node.getChild(indName));
        if (nodeName.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        name = nodeName.getKnownVal().id;
    }

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

NodeVal Processor::processCall(const NodeVal &node, const NodeVal &starting) {
    NamePool::Id name = starting.getKnownVal().getCallableId().value();
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
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

        args.push_back(move(arg));
    }
    
    return performCall(node.getCodeLoc(), *funcVal, args);
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
    NodeVal name = processWithEscapeIfLeafAndExpectId(node.getChild(1));
    if (name.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
    if (!symbolTable->nameAvailable(name.getKnownVal().id, namePool, typeTable)) {
        msgs->errorFuncNameTaken(name.getCodeLoc(), name.getKnownVal().id);
        return NodeVal();
    }
    val.name = name.getKnownVal().id;

    // arguments
    const NodeVal &nodeArgs = node.getChild(2);
    if (!checkIsComposite(nodeArgs, true)) return NodeVal();
    val.variadic = false;
    for (size_t i = 0; i < nodeArgs.getChildrenCnt(); ++i) {
        const NodeVal &nodeArg = nodeArgs.getChild(i);

        if (val.variadic) {
            msgs->errorNotLastParam(nodeArg.getCodeLoc());
            return NodeVal();
        }

        pair<NodeVal, optional<NodeVal>> arg = processForIdTypePair(nodeArg);
        if (arg.first.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        NamePool::Id argId = arg.first.getKnownVal().id;
        if (isMeaningful(argId, Meaningful::ELLIPSIS)) {
            val.variadic = true;
        } else {
            if (!arg.second.has_value()) {
                msgs->errorMissingTypeAttribute(nodeArg.getCodeLoc());
                return NodeVal();
            }
            val.argNames.push_back(argId);
            val.argTypes.push_back(arg.second.value().getKnownVal().ty);
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
    if (!nodeRetType.isEmpty()) {
        NodeVal ty = processAndExpectType(nodeRetType);
        if (ty.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        val.retType = ty.getKnownVal().ty;
    }

    // body
    optional<NodeVal> nodeBodyOpt;
    if (val.defined) {
        nodeBodyOpt = node.getChild(4);
        if (!checkIsComposite(nodeBodyOpt.value(), true)) {
            return NodeVal();
        }
    }

    if (!performFunctionDeclaration(node.getCodeLoc(), val)) return NodeVal();
    symbolTable->registerFunc(val);

    if (val.defined) {
        if (!performFunctionDefinition(nodeArgs, nodeBodyOpt.value(), val)) return NodeVal();
    }

    return NodeVal(node.getCodeLoc());
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
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

        if (!performRet(node.getCodeLoc(), val)) return NodeVal();
    } else {
        if (optFuncValue.value().hasRet()) {
            msgs->errorRetNoValue(node.getCodeLoc(), optFuncValue.value().retType.value());
            return NodeVal();
        }

        if (!performRet(node.getCodeLoc())) return NodeVal();
    }

    return NodeVal(node.getCodeLoc());
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
    if (!file.isKnownVal() || !KnownVal::isStr(file.getKnownVal(), typeTable) ||
        KnownVal::isNull(file.getKnownVal(), typeTable)) {
        msgs->errorImportNotString(file.getCodeLoc());
        return NodeVal();
    }

    return NodeVal(node.getCodeLoc(), file.getKnownVal().str.value());
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
    if (node.getChildrenCnt() == 1) return starting;

    vector<NodeVal> membs;
    membs.reserve(node.getChildrenCnt());
    membs.push_back(starting);
    for (size_t i = 1; i < node.getChildrenCnt(); ++i) {
        NodeVal memb = processNode(node.getChild(i));
        if (memb.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        membs.push_back(move(memb));
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

    return performTuple(node.getCodeLoc(), tupTypeIdOpt.value(), membs);
}

NodeVal Processor::promoteLiteralVal(const NodeVal &node) {
    bool isId = false;

    KnownVal known;
    LiteralVal lit = node.getLiteralVal();
    switch (lit.kind) {
    case LiteralVal::Kind::kId:
        known.type = typeTable->getPrimTypeId(TypeTable::P_ID);
        known.id = lit.val_id;
        isId = true;
        break;
    case LiteralVal::Kind::kSint:
        {
            TypeTable::PrimIds fitting = typeTable->shortestFittingPrimTypeI(lit.val_si);
            TypeTable::PrimIds chosen = max(TypeTable::P_I32, fitting);
            known.type = typeTable->getPrimTypeId(chosen);
            if (chosen == TypeTable::P_I32) known.i32 = lit.val_si;
            else known.i64 = lit.val_si;
            break;
        }
    case LiteralVal::Kind::kFloat:
        {
            TypeTable::PrimIds fitting = typeTable->shortestFittingPrimTypeF(lit.val_f);
            TypeTable::PrimIds chosen = max(TypeTable::P_F32, fitting);
            known.type = typeTable->getPrimTypeId(chosen);
            if (chosen == TypeTable::P_F32) known.f32 = lit.val_f;
            else known.f64 = lit.val_f;
            break;
        }
    case LiteralVal::Kind::kChar:
        known.type = typeTable->getPrimTypeId(TypeTable::P_C8);
        known.c8 = lit.val_c;
        break;
    case LiteralVal::Kind::kBool:
        known.type = typeTable->getPrimTypeId(TypeTable::P_BOOL);
        known.b = lit.val_b;
        break;
    case LiteralVal::Kind::kString:
        known.type = typeTable->getTypeIdStr();
        known.str = lit.val_str;
        break;
    case LiteralVal::Kind::kNull:
        known.type = typeTable->getPrimTypeId(TypeTable::P_PTR);
        break;
    default:
        msgs->errorInternal(node.getCodeLoc());
        return NodeVal();
    }
    NodeVal prom(node.getCodeLoc(), known);

    if (node.isEscaped()) prom.escape();
    
    if (node.hasTypeAttr() && !isId) {
        NodeVal nodeTy = processAndExpectType(node.getTypeAttr());
        if (nodeTy.isInvalid()) return NodeVal();
        TypeTable::Id ty = nodeTy.getKnownVal().ty;

        if (!KnownVal::isImplicitCastable(known, ty, stringPool, typeTable)) {
            msgs->errorExprCannotPromote(node.getCodeLoc(), ty);
            return NodeVal();
        }
        prom = performCast(prom.getCodeLoc(), prom, ty);
    }

    return prom;
}

bool Processor::applyTypeDescrDecor(TypeTable::TypeDescr &descr, const NodeVal &node) {
    if (!node.isKnownVal()) {
        msgs->errorInvalidTypeDecorator(node.getCodeLoc());
        return false;
    }

    if (KnownVal::isId(node.getKnownVal(), typeTable)) {
        optional<Meaningful> mean = getMeaningful(node.getKnownVal().id);
        if (!mean.has_value()) {
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
        optional<uint64_t> arrSize = KnownVal::getValueNonNeg(node.getKnownVal(), typeTable);
        if (!arrSize.has_value() || arrSize.value() == 0) {
            if (arrSize.value() == 0) {
                msgs->errorBadArraySize(node.getCodeLoc(), arrSize.value());
            } else {
                optional<int64_t> integ = KnownVal::getValueI(node.getKnownVal(), typeTable);
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
        rhs = performCast(rhs.getCodeLoc(), rhs, lhs.getType().value());
        return !rhs.isInvalid();
    } else if (!oneWayOnly && checkImplicitCastable(lhs, rhs.getType().value(), false)) {
        lhs = performCast(lhs.getCodeLoc(), lhs, rhs.getType().value());
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
    if (isSkippingProcessing()) return NodeVal(codeLoc);

    return op == Oper::MUL ? performOperUnaryDeref(codeLoc, operProc) : performOperUnary(codeLoc, operProc, op);
}

NodeVal Processor::processOperComparison(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers, Oper op) {
    void *signal = performOperComparisonSetUp(codeLoc, opers.size());
    if (signal == nullptr) return NodeVal();
    DeferredCallback signalDeleteGuard([=] { delete signal; });

    NodeVal lhs = processAndCheckHasType(*opers[0]);
    if (lhs.isInvalid()) return performOperComparisonTearDown(codeLoc, false, signal);
    if (isSkippingProcessing()) {
        performOperComparisonTearDown(codeLoc, true, signal);
        return NodeVal(codeLoc);
    }

    for (size_t i = 1; i < opers.size(); ++i) {
        NodeVal rhs = processAndCheckHasType(*opers[i]);
        if (rhs.isInvalid()) return performOperComparisonTearDown(codeLoc, false, signal);
        if (isSkippingProcessing()) {
            performOperComparisonTearDown(codeLoc, true, signal);
            return NodeVal(codeLoc);
        }

        if (!implicitCastOperands(lhs, rhs, false)) return performOperComparisonTearDown(codeLoc, false, signal);

        optional<bool> compSuccess = performOperComparison(codeLoc, lhs, rhs, op, signal);
        if (!compSuccess.has_value()) return performOperComparisonTearDown(codeLoc, false, signal);
        if (compSuccess.value()) break;

        lhs = move(rhs);
    }

    return performOperComparisonTearDown(codeLoc, true, signal);
}

NodeVal Processor::processOperAssignment(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal rhs = processAndCheckHasType(*opers.back());
    if (rhs.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(codeLoc);

    for (size_t i = opers.size()-2;; --i) {
        NodeVal lhs = processAndCheckHasType(*opers[i]);
        if (lhs.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(codeLoc);

        if (!lhs.hasRef()) {
            msgs->errorExprAsgnNonRef(lhs.getCodeLoc());
            return NodeVal();
        }
        if (typeTable->worksAsTypeCn(lhs.getType().value())) {
            msgs->errorExprAsgnOnCn(lhs.getCodeLoc());
            return NodeVal();
        }

        if (!implicitCastOperands(lhs, rhs, true)) return NodeVal();

        rhs = performOperAssignment(codeLoc, lhs, rhs);
        if (rhs.isInvalid()) return NodeVal();

        if (i == 0) break;
    }

    return rhs;
}

NodeVal Processor::processOperIndex(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal base = processAndCheckHasType(*opers[0]);
    if (base.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(codeLoc);

    for (size_t i = 1; i < opers.size(); ++i) {
        TypeTable::Id baseType = base.getType().value();

        optional<TypeTable::Id> elemType = typeTable->addTypeIndexOf(baseType);
        if (!elemType.has_value()) {
            msgs->errorExprIndexOnBadType(base.getCodeLoc(), baseType);
            return NodeVal();
        }

        NodeVal index = processAndCheckHasType(*opers[i]);
        if (index.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(codeLoc);
        if (!typeTable->worksAsTypeI(index.getType().value()) && !typeTable->worksAsTypeU(index.getType().value())) {
            msgs->errorExprIndexNotIntegral(index.getCodeLoc());
            return NodeVal();
        }

        if (index.isKnownVal() && typeTable->worksAsTypeArr(baseType)) {
            if (KnownVal::isI(index.getKnownVal(), typeTable)) {
                int64_t ind = KnownVal::getValueI(index.getKnownVal(), typeTable).value();
                size_t len = typeTable->extractLenOfArr(baseType).value();
                if (ind < 0 || (size_t) ind >= len) {
                    msgs->warnExprIndexOutOfBounds(index.getCodeLoc());
                }
            } else if (KnownVal::isU(index.getKnownVal(), typeTable)) {
                uint64_t ind = KnownVal::getValueU(index.getKnownVal(), typeTable).value();
                size_t len = typeTable->extractLenOfArr(baseType).value();
                if (ind >= len) {
                    msgs->warnExprIndexOutOfBounds(index.getCodeLoc());
                }
            }
        }

        base = performOperIndex(base.getCodeLoc(), base, index, elemType.value());
        if (base.isInvalid()) return NodeVal();
    }

    return base;
}

NodeVal Processor::processOperMember(CodeLoc codeLoc, const std::vector<const NodeVal*> &opers) {
    NodeVal base = processAndCheckHasType(*opers[0]);
    if (base.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(codeLoc);

    for (size_t i = 1; i < opers.size(); ++i) {
        if (typeTable->worksAsTypeP(base.getType().value())) {
            base = performOperUnaryDeref(base.getCodeLoc(), base);
            if (base.isInvalid()) return NodeVal();
            if (!checkHasType(base, true)) return NodeVal();
        }
        TypeTable::Id baseType = base.getType().value();

        NodeVal index = processNode(*opers[i]);
        if (index.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(codeLoc);
        if (!index.isKnownVal()) {
            msgs->errorMemberIndex(index.getCodeLoc());
            return NodeVal();
        }

        optional<const TypeTable::Tuple*> tupleOpt = typeTable->extractTuple(baseType);
        if (!tupleOpt.has_value()) {
            msgs->errorExprDotInvalidBase(base.getCodeLoc());
            return NodeVal();
        }
        const TypeTable::Tuple &tuple = *tupleOpt.value();

        optional<uint64_t> indexValOpt = KnownVal::getValueNonNeg(index.getKnownVal(), typeTable);
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

        base = performOperMember(index.getCodeLoc(), base, indexVal, resType.value());
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
    if (isSkippingProcessing()) return NodeVal(codeLoc);

    for (size_t i = 1; i < opers.size(); ++i) {
        NodeVal rhs = processAndCheckHasType(*opers[i]);
        if (rhs.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(codeLoc);

        if (!implicitCastOperands(lhs, rhs, false)) return NodeVal();

        lhs = performOperRegular(codeLoc, lhs, rhs, op);
        if (lhs.isInvalid()) return NodeVal();
    }

    return lhs;
}

NodeVal Processor::processAndExpectType(const NodeVal &node) {
    NodeVal ty = processNode(node);
    if (ty.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
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

NodeVal Processor::processWithEscapeIfLeafAndExpectId(const NodeVal &node) {
    NodeVal id = processWithEscapeIfLeaf(node);
    if (id.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
    if (!checkIsId(id, true)) return NodeVal();
    return id;
}

NodeVal Processor::processWithEscapeIfLeafUnlessType(const NodeVal &node) {
    if (node.isLeaf() && !node.isEscaped()) {
        NodeVal esc = processWithEscapeIfLeaf(node);
        if (esc.isInvalid()) return NodeVal();
        if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
        if (esc.isKnownVal() && KnownVal::isId(esc.getKnownVal(), typeTable) &&
            typeTable->isType(esc.getKnownVal().id)) {
            esc.unescape();
            return processNode(esc);
        } else {
            return esc;
        }
    } else {
        return processNode(node);
    }
}

pair<NodeVal, optional<NodeVal>> Processor::processForIdTypePair(const NodeVal &node) {
    const pair<NodeVal, optional<NodeVal>> retInvalid = make_pair<NodeVal, optional<NodeVal>>(NodeVal(), nullopt);
    const pair<NodeVal, optional<NodeVal>> retSkipping = make_pair<NodeVal, optional<NodeVal>>(NodeVal(node.getCodeLoc()), nullopt);
    
    NodeVal id = node;
    id.clearTypeAttr();
    id = processWithEscapeIfLeafAndExpectId(id);
    if (id.isInvalid()) return retInvalid;
    if (isSkippingProcessing()) return retSkipping;

    optional<NodeVal> ty;
    if (node.hasTypeAttr()) {
        ty = processAndExpectType(node.getTypeAttr());
        if (ty.value().isInvalid()) return retInvalid;
        if (isSkippingProcessing()) return retSkipping;
    }

    return make_pair<NodeVal, optional<NodeVal>>(move(id), move(ty));
}

NodeVal Processor::processAndCheckHasType(const NodeVal &node) {
    NodeVal proc = processNode(node);
    if (proc.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());
    if (!checkHasType(proc, true)) return NodeVal();
    return proc;
}

NodeVal Processor::processAndImplicitCast(const NodeVal &node, TypeTable::Id ty) {
    NodeVal proc = processNode(node);
    if (proc.isInvalid()) return NodeVal();
    if (isSkippingProcessing()) return NodeVal(node.getCodeLoc());

    if (!checkImplicitCastable(proc, ty, true)) return NodeVal();
    return performCast(proc.getCodeLoc(), proc, ty);
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

bool Processor::checkIsTopmost(CodeLoc codeLoc, bool orError) {
    if (topmost != 1) {
        if (orError) msgs->errorNotTopmost(codeLoc);
        return false;
    }
    return true;
}

bool Processor::checkIsId(const NodeVal &node, bool orError) {
    if (!node.isKnownVal() || !KnownVal::isId(node.getKnownVal(), typeTable)) {
        if (orError) msgs->errorUnexpectedNotId(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsType(const NodeVal &node, bool orError) {
    if (!node.isKnownVal() || !KnownVal::isType(node.getKnownVal(), typeTable)) {
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

bool Processor::checkIsComposite(const NodeVal &node, bool orError) {
    if (!node.isComposite()) {
        if (orError) msgs->errorUnexpectedIsTerminal(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkIsValue(const NodeVal &node, bool orError) {
    if (!node.isKnownVal() && !node.isLlvmVal()) {
        if (orError) msgs->errorUnknown(node.getCodeLoc());
        return false;
    }
    return true;
}

bool Processor::checkExactlyChildren(const NodeVal &node, std::size_t n, bool orError) {
    if (!node.isComposite() || node.getChildrenCnt() != n) {
        if (orError) msgs->errorChildrenNotEq(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkAtLeastChildren(const NodeVal &node, std::size_t n, bool orError) {
    if (!node.isComposite() || node.getChildrenCnt() < n) {
        if (orError) msgs->errorChildrenLessThan(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkAtMostChildren(const NodeVal &node, std::size_t n, bool orError) {
    if (!node.isComposite() || node.getChildrenCnt() > n) {
        if (orError) msgs->errorChildrenMoreThan(node.getCodeLoc(), n);
        return false;
    }
    return true;
}

bool Processor::checkBetweenChildren(const NodeVal &node, std::size_t nLo, std::size_t nHi, bool orError) {
    if (!node.isComposite() || !between(node.getChildrenCnt(), nLo, nHi)) {
        if (orError) msgs->errorChildrenNotBetween(node.getCodeLoc(), nLo, nHi);
        return false;
    }
    return true;
}

bool Processor::checkImplicitCastable(const NodeVal &node, TypeTable::Id ty, bool orError) {
    if (!checkHasType(node, true)) return false;
    TypeTable::Id nodeTy = node.getType().value();
    if (node.isKnownVal()) {
        if (!KnownVal::isImplicitCastable(node.getKnownVal(), ty, stringPool, typeTable)) {
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