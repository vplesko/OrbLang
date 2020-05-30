#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "CodeLoc.h"
#include "Values.h"

struct AstNode {
    const CodeLoc codeLoc;

    enum class Kind {
        kTuple,
        kTerminal
    };

    const Kind kind;

    bool escaped = false;
    
    std::vector<std::unique_ptr<AstNode>> children;
    std::optional<TerminalVal> terminal;

    std::optional<std::unique_ptr<AstNode>> type;

    AstNode(CodeLoc loc, Kind k) : codeLoc(loc), kind(k) {}

    bool isTerminal() const { return kind == Kind::kTerminal; }
    bool hasType() const { return type.has_value(); }

    std::unique_ptr<AstNode> clone() const;
};

class AstStorage {
    std::vector<std::unique_ptr<AstNode>> nodes;

public:

    void store(std::unique_ptr<AstNode> node) {
        nodes.push_back(std::move(node));
    }
};