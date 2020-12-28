#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>
#include "CodeLoc.h"
#include "LiteralVal.h"
#include "SpecialVal.h"
#include "EvalVal.h"
#include "LlvmVal.h"

class NodeVal {
public:
    typedef std::unordered_map<NamePool::Id, std::unique_ptr<NodeVal>> AttrMap;

private:
    enum class Kind {
        kInvalid,
        kValid,
        kImport,
        kLiteral,
        kSpecial,
        kAttrMap,
        kEval,
        kLlvm
    };

    CodeLoc codeLoc;

    Kind kind;
    std::variant<StringPool::Id, LiteralVal, SpecialVal, AttrMap, LlvmVal, EvalVal> value;
    std::unique_ptr<NodeVal> typeAttr, nonTypeAttrs;

    void copyAttrMap(const AttrMap &a);
    void copyFrom(const NodeVal &other);

public:
    // Invalid node
    NodeVal();
    // Valid node
    NodeVal(CodeLoc codeLoc);
    NodeVal(CodeLoc codeLoc, StringPool::Id import);
    NodeVal(CodeLoc codeLoc, LiteralVal val);
    NodeVal(CodeLoc codeLoc, SpecialVal val);
    NodeVal(CodeLoc codeLoc, AttrMap val);
    NodeVal(CodeLoc codeLoc, EvalVal val);
    NodeVal(CodeLoc codeLoc, LlvmVal val);

    NodeVal(const NodeVal &other);
    NodeVal& operator=(const NodeVal &other);

    NodeVal(NodeVal &&other) = default;
    NodeVal& operator=(NodeVal &&other) = default;

    static NodeVal makeEmpty(CodeLoc codeLoc, TypeTable *typeTable);

    CodeLoc getCodeLoc() const { return codeLoc; }
    bool isEscaped() const;
    EscapeScore getEscapeScore() const;
    std::optional<TypeTable::Id> getType() const;
    bool hasRef() const;

    // Remember to check when returned to you before any other checks or usages.
    bool isInvalid() const { return kind == Kind::kInvalid; }

    bool isImport() const { return kind == Kind::kImport; }
    StringPool::Id getImportFile() const { return std::get<StringPool::Id>(value); }

    bool isLiteralVal() const { return kind == Kind::kLiteral; }
    LiteralVal& getLiteralVal() { return std::get<LiteralVal>(value); }
    const LiteralVal& getLiteralVal() const { return std::get<LiteralVal>(value); }

    bool isSpecialVal() const { return kind == Kind::kSpecial; }
    SpecialVal& getSpecialVal() { return std::get<SpecialVal>(value); }
    const SpecialVal& getSpecialVal() const { return std::get<SpecialVal>(value); }

    bool isAttrMap() const { return kind == Kind::kAttrMap; }
    AttrMap& getAttrMap() { return std::get<AttrMap>(value); }
    const AttrMap& getAttrMap() const { return std::get<AttrMap>(value); }

    bool isEvalVal() const { return kind == Kind::kEval; }
    EvalVal& getEvalVal() { return std::get<EvalVal>(value); }
    const EvalVal& getEvalVal() const { return std::get<EvalVal>(value); }
    std::size_t getChildrenCnt() const { return getEvalVal().elems.size(); }
    NodeVal& getChild(std::size_t ind) { return getEvalVal().elems[ind]; }
    const NodeVal& getChild(std::size_t ind) const { return getEvalVal().elems[ind]; }
    static void addChild(NodeVal &node, NodeVal c, TypeTable *typeTable);
    static void addChildren(NodeVal &node, std::vector<NodeVal> c, TypeTable *typeTable);
    static void addChildren(NodeVal &node, std::vector<NodeVal>::iterator start, std::vector<NodeVal>::iterator end, TypeTable *typeTable);

    bool isLlvmVal() const { return kind == Kind::kLlvm; }
    LlvmVal& getLlvmVal() { return std::get<LlvmVal>(value); }
    const LlvmVal& getLlvmVal() const { return std::get<LlvmVal>(value); }

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

    static bool isFunc(const NodeVal &val, const TypeTable *typeTable);

    static void escape(NodeVal &node, const TypeTable *typeTable, EscapeScore amount = 1);
    static void unescape(NodeVal &node, const TypeTable *typeTable);

    static NodeVal copyNoRef(const NodeVal &k);
    static NodeVal copyNoRef(CodeLoc codeLoc, const NodeVal &k);
    static void copyNonValFieldsLeaf(NodeVal &dst, const NodeVal &src, const TypeTable *typeTable);
};