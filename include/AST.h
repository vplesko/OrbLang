#pragma once

#include <memory>
#include <vector>
#include <optional>
#include "Lexer.h"
#include "Values.h"
#include "SymbolTable.h"

struct AstNode {
    const CodeLoc codeLoc;

    enum class Kind {
        kTuple,
        kTerminal
    };
    const Kind kind;
    
    std::vector<std::unique_ptr<AstNode>> children;
    std::optional<TerminalVal> terminal;

    std::optional<std::unique_ptr<AstNode>> type;

    AstNode(CodeLoc loc, Kind k) : codeLoc(loc), kind(k) {}
};