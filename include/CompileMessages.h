#pragma once

#include <vector>
#include <string>
#include "CodeLoc.h"
#include "Token.h"
#include "SymbolTable.h"

class NamePool;

class CompileMessages {
    NamePool *namePool;
    SymbolTable *symbolTable;
    std::vector<std::string> errors;

    void error(CodeLoc loc, const std::string &str);

    std::string errorStringOfType(TypeTable::Id ty) const;

public:
    explicit CompileMessages(NamePool *namePool, SymbolTable *symbolTable)
        : namePool(namePool), symbolTable(symbolTable) {}

    bool isAbort() const { return !errors.empty(); }

    const std::vector<std::string>& getErrors() const { return errors; }

    void errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see);
    void errorUnexpectedTokenType(CodeLoc loc, std::vector<Token::Type> exp, Token see);
    void errorNotSimple(CodeLoc loc);
    void errorNotPrim(CodeLoc loc);
    void errorNotTypeId(CodeLoc loc, NamePool::Id name);
    void errorBadArraySize(CodeLoc loc, long int size);
    void errorSwitchNoBranches(CodeLoc loc);
    void errorSwitchMultiElse(CodeLoc loc);
    void errorNotLastParam(CodeLoc loc);
    void errorBadAttr(CodeLoc loc, Token::Attr attr);
    void errorEmptyArr(CodeLoc loc);
    void errorNonUnOp(CodeLoc loc, Token op);
    void errorNonBinOp(CodeLoc loc, Token op);
    void errorVarNameTaken(CodeLoc loc, NamePool::Id name);
    void errorCnNoInit(CodeLoc loc, NamePool::Id name);
    void errorExprNotBaked(CodeLoc loc);
    void errorExprCannotPromote(CodeLoc loc, TypeTable::Id ty);
    void errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorUnknown(CodeLoc loc);
};