#pragma once

#include <vector>
#include <memory>
#include "CodeLoc.h"
#include "LiteralVal.h"
#include "KnownVal.h"
#include "LlvmVal.h"

class NodeVal {
public:
    enum class Kind {
        kInvalid,
        kImport,
        kLiteral,
        kKnown,
        kLlvm,
        kComposite
    };

private:
    CodeLoc codeLoc;

    Kind kind;
    StringPool::Id importFile;
    LiteralVal literal;
    LlvmVal llvm;
    KnownVal known;
    std::vector<std::unique_ptr<NodeVal>> children;

    std::unique_ptr<NodeVal> typeAttr;

    bool escaped = false;

    void copyFrom(const NodeVal &other);

public:
    // Invalid value. Remember to set code loc before returning if meant to be valid.
    NodeVal();
    // Empty terminal.
    // TODO split the use of this for valid processing ret and actual empty node
    NodeVal(CodeLoc codeLoc);
    NodeVal(CodeLoc codeLoc, StringPool::Id import);
    NodeVal(CodeLoc codeLoc, const LiteralVal &val);
    NodeVal(CodeLoc codeLoc, const KnownVal &val);
    NodeVal(CodeLoc codeLoc, const LlvmVal &val);

    NodeVal(const NodeVal &other);
    NodeVal& operator=(const NodeVal &other);

    NodeVal(NodeVal &&other) = default;
    NodeVal& operator=(NodeVal &&other) = default;

    CodeLoc getCodeLoc() const { return codeLoc; }
    bool isEscaped() const { return escaped; }
    std::optional<TypeTable::Id> getType() const;
    bool hasRef() const;
    std::size_t getLength() const;
    bool isEmpty() const { return isComposite() && getChildrenCnt() == 0; }
    bool isLeaf() const { return !isComposite() || isEmpty(); }

    // Remember to check when returned to you before any other checks or usages.
    bool isInvalid() const { return kind == Kind::kInvalid; }

    bool isImport() const { return kind == Kind::kImport; }
    StringPool::Id getImportFile() const { return importFile; }

    bool isLiteralVal() const { return kind == Kind::kLiteral; }
    LiteralVal& getLiteralVal() { return literal; }
    const LiteralVal& getLiteralVal() const { return literal; }

    bool isKnownVal() const { return kind == Kind::kKnown; }
    KnownVal& getKnownVal() { return known; }
    const KnownVal& getKnownVal() const { return known; }

    bool isLlvmVal() const { return kind == Kind::kLlvm; }
    LlvmVal& getLlvmVal() { return llvm; }
    const LlvmVal& getLlvmVal() const { return llvm; }

    bool isComposite() const  { return kind == Kind::kComposite; }
    NodeVal& getChild(std::size_t ind) { return *children[ind]; }
    const NodeVal& getChild(std::size_t ind) const { return *children[ind]; }
    void addChild(NodeVal c);
    void addChildren(std::vector<NodeVal> c);
    std::size_t getChildrenCnt() const { return children.size(); }

    bool hasTypeAttr() const { return typeAttr != nullptr; }
    NodeVal& getTypeAttr() { return *typeAttr; }
    const NodeVal& getTypeAttr() const { return *typeAttr; }
    void setTypeAttr(NodeVal t);
    void clearTypeAttr() { typeAttr.reset(); }

    void escape();
    void unescape();
};