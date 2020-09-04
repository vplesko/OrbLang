#include "Processor.h"
#include "Reserved.h"
using namespace std;

Processor::Processor(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs)
    : namePool(namePool), stringPool(stringPool), typeTable(typeTable), symbolTable(symbolTable), msgs(msgs) {
}

NodeVal Processor::processNode(const NodeVal &node) {
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
    return NodeVal(); // TODO!
}

NodeVal Processor::processType(const NodeVal &node, const NodeVal &starting) {
    if (node.getLength() == 1) return starting;

    NodeVal second = processWithEscapeIfLeafUnlessType(node.getChild(1));

    KnownVal knownTy(typeTable->getPrimTypeId(TypeTable::P_TYPE));

    if (second.isKnownVal() && KnownVal::isType(second.getKnownVal(), typeTable)) {
        TypeTable::Tuple tup;
        tup.members.resize(node.getChildrenCnt());
        tup.members[0] = starting.getKnownVal().ty;
        tup.members[1] = second.getKnownVal().ty;
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal ty = processAndExpectType(node.getChild(i));
            if (ty.isInvalid()) return NodeVal();
            tup.members[i] = ty.getKnownVal().ty;
        }

        optional<TypeTable::Id> tupTypeId = typeTable->addTuple(tup);
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
            if (!applyTypeDescrDecor(descr, decor)) return NodeVal();
        }

        knownTy.ty = typeTable->addTypeDescr(move(descr));
    }

    return NodeVal(node.getCodeLoc(), knownTy);
}

NodeVal Processor::processId(const NodeVal &node) {
    NamePool::Id id = node.getKnownVal().id;

    if (symbolTable->isFuncName(id) || symbolTable->isMacroName(id) ||
        isKeyword(id) || isOper(id)) {
        // TODO these as first-class values, with ref
        KnownVal known;
        known.id = id;

        return NodeVal(node.getCodeLoc(), known);
    } else if (typeTable->isType(id)) {
        // TODO ref for types
        KnownVal known(typeTable->getPrimTypeId(TypeTable::P_TYPE));
        known.ty = typeTable->getTypeId(id).value();

        return NodeVal(node.getCodeLoc(), known);
    } else {
        NodeVal *value = symbolTable->getVar(id);
        if (value == nullptr) {
            msgs->errorVarNotFound(node.getCodeLoc(), id);
            return NodeVal();
        }

        if (value->isKnownVal()) {
            return NodeVal(node.getCodeLoc(), value->getKnownVal());
        } else {
            return performLoad(node.getCodeLoc(), id);
        }
    }
}

