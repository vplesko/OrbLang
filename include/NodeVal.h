#pragma once

#include <vector>
#include "CodeLoc.h"
#include "LiteralVal.h"
#include "KnownVal.h"
#include "LlvmVal.h"

struct NodeVal {
    enum class Kind {
        kInvalid,
        kImport,
        kLiteral,
        kKnown,
        kLlvm,
        kComposite
    };

    CodeLoc codeLoc;

    Kind kind;
    union {
        NamePool::Id importFile;
        LiteralVal literal;
        LlvmVal llvm;
    };
    KnownVal known;
    std::vector<NodeVal> children;

    std::optional<NodeVal> typeAnnot;

    bool isEscaped = false;

    // Invalid value.
    NodeVal();
    // Empty terminal.
    NodeVal(CodeLoc codeLoc);
    NodeVal(CodeLoc codeLoc, const LiteralVal &val);
    NodeVal(CodeLoc codeLoc, const KnownVal &val);
    NodeVal(CodeLoc codeLoc, const LlvmVal &val);
    NodeVal(CodeLoc codeLoc, const std::vector<NodeVal> &nodes);

    std::size_t getLength() const;
    bool isEmpty() const { return isComposite() && getChildrenCnt() == 0; }
    bool isLeaf() const { return !isComposite() || isEmpty(); }
    bool isInvalid() const { return kind == Kind::kInvalid; }

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
    std::vector<NodeVal>& getChildren() { return children; }
    const std::vector<NodeVal>& getChildren() const { return children; }
    std::size_t getChildrenCnt() const { return getChildren().size(); }

    void escape();
};