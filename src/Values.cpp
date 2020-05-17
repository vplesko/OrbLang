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
        untyVal.kind = UntypedVal::Kind::kNone;
        break;
    default:
        // do nothing
        break;
    };
}