NodeVal Processor::processSym(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processCast(const NodeVal &node) {
    if (!checkExactlyChildren(node, 3, true)) {
        return NodeVal();
    }

    NodeVal ty = processAndExpectType(node.getChild(1));
    if (ty.isInvalid()) return NodeVal();

    NodeVal value = processNode(node.getChild(2));
    if (value.isInvalid()) return NodeVal();

    return performCast(node.getCodeLoc(), value, ty.getKnownVal().ty);
}

NodeVal Processor::processBlock(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processExit(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processLoop(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processPass(const NodeVal &node) {
    return NodeVal(); // TODO!
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
        NodeVal arg = processNode(node.getChild(i+1));
        if (arg.isInvalid()) return NodeVal();

        if (i < funcVal->argCnt()) {
            TypeTable::Id argCastType = funcVal->argTypes[i];
            if (!checkImplicitCastable(arg, argCastType, true)) return NodeVal();
            arg = performCast(arg.getCodeLoc(), arg, argCastType);
            if (arg.isInvalid()) return NodeVal();
        }
        args.push_back(move(arg));
    }
    
    return performCall(node.getCodeLoc(), *funcVal, args);
}

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
        NamePool::Id argId = arg.first.getKnownVal().id;
        if (isMeaningful(argId, Meaningful::ELLIPSIS)) {
            val.variadic = true;
        } else {
            if (!arg.second.has_value()) {
                msgs->errorMissingTypeAnnotation(nodeArg.getCodeLoc());
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
        val.retType = ty.getKnownVal().ty;
    }

    // body
    optional<NodeVal> nodeBodyOpt;
    if (val.defined) {
        nodeBodyOpt = node.getChild(4);
        if (!nodeBodyOpt.value().isComposite()) {
            msgs->errorUnexpectedIsTerminal(nodeBodyOpt.value().getCodeLoc());
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
    return NodeVal(); // TODO!
}

NodeVal Processor::processMac(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processEval(const NodeVal &node) {
    if (!checkExactlyChildren(node, 2, true)) {
        return NodeVal();
    }

    return performEvaluation(node.getChild(1));
}

NodeVal Processor::processImport(const NodeVal &node) {
    if (!checkInGlobalScope(node.getCodeLoc(), true) ||
        !checkExactlyChildren(node, 2, true)) {
        return NodeVal();
    }

    NodeVal file = processNode(node.getChild(1));
    if (file.isInvalid()) return NodeVal();
    if (!file.isKnownVal() || !KnownVal::isStr(file.getKnownVal(), typeTable) ||
        KnownVal::isNull(file.getKnownVal(), typeTable)) {
        msgs->errorImportNotString(file.getCodeLoc());
        return NodeVal();
    }

    return NodeVal(node.getCodeLoc(), file.getKnownVal().str.value());
}

NodeVal Processor::processOper(const NodeVal &node, Oper op) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processTuple(const NodeVal &node, const NodeVal &starting) {
    return NodeVal(); // TODO!
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
    
    if (node.hasTypeAnnot() && !isId) {
        NodeVal nodeTy = processAndExpectType(node.getTypeAnnot());
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

bool Processor::processChildNodes(const NodeVal &node) {
    for (size_t i = 0; i < node.getChildrenCnt(); ++i) {
        NodeVal tmp = processNode(node.getChild(i));
        if (tmp.isInvalid()) return false;
    }

    return true;
}

NodeVal Processor::processAndExpectType(const NodeVal &node) {
    NodeVal ty = processNode(node);
    if (ty.isInvalid()) return NodeVal();
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
    if (!checkIsId(id, true)) return NodeVal();
    return id;
}

NodeVal Processor::processWithEscapeIfLeafUnlessType(const NodeVal &node) {
    if (node.isLeaf() && !node.isEscaped()) {
        NodeVal esc = processWithEscapeIfLeaf(node);
        if (esc.isInvalid()) return NodeVal();
        if (esc.isKnownVal() && KnownVal::isId(esc.getKnownVal(), typeTable) &&
            typeTable->isType(esc.getKnownVal().id)) {
            return processNode(esc);
        } else {
            return esc;
        }
    } else {
        return processNode(node);
    }
}

pair<NodeVal, optional<NodeVal>> Processor::processForIdTypePair(const NodeVal &node) {
    const pair<NodeVal, optional<NodeVal>> invalidRet = make_pair<NodeVal, optional<NodeVal>>(NodeVal(), nullopt);
    
    NodeVal id = processWithEscapeIfLeafAndExpectId(node);
    if (id.isInvalid()) return invalidRet;

    optional<NodeVal> ty;
    if (node.hasTypeAnnot()) {
        ty = processAndExpectType(node.getTypeAnnot());
        if (ty.value().isInvalid()) return invalidRet;
    }

    return make_pair<NodeVal, optional<NodeVal>>(move(id), move(ty));
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

bool Processor::checkInGlobalScope(CodeLoc codeLoc, bool orError) {
    if (!symbolTable->inGlobalScope()) {
        if (orError) msgs->errorNotGlobalScope(codeLoc);
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

bool Processor::checkIsComposite(const NodeVal &node, bool orError) {
    if (!node.isComposite()) {
        if (orError) msgs->errorUnexpectedIsTerminal(node.getCodeLoc());
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
    optional<TypeTable::Id> nodeTy = node.getType();
    if (!nodeTy.has_value()) {
        if (orError) msgs->errorUnknown(node.getCodeLoc());
        return false;
    }
    if (node.isKnownVal()) {
        if (!KnownVal::isImplicitCastable(node.getKnownVal(), ty, stringPool, typeTable)) {
            if (orError) msgs->errorExprCannotImplicitCast(node.getCodeLoc(), nodeTy.value(), ty);
            return false;
        }
    } else {
        if (!typeTable->isImplicitCastable(nodeTy.value(), ty)) {
            if (orError) msgs->errorExprCannotImplicitCast(node.getCodeLoc(), nodeTy.value(), ty);
            return false;
        }
    }
    return true;
}