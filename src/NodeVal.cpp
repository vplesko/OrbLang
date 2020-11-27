#include "NodeVal.h"
using namespace std;

NodeVal::NodeVal(bool valid) : kind(valid ? Kind::kValid : Kind::kInvalid) {
}

NodeVal::NodeVal(CodeLoc codeLoc, StringPool::Id import) : codeLoc(codeLoc), kind(Kind::kImport), importFile(import) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const LiteralVal &val) : codeLoc(codeLoc), kind(Kind::kLiteral), literal(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const EvalVal &val) : codeLoc(codeLoc), kind(Kind::kEval), eval(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const LlvmVal &val) : codeLoc(codeLoc), kind(Kind::kLlvm), llvm(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const RawVal &val) : codeLoc(codeLoc), kind(Kind::kRaw), raw(val) {
}

void NodeVal::copyFrom(const NodeVal &other) {
    codeLoc = other.codeLoc;
    kind = other.kind;
    importFile = other.importFile;
    literal = other.literal;
    eval = other.eval;
    llvm = other.llvm;
    raw = other.raw;
    
    if (other.hasTypeAttr()) {
        typeAttr = make_unique<NodeVal>(*other.typeAttr);
    }

    escaped = other.escaped;
}

NodeVal::NodeVal(const NodeVal &other) : NodeVal() {
    copyFrom(other);
}

NodeVal& NodeVal::operator=(const NodeVal &other) {
    if (this != &other) {
        copyFrom(other);
    }
    return *this;
}

optional<TypeTable::Id> NodeVal::getType() const {
    if (isEvalVal()) return getEvalVal().getType();
    if (isLlvmVal()) return getLlvmVal().type;
    return nullopt;
}

bool NodeVal::hasRef() const {
    if (isEvalVal()) return getEvalVal().ref != nullptr;
    if (isLlvmVal()) return getLlvmVal().ref != nullptr;
    if (isRawVal()) return raw.ref != nullptr;
    return false;
}

std::size_t NodeVal::getLength() const {
    if (isInvalid()) return 0;
    if (isRawVal()) return raw.getChildrenCnt();
    return 1;
}

void NodeVal::setTypeAttr(NodeVal t) {
    typeAttr = make_unique<NodeVal>(move(t));
}

void NodeVal::escape() {
    escaped = true;
    if (isRawVal()) {
        for (auto &child : raw.children) {
            child.escape();
        }
    }
}

void NodeVal::unescape() {
    if (isRawVal()) {
        for (auto it = raw.children.rbegin(); it != raw.children.rend(); ++it) {
            (*it).unescape();
        }
    }
    escaped = false;
}