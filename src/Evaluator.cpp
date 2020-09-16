#include "Evaluator.h"
using namespace std;

Evaluator::Evaluator(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, CompileMessages *msgs)
    : Processor(namePool, stringPool, typeTable, symbolTable, msgs) {
}

NodeVal Evaluator::performCast(CodeLoc codeLoc, const NodeVal &node, TypeTable::Id ty) {
    if (node.getType().has_value() && node.getType().value() == ty) {
        // TODO this may give back a ref value, is that wanted? what about other similar cases?
        return node;
    }

    if (!checkIsKnownVal(node, true)) return NodeVal();

    // TODO remove when KnownVal's type no longer optional
    if (!node.getKnownVal().type.has_value()) {
        msgs->errorUnknown(node.getCodeLoc());
        return NodeVal();
    }

    optional<KnownVal> knownValCast = makeCast(node.getKnownVal(), node.getKnownVal().type.value(), ty);
    if (!knownValCast.has_value()) {
        msgs->errorExprCannotCast(codeLoc, node.getKnownVal().type.value(), ty);
        return NodeVal();
    }

    return NodeVal(codeLoc, knownValCast.value());
}

NodeVal Evaluator::performEvaluation(const NodeVal &node) {
    return processNode(node);
}

bool Evaluator::checkIsKnownVal(const NodeVal &node, bool orError) {
    if (!node.isKnownVal()) {
        if (orError) msgs->errorUnknown(node.getCodeLoc());
        return false;
    }
    return true;
}

// TODO warn on lossy
optional<KnownVal> Evaluator::makeCast(const KnownVal &srcKnownVal, TypeTable::Id srcTypeId, TypeTable::Id dstTypeId) {
    // TODO! may pass ref value
    if (srcTypeId == dstTypeId) return srcKnownVal;

    optional<KnownVal> dstKnownVal = KnownVal(dstTypeId);

    if (typeTable->worksAsTypeI(srcTypeId)) {
        int64_t x = KnownVal::getValueI(srcKnownVal, typeTable).value();
        if (!assignBasedOnTypeI(dstKnownVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstKnownVal.value(), (double) x, dstTypeId) &&
            !assignBasedOnTypeC(dstKnownVal.value(), (char) x, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), (bool) x, dstTypeId) &&
            !(typeTable->worksAsTypeStr(dstTypeId) && x == 0) &&
            !(typeTable->worksAsTypeAnyP(dstTypeId) && x == 0)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeU(srcTypeId)) {
        uint64_t x = KnownVal::getValueU(srcKnownVal, typeTable).value();
        if (!assignBasedOnTypeI(dstKnownVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstKnownVal.value(), (double) x, dstTypeId) &&
            !assignBasedOnTypeC(dstKnownVal.value(), (char) x, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), (bool) x, dstTypeId) &&
            !(typeTable->worksAsTypeStr(dstTypeId) && x == 0) &&
            !(typeTable->worksAsTypeAnyP(dstTypeId) && x == 0)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeF(srcTypeId)) {
        double x = KnownVal::getValueF(srcKnownVal, typeTable).value();
        if (!assignBasedOnTypeI(dstKnownVal.value(), (int64_t) x, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), (uint64_t) x, dstTypeId) &&
            !assignBasedOnTypeF(dstKnownVal.value(), (double) x, dstTypeId)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeC(srcTypeId)) {
        if (!assignBasedOnTypeI(dstKnownVal.value(), (int64_t) srcKnownVal.c8, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), (uint64_t) srcKnownVal.c8, dstTypeId) &&
            !assignBasedOnTypeC(dstKnownVal.value(), srcKnownVal.c8, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), (bool) srcKnownVal.c8, dstTypeId)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeB(srcTypeId)) {
        if (!assignBasedOnTypeI(dstKnownVal.value(), srcKnownVal.b ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), srcKnownVal.b ? 1 : 0, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), srcKnownVal.b, dstTypeId)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeStr(srcTypeId)) {
        if (srcKnownVal.str.has_value()) {
            const string &str = stringPool->get(srcKnownVal.str.value());
            if (typeTable->worksAsTypeStr(dstTypeId)) {
                dstKnownVal.value().str = srcKnownVal.str;
            } else if (typeTable->worksAsTypeB(dstTypeId)) {
                dstKnownVal.value().b = true;
            } else if (typeTable->worksAsTypeCharArrOfLen(dstTypeId, LiteralVal::getStringLen(str))) {
                dstKnownVal = makeArray(dstTypeId);
                if (dstKnownVal.has_value()) {
                    for (size_t i = 0; i < LiteralVal::getStringLen(str); ++i) {
                        dstKnownVal.value().elems[i].c8 = str[i];
                    }
                }
            } else {
                dstKnownVal.reset();
            }
        } else {
            if (!assignBasedOnTypeI(dstKnownVal.value(), 0, dstTypeId) &&
                !assignBasedOnTypeU(dstKnownVal.value(), 0, dstTypeId) &&
                !assignBasedOnTypeB(dstKnownVal.value(), false, dstTypeId) &&
                !typeTable->worksAsTypeAnyP(dstTypeId)) {
                dstKnownVal.reset();
            }
        }
    } else if (typeTable->worksAsTypeAnyP(srcTypeId)) {
        if (!assignBasedOnTypeI(dstKnownVal.value(), 0, dstTypeId) &&
            !assignBasedOnTypeU(dstKnownVal.value(), 0, dstTypeId) &&
            !assignBasedOnTypeB(dstKnownVal.value(), false, dstTypeId) &&
            !typeTable->worksAsTypeAnyP(dstTypeId)) {
            dstKnownVal.reset();
        }
    } else if (typeTable->worksAsTypeArr(srcTypeId) || typeTable->worksAsTuple(srcTypeId) ||
        typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_ID) || typeTable->worksAsPrimitive(srcTypeId, TypeTable::P_TYPE)) {
        // these types are only castable when changing constness
        if (typeTable->isImplicitCastable(srcTypeId, dstTypeId)) {
            // no action is needed in case of a cast
            // TODO! may pass ref value
            dstKnownVal = srcKnownVal;
        } else {
            dstKnownVal.reset();
        }
    } else {
        dstKnownVal.reset();
    }

    return dstKnownVal;
}

