#pragma once

#include <vector>
#include <string>
#include "CodeLoc.h"
#include "Token.h"
#include "Reserved.h"
#include "TypeTable.h"
#include "SymbolTable.h"

class NamePool;

class CompilationMessages {
public:
    enum Status {
        S_NONE,
        S_INFO,
        S_WARNING,
        S_ERROR,
        S_INTERNAL
    };

private:
    NamePool *namePool;
    StringPool *stringPool;
    TypeTable *typeTable;
    SymbolTable *symbolTable;
    std::ostream *out;
    Status status;

    void raise(Status s) { status = std::max(status, s); }
    void heading(CodeLoc loc);
    void bolded(const std::string &str);
    void info();
    void info(const std::string &str);
    void info(CodeLoc loc, const std::string &str);
    void warning();
    void warning(const std::string &str);
    void warning(CodeLoc loc, const std::string &str);
    void error();
    void error(const std::string &str);
    void error(CodeLoc loc, const std::string &str);

    std::string errorStringOfTokenType(Token::Type tokTy) const;
    std::string errorStringOfToken(Token tok) const;
    std::string errorStringOfKeyword(Keyword k) const;
    std::string errorStringOfType(TypeTable::Id ty) const;

public:
    CompilationMessages(NamePool *namePool, StringPool *stringPool, TypeTable *typeTable, SymbolTable *symbolTable, std::ostream &out)
        : namePool(namePool), stringPool(stringPool), typeTable(typeTable), symbolTable(symbolTable), out(&out), status(S_NONE) {}

    Status getStatus() const {return status; }
    bool isFail() const { return status >= S_ERROR; }

    void displayCodeSegment(CodeLoc loc);

    bool userMessageStart(CodeLoc loc, Status s);
    void userMessageEnd();
    void userMessage(std::int64_t x);
    void userMessage(std::uint64_t x);
    void userMessage(double x);
    void userMessage(char x);
    void userMessage(bool x);
    void userMessage(NamePool::Id x);
    void userMessage(TypeTable::Id x);
    void userMessageNull();
    void userMessage(StringPool::Id str);

    void hintForgotCloseNode();

    void warnUnusedSpecial(CodeLoc loc, SpecialVal spec);
    void warnUnusedFunc(CodeLoc loc);
    void warnUnusedMacro(CodeLoc loc);

    void errorInputFileNotFound(const std::string &path);
    void errorBadToken(CodeLoc loc);
    void errorUnclosedMultilineComment(CodeLoc loc);
    void errorImportNotString(CodeLoc loc);
    void errorImportNotFound(CodeLoc loc, const std::string &path);
    void errorImportCyclical(CodeLoc loc, const std::string &path);
    void errorUnexpectedTokenType(CodeLoc loc, Token see);
    void errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see);
    void errorUnexpectedKeyword(CodeLoc loc, Keyword keyw);
    void errorUnexpectedNonLeafStart(CodeLoc loc);
    void errorUnexpectedIsNotTerminal(CodeLoc loc);
    void errorUnexpectedIsTerminal(CodeLoc loc);
    void errorUnexpectedNotId(CodeLoc loc);
    void errorUnexpectedNotType(CodeLoc loc);
    void errorUnexpectedNotBool(CodeLoc loc);
    void errorChildrenNotEq(CodeLoc loc, std::size_t cnt, std::size_t see);
    void errorChildrenLessThan(CodeLoc loc, std::size_t cnt);
    void errorChildrenMoreThan(CodeLoc loc, std::size_t cnt);
    void errorChildrenNotBetween(CodeLoc loc, std::size_t lo, std::size_t hi, std::size_t see);
    void errorInvalidTypeDecorator(CodeLoc loc);
    void errorBadArraySize(CodeLoc loc, long int size);
    void errorNotLastParam(CodeLoc loc);
    void errorNonUnOp(CodeLoc loc, Oper op);
    void errorNonBinOp(CodeLoc loc, Oper op);
    void errorSymNameTaken(CodeLoc loc, NamePool::Id name);
    void errorSymUndef(CodeLoc loc, TypeTable::Id ty);
    void errorSymbolNotFound(CodeLoc loc, NamePool::Id name);
    void errorCnNoInit(CodeLoc loc, NamePool::Id name);
    void errorExprCannotPromote(CodeLoc loc);
    void errorExprCannotPromote(CodeLoc loc, TypeTable::Id into);
    void errorExprEvalBinBadOp(CodeLoc loc);
    void errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotImplicitCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotImplicitCastEither(CodeLoc loc, TypeTable::Id ty1, TypeTable::Id ty2);
    void errorExprIndexOnBadType(CodeLoc loc);
    void errorExprIndexOnBadType(CodeLoc loc, TypeTable::Id ty);
    void errorExprIndexOutOfBounds(CodeLoc loc);
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
    void errorFuncNoValue(CodeLoc loc);
    void errorMacroNoValue(CodeLoc loc);
    void errorDataCnMember(CodeLoc loc);
    void errorBlockNotFound(CodeLoc loc, NamePool::Id name);
    void errorBlockNoPass(CodeLoc loc);
    void errorMemberIndex(CodeLoc loc);
    void errorLenOfBadType(CodeLoc loc);
    void errorMissingTypeAttribute(CodeLoc loc);
    void errorMissingType(CodeLoc loc);
    void errorTypeCannotCompile(CodeLoc loc, TypeTable::Id ty);
    void errorNotEvalVal(CodeLoc loc);
    void errorNotEvalFunc(CodeLoc loc);
    void errorNotCompiledVal(CodeLoc loc);
    void errorNotCompiledFunc(CodeLoc loc);
    void errorBadTransfer(CodeLoc loc);
    void errorDropFuncBadSig(CodeLoc loc);
    void errorTransferNoDrop(CodeLoc loc);
    void errorNotGlobalScope(CodeLoc loc);
    void errorNotLocalScope(CodeLoc loc);
    void errorNotTopmost(CodeLoc loc);
    void errorNoMain();
    // placeholder error, should not stay in code
    void errorEvaluationNotSupported(CodeLoc loc);
    // placeholder error, should not stay in code
    void errorUnknown(CodeLoc loc);
    void errorInternal(CodeLoc loc);
};