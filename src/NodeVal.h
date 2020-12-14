#pragma once

#include <vector>
#include <memory>
#include "CodeLoc.h"
#include "LiteralVal.h"
#include "EvalVal.h"
#include "LlvmVal.h"

class NodeVal {
public:
    enum class Kind {
        kInvalid,
        kValid,
        kImport,
        kLiteral,
        kEval,
        kLlvm
    };

private:
    CodeLoc codeLoc;

    Kind kind;
    StringPool::Id importFile;
    LiteralVal literal;
    LlvmVal llvm;
    EvalVal eval;

    std::unique_ptr<NodeVal> typeAttr, attrs;

    void copyFrom(const NodeVal &other);

public:
    // Invalid node
    NodeVal();
    NodeVal(CodeLoc codeLoc);
    NodeVal(CodeLoc codeLoc, StringPool::Id import);
    NodeVal(CodeLoc codeLoc, const LiteralVal &val);
    NodeVal(CodeLoc codeLoc, const EvalVal &val);
    NodeVal(CodeLoc codeLoc, const LlvmVal &val);

    NodeVal(const NodeVal &other);
    NodeVal& operator=(const NodeVal &other);

    NodeVal(NodeVal &&other) = default;
    NodeVal& operator=(NodeVal &&other) = default;

    CodeLoc getCodeLoc() const { return codeLoc; }
    bool isEscaped() const;
    EscapeScore getEscapeScore() const;
    std::optional<TypeTable::Id> getType() const;
    bool hasRef() const;

    // Remember to check when returned to you before any other checks or usages.
    bool isInvalid() const { return kind == Kind::kInvalid; }

    bool isImport() const { return kind == Kind::kImport; }
    StringPool::Id getImportFile() const { return importFile; }

    bool isLiteralVal() const { return kind == Kind::kLiteral; }
    LiteralVal& getLiteralVal() { return literal; }
    const LiteralVal& getLiteralVal() const { return literal; }

    bool isEvalVal() const { return kind == Kind::kEval; }
    EvalVal& getEvalVal() { return eval; }
    const EvalVal& getEvalVal() const { return eval; }
    std::size_t getChildrenCnt() const { return eval.elems.size(); }
    NodeVal& getChild(std::size_t ind) { return eval.elems[ind]; }
    const NodeVal& getChild(std::size_t ind) const { return eval.elems[ind]; }
    void addChild(NodeVal c);
    void addChildren(std::vector<NodeVal> c);

    bool isLlvmVal() const { return kind == Kind::kLlvm; }
    LlvmVal& getLlvmVal() { return llvm; }
    const LlvmVal& getLlvmVal() const { return llvm; }

    bool hasTypeAttr() const { return typeAttr != nullptr; }
    NodeVal& getTypeAttr() { return *typeAttr; }
    const NodeVal& getTypeAttr() const { return *typeAttr; }
    void setTypeAttr(NodeVal t);
    void clearTypeAttr() { typeAttr.reset(); }

    bool hasAttrs() const { return attrs != nullptr; }
    NodeVal& getAttrs() { return *attrs; }
    const NodeVal& getAttrs() const { return *attrs; }
    void setAttrs(NodeVal a);
    void clearAttrs() { attrs.reset(); }

    static bool isEmpty(const NodeVal &node, const TypeTable *typeTable);
    static bool isLeaf(const NodeVal &node, const TypeTable *typeTable);
    static bool isRawVal(const NodeVal &node, const TypeTable *typeTable);

    static NodeVal makeEmpty(CodeLoc codeLoc, TypeTable *typeTable);
    static void escape(NodeVal &node, const TypeTable *typeTable, EscapeScore amount = 1);
    static void unescape(NodeVal &node, const TypeTable *typeTable);
    static void copyNonValFieldsLeaf(NodeVal &dst, const NodeVal &src, const TypeTable *typeTable);
};