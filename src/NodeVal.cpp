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

void NodeVal::copyFrom(const NodeVal &other) {
    codeLoc = other.codeLoc;
    kind = other.kind;
    importFile = other.importFile;
    literal = other.literal;
    eval = other.eval;
    llvm = other.llvm;
    
    if (other.hasTypeAttr()) {
        typeAttr = make_unique<NodeVal>(*other.typeAttr);
    }
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
    return false;
}

void NodeVal::setTypeAttr(NodeVal t) {
    typeAttr = make_unique<NodeVal>(move(t));
}

size_t NodeVal::getLength(const NodeVal &node, const TypeTable *typeTable) {
    if (node.isInvalid()) return 0;
    if (isRawVal(node, typeTable)) return node.eval.raw.getChildrenCnt();
    return 1;
}

bool NodeVal::isEmpty(const NodeVal &node, const TypeTable *typeTable) {
    return isRawVal(node, typeTable) && node.eval.raw.isEmpty();
}

bool NodeVal::isLeaf(const NodeVal &node, const TypeTable *typeTable) {
    return !isRawVal(node, typeTable) || node.eval.raw.isEmpty();
}

bool NodeVal::isRawVal(const NodeVal &node, const TypeTable *typeTable) {
    return node.isEvalVal() && EvalVal::isRaw(node.getEvalVal(), typeTable);
}

RawVal& NodeVal::getRawVal(NodeVal &node) {
    return node.getEvalVal().raw;
}

const RawVal& NodeVal::getRawVal(const NodeVal &node) {
    return node.getEvalVal().raw;
}

bool NodeVal::isEscaped(const NodeVal &node, const TypeTable *typeTable) {
    return (node.isLiteralVal() && node.getLiteralVal().escaped) ||
        (node.isEvalVal() && node.getEvalVal().isEscaped());
}

void NodeVal::escape(NodeVal &node, const TypeTable *typeTable) {
    if (node.isLiteralVal()) {
        node.getLiteralVal().escaped = true;
    } else if (node.isEvalVal()) {
        node.getEvalVal().escaped = true;
        if (isRawVal(node, typeTable)) {
            for (auto &child : getRawVal(node).children) {
                escape(child, typeTable);
            }
        }
    }
}

void NodeVal::unescape(NodeVal &node, const TypeTable *typeTable) {
    if (node.isLiteralVal()) {
        node.getLiteralVal().escaped = false;
    } else if (node.isEvalVal()) {
        if (isRawVal(node, typeTable)) {
            for (auto it = getRawVal(node).children.rbegin(); it != getRawVal(node).children.rend(); ++it) {
                unescape(*it, typeTable);
            }
        }
        node.getEvalVal().escaped = false;
    }
}