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
        if (!applyTypeDescrDecorOrFalse(descr, second)) return NodeVal();
        for (size_t i = 2; i < node.getChildrenCnt(); ++i) {
            NodeVal decor = processWithEscapeIfLeaf(node.getChild(i));
            if (decor.isInvalid()) return NodeVal();
            if (!applyTypeDescrDecorOrFalse(descr, decor)) return NodeVal();
        }

        knownTy.ty = typeTable->addTypeDescr(move(descr));
    }

    return NodeVal(node.getCodeLoc(), knownTy);
}

NodeVal Processor::processCall(const NodeVal &node, const NodeVal &starting) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processId(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processSym(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processCast(const NodeVal &node) {
    return NodeVal(); // TODO!
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

NodeVal Processor::processFnc(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processRet(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processMac(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processEval(const NodeVal &node) {
    return NodeVal(); // TODO!
}

NodeVal Processor::processImport(const NodeVal &node) {
    return NodeVal(); // TODO!
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
        prom = cast(prom, ty);
    }

    return prom;
}

NodeVal Processor::processAndExpectType(const NodeVal &node) {
    NodeVal ty = processNode(node);
    if (ty.isInvalid()) return NodeVal();
    if (!ty.isKnownVal() || !KnownVal::isType(ty.getKnownVal(), typeTable)) {
        msgs->errorUnexpectedNotType(node.getCodeLoc());
        return NodeVal();
    }
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

bool Processor::applyTypeDescrDecorOrFalse(TypeTable::TypeDescr &descr, const NodeVal &node) {
    if (!node.isKnownVal()) return false;

    if (KnownVal::isId(node.getKnownVal(), typeTable)) {
        optional<Meaningful> mean = getMeaningful(node.getKnownVal().id);
        if (!mean.has_value()) return false;

        if (mean == Meaningful::CN)
            descr.setLastCn();
        else if (mean == Meaningful::ASTERISK)
            descr.addDecor({.type=TypeTable::TypeDescr::Decor::D_PTR});
        else if (mean == Meaningful::SQUARE)
            descr.addDecor({.type=TypeTable::TypeDescr::Decor::D_ARR_PTR});
        else
            return false;
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