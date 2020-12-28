#include "NodeVal.h"
using namespace std;

NodeVal::NodeVal() : kind(Kind::kInvalid) {
}

NodeVal::NodeVal(CodeLoc codeLoc) : codeLoc(codeLoc), kind(Kind::kValid) {
}

NodeVal::NodeVal(CodeLoc codeLoc, StringPool::Id import) : codeLoc(codeLoc), kind(Kind::kImport), value(import) {
}

NodeVal::NodeVal(CodeLoc codeLoc, LiteralVal val) : codeLoc(codeLoc), kind(Kind::kLiteral), value(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, SpecialVal val) : codeLoc(codeLoc), kind(Kind::kSpecial), value(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, AttrMap val) : codeLoc(codeLoc), kind(Kind::kAttrMap), value(move(val)) {
}

NodeVal::NodeVal(CodeLoc codeLoc, EvalVal val) : codeLoc(codeLoc), kind(Kind::kEval), value(move(val)) {
}

NodeVal::NodeVal(CodeLoc codeLoc, LlvmVal val) : codeLoc(codeLoc), kind(Kind::kLlvm), value(val) {
}

void NodeVal::copyAttrMap(const AttrMap &a) {
    value = AttrMap();
    for (const auto &loc : a) {
        getAttrMap().insert({loc.first, make_unique<NodeVal>(*loc.second)});
    }
}

void NodeVal::copyFrom(const NodeVal &other) {
    codeLoc = other.codeLoc;
    kind = other.kind;

    if (other.isImport()) value = other.getImportFile();
    else if (other.isLiteralVal()) value = other.getLiteralVal();
    else if (other.isSpecialVal()) value = other.getSpecialVal();
    else if (other.isEvalVal()) value = other.getEvalVal();
    else if (other.isLlvmVal()) value = other.getLlvmVal();
    else if (other.isAttrMap()) copyAttrMap(other.getAttrMap());

    if (other.hasTypeAttr()) {
        typeAttr = make_unique<NodeVal>(other.getTypeAttr());
    }
    if (other.hasNonTypeAttrs()) {
        nonTypeAttrs = make_unique<NodeVal>(other.getNonTypeAttrs());
    }
}

NodeVal::NodeVal(const NodeVal &other) {
    copyFrom(other);
}

NodeVal& NodeVal::operator=(const NodeVal &other) {
    if (this != &other) {
        copyFrom(other);
    }
    return *this;
}

NodeVal NodeVal::makeEmpty(CodeLoc codeLoc, TypeTable *typeTable) {
    EvalVal emptyRaw = EvalVal::makeVal(typeTable->getPrimTypeId(TypeTable::P_RAW), typeTable);
    return NodeVal(codeLoc, emptyRaw);
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

void NodeVal::addChild(NodeVal &node, NodeVal c, TypeTable *typeTable) {
    if (isRawVal(c, typeTable) && typeTable->worksAsTypeCn(c.getEvalVal().getType().value()))
        node.getEvalVal().getType() = typeTable->addTypeCnOf(node.getEvalVal().getType().value());

    node.getEvalVal().elems.push_back(move(c));
}

void NodeVal::addChildren(NodeVal &node, vector<NodeVal> c, TypeTable *typeTable) {
    addChildren(node, c.begin(), c.end(), typeTable);
}

void NodeVal::addChildren(NodeVal &node, vector<NodeVal>::iterator start, vector<NodeVal>::iterator end, TypeTable *typeTable) {
    node.getEvalVal().elems.reserve(node.getChildrenCnt()+(end-start));

    bool setCn = false;
    for (auto it = start; it != end; ++it) {
        if (isRawVal(*it, typeTable) && typeTable->worksAsTypeCn(it->getEvalVal().getType().value())) setCn = true;

        node.getEvalVal().elems.push_back(move(*it));
    }

    if (setCn) node.getEvalVal().getType() = typeTable->addTypeCnOf(node.getEvalVal().getType().value());
}

void NodeVal::setTypeAttr(NodeVal t) {
    typeAttr = make_unique<NodeVal>(move(t));
}

void NodeVal::setNonTypeAttrs(NodeVal a) {
    nonTypeAttrs = make_unique<NodeVal>(move(a));
}

bool NodeVal::isEmpty(const NodeVal &node, const TypeTable *typeTable) {
    return isRawVal(node, typeTable) && node.getEvalVal().elems.empty();
}

bool NodeVal::isLeaf(const NodeVal &node, const TypeTable *typeTable) {
    return !isRawVal(node, typeTable) || node.getEvalVal().elems.empty();
}

bool NodeVal::isRawVal(const NodeVal &node, const TypeTable *typeTable) {
    return node.isEvalVal() && EvalVal::isRaw(node.getEvalVal(), typeTable);
}

bool NodeVal::isFunc(const NodeVal &val, const TypeTable *typeTable) {
    optional<TypeTable::Id> type = val.getType();
    return type.has_value() && typeTable->worksAsCallable(type.value(), true);
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

NodeVal NodeVal::copyNoRef(const NodeVal &k) {
    NodeVal nodeVal(k);
    if (nodeVal.isLlvmVal()) nodeVal.getLlvmVal().ref = nullptr;
    else if (nodeVal.isEvalVal()) nodeVal.getEvalVal().ref = nullptr;
    return nodeVal;
}

NodeVal NodeVal::copyNoRef(CodeLoc codeLoc, const NodeVal &k) {
    NodeVal nodeVal = copyNoRef(k);
    nodeVal.codeLoc = codeLoc;
    return nodeVal;
}

void NodeVal::copyNonValFieldsLeaf(NodeVal &dst, const NodeVal &src, const TypeTable *typeTable) {
    escape(dst, typeTable, src.getEscapeScore()-dst.getEscapeScore());
    if (src.hasTypeAttr()) {
        dst.setTypeAttr(src.getTypeAttr());
    }
    if (src.hasNonTypeAttrs()) {
        dst.setNonTypeAttrs(src.getNonTypeAttrs());
    }
}