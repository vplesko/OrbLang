#pragma once

#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>
#include "AttrMap.h"
#include "CodeLoc.h"
#include "EvalVal.h"
#include "LifetimeInfo.h"
#include "LiteralVal.h"
#include "LlvmVal.h"
#include "SpecialVal.h"
#include "UndecidedCallableVal.h"

class NodeVal {
    CodeLoc codeLoc;

    std::variant<bool, StringPool::Id, LiteralVal, SpecialVal, AttrMap, LlvmVal, EvalVal, UndecidedCallableVal> value;
    std::unique_ptr<NodeVal> typeAttr, nonTypeAttrs;

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
    NodeVal(CodeLoc codeLoc, UndecidedCallableVal val);

    // TODO remove as many occurences of NodeVal copying as possible
    NodeVal(const NodeVal &other);
    void operator=(const NodeVal &other);

    NodeVal(NodeVal &&other) = default;
    NodeVal& operator=(NodeVal &&other) = default;

    CodeLoc getCodeLoc() const { return codeLoc; }
    void setCodeLoc(CodeLoc loc) { codeLoc = loc; }

    bool isEscaped() const;
    EscapeScore getEscapeScore() const;
    std::optional<TypeTable::Id> getType() const;
    bool hasRef() const;
    void removeRef();
    std::optional<VarId> getVarId() const;
    bool isNoDrop() const;
    bool isInvokeArg() const;
    // returns whether set successfully
    bool setNoDrop(bool b);
    std::optional<LifetimeInfo> getLifetimeInfo() const;
    // returns whether set successfully
    bool setLifetimeInfo(LifetimeInfo lifetimeInfo);

    // Remember to check when returned to you before any other checks or usages.
    bool isInvalid() const { return std::holds_alternative<bool>(value) && std::get<bool>(value) == false; }

    bool isImport() const { return std::holds_alternative<StringPool::Id>(value); }
    StringPool::Id getImportFile() const { return std::get<StringPool::Id>(value); }

    bool isLiteralVal() const { return std::holds_alternative<LiteralVal>(value); }
    LiteralVal& getLiteralVal() { return std::get<LiteralVal>(value); }
    const LiteralVal& getLiteralVal() const { return std::get<LiteralVal>(value); }

    bool isSpecialVal() const { return std::holds_alternative<SpecialVal>(value); }
    SpecialVal& getSpecialVal() { return std::get<SpecialVal>(value); }
    const SpecialVal& getSpecialVal() const { return std::get<SpecialVal>(value); }

    bool isAttrMap() const { return std::holds_alternative<AttrMap>(value); }
    AttrMap& getAttrMap() { return std::get<AttrMap>(value); }
    const AttrMap& getAttrMap() const { return std::get<AttrMap>(value); }

    bool isLlvmVal() const { return std::holds_alternative<LlvmVal>(value); }
    LlvmVal& getLlvmVal() { return std::get<LlvmVal>(value); }
    const LlvmVal& getLlvmVal() const { return std::get<LlvmVal>(value); }

    bool isEvalVal() const { return std::holds_alternative<EvalVal>(value); }
    EvalVal& getEvalVal() { return std::get<EvalVal>(value); }
    const EvalVal& getEvalVal() const { return std::get<EvalVal>(value); }
    std::size_t getChildrenCnt() const { return getEvalVal().elems().size(); }
    NodeVal& getChild(std::size_t ind) { return getEvalVal().elems()[ind]; }
    const NodeVal& getChild(std::size_t ind) const { return getEvalVal().elems()[ind]; }
    static void addChild(NodeVal &node, NodeVal c, TypeTable *typeTable);
    static void addChildren(NodeVal &node, std::vector<NodeVal> c, TypeTable *typeTable);
    static void addChildren(NodeVal &node, std::vector<NodeVal>::iterator start, std::vector<NodeVal>::iterator end, TypeTable *typeTable);

    bool isUndecidedCallableVal() const { return std::holds_alternative<UndecidedCallableVal>(value); }
    UndecidedCallableVal& getUndecidedCallableVal() { return std::get<UndecidedCallableVal>(value); }
    const UndecidedCallableVal& getUndecidedCallableVal() const { return std::get<UndecidedCallableVal>(value); }

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
    static bool isMacro(const NodeVal &val, const TypeTable *typeTable);

    static void escape(NodeVal &node, const TypeTable *typeTable, EscapeScore amount = 1);
    static void unescape(NodeVal &node, const TypeTable *typeTable, bool total = false);
    static void clearInvokeArg(NodeVal &node, const TypeTable *typeTable);

    static NodeVal makeEmpty(CodeLoc codeLoc, TypeTable *typeTable);

    // TODO remove as many calls to copyNoRef as possible
    static NodeVal copyNoRef(const NodeVal &k);
    static NodeVal copyNoRef(const NodeVal &k, LifetimeInfo lifetimeInfo);
    static NodeVal copyNoRef(CodeLoc codeLoc, const NodeVal &k);
    static NodeVal copyNoRef(CodeLoc codeLoc, const NodeVal &k, LifetimeInfo lifetimeInfo);
    static NodeVal moveNoRef(NodeVal &&k, LifetimeInfo lifetimeInfo);
    static NodeVal moveNoRef(CodeLoc codeLoc, NodeVal &&k, LifetimeInfo lifetimeInfo);
    static void copyNonValFieldsLeaf(NodeVal &dst, const NodeVal &src, const TypeTable *typeTable);
};