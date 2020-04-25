#pragma once

#include <vector>
#include <string>
#include "CodeLoc.h"
#include "Token.h"
#include "SymbolTable.h"

class NamePool;

class CompileMessages {
    enum Status {
        S_OK,
        S_WARN,
        S_ERROR
    };

    NamePool *namePool;
    SymbolTable *symbolTable;
    std::ostream *out;
    Status status;

    void warn(const std::string &str);
    void warn(CodeLoc loc, const std::string &str);
    void error(const std::string &str);
    void error(CodeLoc loc, const std::string &str);

    std::string errorStringOfType(TypeTable::Id ty) const;

public:
    explicit CompileMessages(NamePool *namePool, SymbolTable *symbolTable, std::ostream &out)
        : namePool(namePool), symbolTable(symbolTable), out(&out), status(S_OK) {}
    
    bool isAbort() const { return status >= S_ERROR; }

    void warnExprIndexOutOfBounds(CodeLoc loc);

    void errorBadToken(CodeLoc loc);
    void errorUnclosedMultilineComment(CodeLoc loc);
    void errorImportNotFound(CodeLoc loc, std::string &path);
    void errorImportCyclical(CodeLoc loc, std::string &path);
    void errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see);
    void errorUnexpectedTokenType(CodeLoc loc, std::vector<Token::Type> exp, Token see);
    void errorNotSimple(CodeLoc loc);
    void errorNotPrim(CodeLoc loc);
    void errorNotTypeId(CodeLoc loc, NamePool::Id name);
    void errorBadArraySize(CodeLoc loc, long int size);
    void errorSwitchNoBranches(CodeLoc loc);
    void errorSwitchMultiElse(CodeLoc loc);
    void errorSwitchNotIntegral(CodeLoc loc);
    void errorSwitchMatchDuplicate(CodeLoc loc);
    void errorNotLastParam(CodeLoc loc);
    void errorBadAttr(CodeLoc loc, Token::Attr attr);
    void errorEmptyArr(CodeLoc loc);
    void errorNonUnOp(CodeLoc loc, Token op);
    void errorNonBinOp(CodeLoc loc, Token op);
    void errorVarNameTaken(CodeLoc loc, NamePool::Id name);
    void errorVarNotFound(CodeLoc loc, NamePool::Id name);
    void errorCnNoInit(CodeLoc loc, NamePool::Id name);
    void errorExprNotBaked(CodeLoc loc);
    void errorExprCannotPromote(CodeLoc loc, TypeTable::Id into);
    void errorExprUntyMismatch(CodeLoc loc);
    void errorExprUntyBinBadOp(CodeLoc loc, Token::Oper op);
    void errorExprCompareStringLits(CodeLoc loc);
    void errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotCastEither(CodeLoc loc, TypeTable::Id ty1, TypeTable::Id ty2);
    void errorExprCallVariadUnty(CodeLoc loc, NamePool::Id name);
    void errorExprIndexOnBadType(CodeLoc loc);
    void errorExprIndexOnBadType(CodeLoc loc, TypeTable::Id ty);
    void errorExprDerefOnBadType(CodeLoc loc, TypeTable::Id ty);
    void errorExprAddressOfNoRef(CodeLoc loc);
    void errorExprIndexNotIntegral(CodeLoc loc);
    void errorExprUnBadType(CodeLoc loc, Token::Oper op);
    void errorExprUnBadType(CodeLoc loc, Token::Oper op, TypeTable::Id ty);
    void errorExprUnOnCn(CodeLoc loc, Token::Oper op);
    void errorExprUnOnNull(CodeLoc loc, Token::Oper op);
    void errorExprAsgnNonRef(CodeLoc loc, Token::Oper op);
    void errorExprAsgnOnCn(CodeLoc loc, Token::Oper op);
    void errorBreakNowhere(CodeLoc loc);
    void errorContinueNowhere(CodeLoc loc);
    void errorRetNoValue(CodeLoc loc, TypeTable::Id shouldRet);
    void errorFuncNameTaken(CodeLoc loc, NamePool::Id name);
    void errorFuncSigConflict(CodeLoc loc);
    void errorFuncArgNameDuplicate(CodeLoc loc, NamePool::Id name);
    void errorFuncNotFound(CodeLoc loc, NamePool::Id name);
    void errorDataNameTaken(CodeLoc loc, NamePool::Id name);
    void errorDataMemberNameDuplicate(CodeLoc loc, NamePool::Id name);
    void errorDataNoMembers(CodeLoc loc, NamePool::Id name);
    void errorUnknown(CodeLoc loc);
};