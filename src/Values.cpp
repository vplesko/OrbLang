#include "Values.h"
using namespace std;

NodeVal::NodeVal(Kind k) : kind(k) {
    switch (k) {
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