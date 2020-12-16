#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
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
        kAttrMap,
        kEval,
        kLlvm
    };

    typedef std::unordered_map<NamePool::Id, std::unique_ptr<NodeVal>> AttrMap;

private:
    CodeLoc codeLoc;

    Kind kind;
    StringPool::Id importFile;
    LiteralVal literal;
    AttrMap attrMap;
    LlvmVal llvm;
    EvalVal eval;

    std::unique_ptr<NodeVal> typeAttr, nonTypeAttrs;

    void copyAttrMap(const AttrMap &a);
    void copyFrom(const NodeVal &other);

public:
    // Invalid node
    NodeVal();
    NodeVal(CodeLoc codeLoc);
    NodeVal(CodeLoc codeLoc, StringPool::Id import);
    NodeVal(CodeLoc codeLoc, const LiteralVal &val);
    NodeVal(CodeLoc codeLoc, const AttrMap &val);
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

    bool isAttrMap() const { return kind == Kind::kAttrMap; }
    AttrMap& getAttrMap() { return attrMap; }
    const AttrMap& getAttrMap() const { return attrMap; }

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

    bool hasNonTypeAttrs() const { return nonTypeAttrs != nullptr; }
    NodeVal& getNonTypeAttrs() { return *nonTypeAttrs; }
    const NodeVal& getNonTypeAttrs() const { return *nonTypeAttrs; }
    void setNonTypeAttrs(NodeVal a);
    void clearNonTypeAttrs() { nonTypeAttrs.reset(); }

    static bool isEmpty(const NodeVal &node, const TypeTable *typeTable);
    static bool isLeaf(const NodeVal &node, const TypeTable *typeTable);
    static bool isRawVal(const NodeVal &node, const TypeTable *typeTable);

    static NodeVal makeEmpty(CodeLoc codeLoc, TypeTable *typeTable);
    static void escape(NodeVal &node, const TypeTable *typeTable, EscapeScore amount = 1);
    static void unescape(NodeVal &node, const TypeTable *typeTable);

    static NodeVal copyNoRef(const NodeVal &k);
    static NodeVal copyNoRef(CodeLoc codeLoc, const NodeVal &k);
    static void copyNonValFieldsLeaf(NodeVal &dst, const NodeVal &src, const TypeTable *typeTable);
};