#include "NodeVal.h"
using namespace std;

NodeVal::NodeVal() : kind(Kind::kInvalid) {
}

NodeVal::NodeVal(CodeLoc codeLoc) : codeLoc(codeLoc), kind(Kind::kValid) {
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
        typeAttr = make_unique<NodeVal>(other.getTypeAttr());
    }
    if (other.hasAttrs()) {
        attrs = make_unique<NodeVal>(other.getAttrs());
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

bool NodeVal::isEscaped() const {
    return (isLiteralVal() && getLiteralVal().isEscaped()) ||
        (isEvalVal() && getEvalVal().isEscaped());
}

EscapeScore NodeVal::getEscapeScore() const {
    if (isLiteralVal()) return getLiteralVal().escapeScore;
    if (isEvalVal()) return getEvalVal().escapeScore;
    return 0;
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

void NodeVal::addChild(NodeVal c) {
    getEvalVal().elems.push_back(move(c));
}

void NodeVal::addChildren(std::vector<NodeVal> c) {
    getEvalVal().elems.reserve(getEvalVal().elems.size()+c.size());
    for (auto &it : c) {
        addChild(move(it));
    }
}

void NodeVal::setTypeAttr(NodeVal t) {
    typeAttr = make_unique<NodeVal>(move(t));
}

void NodeVal::setAttrs(NodeVal a) {
    attrs = make_unique<NodeVal>(move(a));
}

bool NodeVal::isEmpty(const NodeVal &node, const TypeTable *typeTable) {
    return isRawVal(node, typeTable) && node.eval.elems.empty();
}

bool NodeVal::isLeaf(const NodeVal &node, const TypeTable *typeTable) {
    return !isRawVal(node, typeTable) || node.eval.elems.empty();
}

bool NodeVal::isRawVal(const NodeVal &node, const TypeTable *typeTable) {
    return node.isEvalVal() && EvalVal::isRaw(node.getEvalVal(), typeTable);
}

NodeVal NodeVal::makeEmpty(CodeLoc codeLoc, TypeTable *typeTable) {
    EvalVal emptyRaw = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_RAW), typeTable);
    return NodeVal(codeLoc, emptyRaw);
}

void NodeVal::escape(NodeVal &node, const TypeTable *typeTable, EscapeScore amount) {
    if (amount == 0) return;

    if (node.isLiteralVal()) {
        node.getLiteralVal().escapeScore += amount;
    } else if (node.isEvalVal()) {
        node.getEvalVal().escapeScore += amount;
        if (isRawVal(node, typeTable)) {
            for (auto &child : node.getEvalVal().elems) {
                escape(child, typeTable, amount);
            }
        }
    }
}

void NodeVal::unescape(NodeVal &node, const TypeTable *typeTable) {
    if (node.isLiteralVal()) {
        node.getLiteralVal().escapeScore -= 1;
    } else if (node.isEvalVal()) {
        if (isRawVal(node, typeTable)) {
            for (auto it = node.getEvalVal().elems.rbegin(); it != node.getEvalVal().elems.rend(); ++it) {
                unescape(*it, typeTable);
            }
        }
        node.getEvalVal().escapeScore -= 1;
    }
}

void NodeVal::copyNonValFieldsLeaf(NodeVal &dst, const NodeVal &src, const TypeTable *typeTable) {
    escape(dst, typeTable, src.getEscapeScore()-dst.getEscapeScore());
    if (src.hasTypeAttr()) {
        dst.setTypeAttr(src.getTypeAttr());
    }
    if (src.hasAttrs()) {
        dst.setAttrs(src.getAttrs());
    }
}