#include "NodeVal.h"
using namespace std;

NodeVal::NodeVal() : kind(Kind::kInvalid) {
}

NodeVal::NodeVal(CodeLoc codeLoc, StringPool::Id import) : codeLoc(codeLoc), kind(Kind::kImport), importFile(import) {
}

NodeVal::NodeVal(CodeLoc codeLoc) : codeLoc(codeLoc), kind(Kind::kComposite) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const LiteralVal &val) : codeLoc(codeLoc), kind(Kind::kLiteral), literal(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const KnownVal &val) : codeLoc(codeLoc), kind(Kind::kKnown), known(val) {
}

NodeVal::NodeVal(CodeLoc codeLoc, const LlvmVal &val) : codeLoc(codeLoc), kind(Kind::kLlvm), llvm(val) {
}

NodeVal::NodeVal(const NodeVal &other) : NodeVal() {
    codeLoc = other.codeLoc;
    kind = other.kind;
    importFile = other.importFile;
    literal = other.literal;
    known = other.known;
    llvm = other.llvm;
    
    children.reserve(other.children.size());
    for (const auto &it : other.children) {
        children.push_back(make_unique<NodeVal>(*it));
    }
    
    if (other.hasTypeAnnot()) {
        typeAnnot = make_unique<NodeVal>(*other.typeAnnot);
    }

    escaped = other.escaped;
}

void swap(NodeVal &lhs, NodeVal &rhs) {
    swap(lhs.codeLoc, rhs.codeLoc);
    swap(lhs.kind, rhs.kind);
    swap(lhs.importFile, rhs.importFile);
    swap(lhs.literal, rhs.literal);
    swap(lhs.known, rhs.known);
    swap(lhs.llvm, rhs.llvm);
    swap(lhs.children, rhs.children);
    swap(lhs.typeAnnot, rhs.typeAnnot);
    swap(lhs.escaped, rhs.escaped);
}

NodeVal& NodeVal::operator=(NodeVal other) {
    swap(*this, other);
    return *this;
}

optional<TypeTable::Id> NodeVal::getType() const {
    if (isKnownVal()) return getKnownVal().getType();
    if (isLlvmVal()) return getLlvmVal().type;
    return nullopt;
}

bool NodeVal::hasRef() const {
    if (isKnownVal()) return getKnownVal().ref != nullptr;
    if (isLlvmVal()) return getLlvmVal().ref != nullptr;
    return false;
}

std::size_t NodeVal::getLength() const {
    if (isInvalid()) return 0;
    if (isComposite()) return getChildrenCnt();
    return 1;
}

void NodeVal::addChild(NodeVal c) {
    children.push_back(make_unique<NodeVal>(move(c)));
}

void NodeVal::addChildren(std::vector<NodeVal> c) {
    children.reserve(children.size()+c.size());
    for (auto &it : c) {
        addChild(move(it));
    }
}

void NodeVal::setTypeAnnot(NodeVal t) {
    typeAnnot = make_unique<NodeVal>(move(t));
}

void NodeVal::escape() {
    escaped = true;
    if (isComposite()) {
        for (auto &child : children) {
            child->escape();
        }
    }
}

void NodeVal::unescape() {
    if (isComposite()) {
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            (*it)->unescape();
        }
    }
    escaped = false;
}