optional<KnownVal> Evaluator::makeArray(TypeTable::Id arrTypeId) {
    optional<TypeTable::Id> elemTypeId = typeTable->addTypeIndexOf(arrTypeId);
    if (!elemTypeId.has_value()) return nullopt;

    optional<size_t> len = typeTable->extractLenOfArr(arrTypeId);
    if (!len.has_value()) return nullopt;

    KnownVal knownVal(arrTypeId);
    KnownVal knownValElem(elemTypeId.value());
    knownVal.elems = vector<KnownVal>(len.value(), knownValElem);

    return knownVal;
}

bool Evaluator::assignBasedOnTypeI(KnownVal &val, int64_t x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_I8)) {
        val.i8 = (int8_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I16)) {
        val.i16 = (int16_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I32)) {
        val.i32 = (int32_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_I64)) {
        val.i64 = (int64_t) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeU(KnownVal &val, uint64_t x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_U8)) {
        val.u8 = (uint8_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U16)) {
        val.u16 = (uint16_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U32)) {
        val.u32 = (uint32_t) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_U64)) {
        val.u64 = (uint64_t) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeF(KnownVal &val, double x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_F32)) {
        val.f32 = (float) x;
    } else if (typeTable->worksAsPrimitive(ty, TypeTable::P_F64)) {
        val.f64 = (double) x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeC(KnownVal &val, char x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_C8)) {
        val.c8 = x;
    } else {
        return false;
    }

    return true;
}

bool Evaluator::assignBasedOnTypeB(KnownVal &val, bool x, TypeTable::Id ty) {
    if (typeTable->worksAsPrimitive(ty, TypeTable::P_BOOL)) {
        val.b = x;
    } else {
        return false;
    }

    return true;
}