#include "AST.h"
using namespace std;

unique_ptr<AstNode> AstNode::clone() const {
    unique_ptr<AstNode> other = make_unique<AstNode>(codeLoc, kind);
    
    other->escaped = escaped;

    other->children.resize(children.size());
    for (size_t i = 0; i < children.size(); ++i) {
        other->children[i] = children[i]->clone();
    }

    if (terminal.has_value()) {
        other->terminal = terminal.value();
    }

    if (type.has_value()) {
        other->type = type.value()->clone();
    }

    return other;
}