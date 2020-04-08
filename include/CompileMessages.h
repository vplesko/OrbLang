#pragma once

#include <vector>
#include <string>
#include "CodeLoc.h"
#include "Token.h"

class NamePool;

class CompileMessages {
    NamePool *namePool;
    std::vector<std::string> errors;

    void error(CodeLoc loc, const std::string &str);

public:
    explicit CompileMessages(NamePool *namePool) : namePool(namePool) {}

    bool isAbort() const { return !errors.empty(); }

    const std::vector<std::string>& getErrors() const { return errors; }

    void errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see);
    void errorNotSimple(CodeLoc loc);
    void errorNotPrim(CodeLoc loc);
    void errorUnknown(CodeLoc loc);
};