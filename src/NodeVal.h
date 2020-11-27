#pragma once

#include <vector>
#include <memory>
#include "CodeLoc.h"
#include "LiteralVal.h"
#include "EvalVal.h"
#include "LlvmVal.h"
#include "RawVal.h"

class NodeVal {
public:
    enum class Kind {
        kInvalid,
        kValid,
        kImport,
        kLiteral,
        kEval,
        kLlvm,
        kRaw
    };

private:
    CodeLoc codeLoc;

    Kind kind;
    StringPool::Id importFile;
    LiteralVal literal;
    LlvmVal llvm;
    EvalVal eval;
    RawVal raw;

    std::unique_ptr<NodeVal> typeAttr;

    bool escaped = false;

    void copyFrom(const NodeVal &other);

public:
    NodeVal(bool valid = false);
    NodeVal(CodeLoc codeLoc, StringPool::Id import);
    NodeVal(CodeLoc codeLoc, const LiteralVal &val);
    NodeVal(CodeLoc codeLoc, const EvalVal &val);
    NodeVal(CodeLoc codeLoc, const LlvmVal &val);
    NodeVal(CodeLoc codeLoc, const RawVal &val);

    NodeVal(const NodeVal &other);
    NodeVal& operator=(const NodeVal &other);

    NodeVal(NodeVal &&other) = default;
    NodeVal& operator=(NodeVal &&other) = default;

    CodeLoc getCodeLoc() const { return codeLoc; }
    bool isEscaped() const { return escaped; }
    std::optional<TypeTable::Id> getType() const;
    bool hasRef() const;
    std::size_t getLength() const;
    bool isEmpty() const { return isRawVal() && raw.isEmpty(); }
    bool isLeaf() const { return !isRawVal() || isEmpty(); }

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

    bool isLlvmVal() const { return kind == Kind::kLlvm; }
    LlvmVal& getLlvmVal() { return llvm; }
    const LlvmVal& getLlvmVal() const { return llvm; }

    bool isRawVal() const  { return kind == Kind::kRaw; }
    RawVal& getRawVal() { return raw; }
    const RawVal& getRawVal() const { return raw; }

    bool hasTypeAttr() const { return typeAttr != nullptr; }
    NodeVal& getTypeAttr() { return *typeAttr; }
    const NodeVal& getTypeAttr() const { return *typeAttr; }
    void setTypeAttr(NodeVal t);
    void clearTypeAttr() { typeAttr.reset(); }

    void escape();
    void unescape();
};