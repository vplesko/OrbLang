#pragma once

#include <vector>
#include <string>
#include "CodeLoc.h"
#include "Token.h"
#include "Reserved.h"
#include "TypeTable.h"
#include "SymbolTable.h"

class NamePool;

// TODO due to eval id and type vals, it's not guaranteed that given ids refer to anything valid, fix by introducing zero init
class CompileMessages {
    enum Status {
        S_OK,
        S_WARN,
        S_ERROR
    };

    NamePool *namePool;
    StringPool *stringPool;
    TypeTable *typeTable;
    SymbolTable *symbolTable;
    std::ostream *out;
    Status status;

    void warn(const std::string &str);
    void warn(CodeLoc loc, const std::string &str);
    void error(const std::string &str);
    void error(CodeLoc loc, const std::string &str);

    std::string errorStringOfTokenType(Token::Type tokTy) const;
    std::string errorStringOfToken(Token tok) const;
    std::string errorStringOfKeyword(Keyword k) const;
    std::string errorStringOfType(TypeTable::Id ty) const;

public:
    CompileMessages(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, std::ostream &out)
        : namePool(namePool), stringPool(stringPool), typeTable(typeTable), symbolTable(symbolTable), out(&out), status(S_OK) {}
    
    bool isFail() const { return status >= S_ERROR; }

    void warnExprIndexOutOfBounds(CodeLoc loc);
    void warnExprCompareStringLits(CodeLoc loc);

    void errorInputFileNotFound(const std::string &path);
    void errorBadToken(CodeLoc loc);
    void errorUnclosedMultilineComment(CodeLoc loc);
    void errorImportNotString(CodeLoc loc);
    void errorImportNotFound(CodeLoc loc, const std::string &path);
    void errorImportCyclical(CodeLoc loc, const std::string &path);
    void errorUnexpectedTokenType(CodeLoc loc, Token::Type exp);
    void errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token::Type see);
    void errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see);
    void errorUnexpectedTokenType(CodeLoc loc, std::vector<Token::Type> exp, Token see);
    void errorUnexpectedKeyword(CodeLoc loc, Keyword keyw);
    void errorUnexpectedIsNotTerminal(CodeLoc loc);
    void errorUnexpectedIsTerminal(CodeLoc loc);
    void errorUnexpectedNotId(CodeLoc loc);
    void errorUnexpectedNotType(CodeLoc loc);
    void errorChildrenNotEq(CodeLoc loc, std::size_t cnt);
    void errorChildrenLessThan(CodeLoc loc, std::size_t cnt);
    void errorChildrenMoreThan(CodeLoc loc, std::size_t cnt);
    void errorChildrenNotBetween(CodeLoc loc, std::size_t lo, std::size_t hi);
    void errorInvalidTypeDecorator(CodeLoc loc);
    void errorBadArraySize(CodeLoc loc, long int size);
    void errorNotLastParam(CodeLoc loc);
    void errorNonUnOp(CodeLoc loc, Oper op);
    void errorNonBinOp(CodeLoc loc, Oper op);
    void errorSymNameTaken(CodeLoc loc, NamePool::Id name);
    void errorSymNotFound(CodeLoc loc, NamePool::Id name);
    void errorCnNoInit(CodeLoc loc, NamePool::Id name);
    void errorExprCannotPromote(CodeLoc loc);
    void errorExprCannotPromote(CodeLoc loc, TypeTable::Id into);
    void errorExprKnownBinBadOp(CodeLoc loc);
    void errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotImplicitCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotImplicitCastEither(CodeLoc loc, TypeTable::Id ty1, TypeTable::Id ty2);
    void errorExprIndexOnBadType(CodeLoc loc);
    void errorExprIndexOnBadType(CodeLoc loc, TypeTable::Id ty);
    void errorExprDerefOnBadType(CodeLoc loc, TypeTable::Id ty);
    void errorExprAddressOfNoRef(CodeLoc loc);
    void errorExprIndexNotIntegral(CodeLoc loc);
    void errorExprUnBadType(CodeLoc loc);
    void errorExprUnOnNull(CodeLoc loc);
    void errorExprAsgnNonRef(CodeLoc loc);
    void errorExprAsgnOnCn(CodeLoc loc);
    void errorExprDotInvalidBase(CodeLoc loc);
    void errorExitNowhere(CodeLoc loc);
    void errorExitPassingBlock(CodeLoc loc);
    void errorPassNonPassingBlock(CodeLoc loc);
    void errorLoopNowhere(CodeLoc loc);
    void errorRetNoValue(CodeLoc loc, TypeTable::Id shouldRet);
    void errorFuncNameTaken(CodeLoc loc, NamePool::Id name);
    void errorMacroNameTaken(CodeLoc loc, NamePool::Id name);
    void errorArgNameDuplicate(CodeLoc loc, NamePool::Id name);
    void errorFuncNotFound(CodeLoc loc, NamePool::Id name);
    void errorMacroNotFound(CodeLoc loc, NamePool::Id name);
    void errorBlockNotFound(CodeLoc loc, NamePool::Id name);
    void errorBlockNoPass(CodeLoc loc);
    void errorMemberIndex(CodeLoc loc);
    void errorMissingTypeAttribute(CodeLoc loc);
    void errorNotGlobalScope(CodeLoc loc);
    void errorNotTopmost(CodeLoc loc);
    void errorNoMain();
    // placeholder error, should not stay in code
    void errorEvaluationNotSupported(CodeLoc loc);
    // placeholder error, should not stay in code
    void errorUnknown(CodeLoc loc);
    void errorInternal(CodeLoc loc);
};