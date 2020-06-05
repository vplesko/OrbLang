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
    StringPool *stringPool;
    SymbolTable *symbolTable;
    std::ostream *out;
    Status status;

    TypeTable* getTypeTable() { return symbolTable->getTypeTable(); }
    const TypeTable* getTypeTable() const { return symbolTable->getTypeTable(); }

    void warn(const std::string &str);
    void warn(CodeLoc loc, const std::string &str);
    void error(const std::string &str);
    void error(CodeLoc loc, const std::string &str);

    std::string errorStringOfType(TypeTable::Id ty) const;

public:
    explicit CompileMessages(NamePool *namePool, StringPool *stringPool, SymbolTable *symbolTable, std::ostream &out)
        : namePool(namePool), stringPool(stringPool), symbolTable(symbolTable), out(&out), status(S_OK) {}
    
    bool isFail() const { return status >= S_ERROR; }

    void warnExprIndexOutOfBounds(CodeLoc loc);

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
    void errorUnexpectedKeyword(CodeLoc loc, Token::Type keyw);
    void errorUnexpectedIsNotTerminal(CodeLoc loc);
    void errorUnexpectedIsTerminal(CodeLoc loc);
    void errorUnexpectedNotKeyword(CodeLoc loc);
    void errorUnexpectedNotId(CodeLoc loc);
    void errorUnexpectedNotFunc(CodeLoc loc);
    void errorUnexpectedNotType(CodeLoc loc);
    void errorUnexpectedNotBlock(CodeLoc loc);
    void errorUnexpectedNotAttribute(CodeLoc loc);
    void errorUnexpectedNodeValue(CodeLoc loc);
    void errorChildrenNotEq(CodeLoc loc, std::size_t cnt);
    void errorChildrenLessThan(CodeLoc loc, std::size_t cnt);
    void errorChildrenMoreThan(CodeLoc loc, std::size_t cnt);
    void errorChildrenNotBetween(CodeLoc loc, std::size_t lo, std::size_t hi);
    void errorInvalidTypeDecorator(CodeLoc loc);
    void errorBadArraySize(CodeLoc loc, long int size);
    void errorNotLastParam(CodeLoc loc);
    void errorBadAttr(CodeLoc loc, Token::Attr attr);
    void errorNonUnOp(CodeLoc loc, Token::Oper op);
    void errorNonBinOp(CodeLoc loc, Token::Oper op);
    void errorVarNameTaken(CodeLoc loc, NamePool::Id name);
    void errorVarNotFound(CodeLoc loc, NamePool::Id name);
    void errorCnNoInit(CodeLoc loc, NamePool::Id name);
    void errorCnNoInit(CodeLoc loc);
    void errorExprNotBaked(CodeLoc loc);
    void errorExprCannotPromote(CodeLoc loc, TypeTable::Id into);
    void errorExprUntyMismatch(CodeLoc loc);
    void errorExprUntyBinBadOp(CodeLoc loc, Token::Oper op);
    void errorExprCompareStringLits(CodeLoc loc);
    void errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotImplicitCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotImplicitCastEither(CodeLoc loc, TypeTable::Id ty1, TypeTable::Id ty2);
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
    void errorExprDotInvalidBase(CodeLoc loc);
    void errorExprNotValue(CodeLoc loc);
    void errorExitNowhere(CodeLoc loc);
    void errorExitPassingBlock(CodeLoc loc);
    void errorPassNonPassingBlock(CodeLoc loc);
    void errorLoopNowhere(CodeLoc loc);
    void errorRetNoValue(CodeLoc loc, TypeTable::Id shouldRet);
    void errorFuncNameTaken(CodeLoc loc, NamePool::Id name);
    void errorMacroNameTaken(CodeLoc loc, NamePool::Id name);
    void errorSigConflict(CodeLoc loc);
    void errorArgNameDuplicate(CodeLoc loc, NamePool::Id name);
    void errorFuncNotFound(CodeLoc loc, NamePool::Id name);
    void errorFuncAmbigious(CodeLoc loc, NamePool::Id name);
    void errorMacroNotFound(CodeLoc loc, NamePool::Id name);
    void errorBlockNotFound(CodeLoc loc, NamePool::Id name);
    void errorBlockNoPass(CodeLoc loc);
    void errorMemberIndex(CodeLoc loc);
    void errorTupleValueMember(CodeLoc loc);
    void errorMismatchTypeAnnotation(CodeLoc loc, TypeTable::Id ty);
    void errorMismatchTypeAnnotation(CodeLoc loc);
    void errorMissingTypeAnnotation(CodeLoc loc);
    void errorNotGlobalScope(CodeLoc loc);
    // placeholder error, should not stay in code
    void errorUnknown(CodeLoc loc);
    void errorInternal(CodeLoc loc);
};