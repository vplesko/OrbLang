#include "NodeVal.h"
using namespace std;

NodeVal::NodeVal() : kind(Kind::kInvalid) {
}

NodeVal::NodeVal(CodeLoc codeLoc) : codeLoc(codeLoc), kind(Kind::kComposite) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const LiteralVal &val) : codeLoc(codeLoc), kind(Kind::kLiteral), literal(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const KnownVal &val) : codeLoc(codeLoc), kind(Kind::kKnown), known(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const LlvmVal &val) : codeLoc(codeLoc), kind(Kind::kLlvm), llvm(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const std::vector<NodeVal> &nodes) : codeLoc(codeLoc), kind(Kind::kComposite), children(nodes) {
}

std::size_t NodeVal::getLength() const {
    if (isInvalid()) return 0;
    if (isComposite()) return getChildrenCnt();
    return 1;
}

void NodeVal::escape() {
    isEscaped = true;
    if (isComposite()) {
        for (NodeVal &child : getChildren()) {
            child.escape();
        }
    }
}