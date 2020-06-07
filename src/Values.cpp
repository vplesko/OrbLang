#include "Values.h"
using namespace std;

NodeVal::NodeVal(Kind k) : kind(k) {
    switch (k) {
    case Kind::kKeyword:
        keyword = Token::T_UNKNOWN;
        break;
    case Kind::kLlvmVal:
        llvmVal = LlvmVal();
        break;
    case Kind::kUntyVal:
        untyVal.val.kind = LiteralVal::Kind::kNone;
        break;
    default:
        // do nothing
        break;
    };
}

bool isImplicitCastable(const UntypedVal &val, TypeTable::Id t, const StringPool *stringPool, const TypeTable *typeTable) {
    switch (val.val.kind) {
    case LiteralVal::Kind::kBool:
        return typeTable->worksAsTypeB(t);
    case LiteralVal::Kind::kSint:
        return (typeTable->worksAsTypeI(t) || typeTable->worksAsTypeU(t)) && typeTable->fitsType(val.val.val_si, t);
    case LiteralVal::Kind::kChar:
        return typeTable->worksAsTypeC(t);
    case LiteralVal::Kind::kFloat:
        // no precision checks for float types, this makes float literals somewhat unsafe
        return typeTable->worksAsTypeF(t);
    case LiteralVal::Kind::kString:
        return typeTable->worksAsTypeStr(t) ||
            typeTable->worksAsTypeCharArrOfLen(t, LiteralVal::getStringLen(stringPool->get(val.val.val_str)));
    case LiteralVal::Kind::kNull:
        return typeTable->worksAsTypeAnyP(t);
    default:
        return false;
    }
}