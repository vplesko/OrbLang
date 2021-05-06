#pragma once

#include <vector>
#include <string>
#include "CodeLoc.h"
#include "reserved.h"
#include "SymbolTable.h"
#include "Token.h"
#include "TypeTable.h"

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
    std::string errorStringOfOper(Oper op) const;
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
    void hintAttrDoubleColon();
    void hintTransferWithMove();
    void hintAttrGlobal();
    void hintDropFuncSig();
    void hintGlobalCompiledLoad();
    void hintBlockSyntax();
    void hintIndexTempOwning();
    void hintUnescapeEscaped();
    void hintWhileProcessingRetType(CodeLoc loc);

    void warnUnusedSpecial(CodeLoc loc, SpecialVal spec);
    void warnUnusedFunc(CodeLoc loc);
    void warnUnusedMacro(CodeLoc loc);
    void warnBlockNameIsType(CodeLoc loc, NamePool::Id name);
    void warnMacroArgTyped(CodeLoc loc);

    void errorInputFileNotFound(const std::string &path);
    void errorBadToken(CodeLoc loc);
    void errorBadLiteral(CodeLoc loc);
    void errorUnclosedMultilineComment(CodeLoc loc);
    void errorImportNotString(CodeLoc loc);
    void errorImportNotFound(CodeLoc loc, const std::string &path);
    void errorImportCyclical(CodeLoc loc, const std::string &path);
    void errorMessageMultiLevel(CodeLoc loc);
    void errorMessageBadType(CodeLoc loc, TypeTable::Id ty);
    void errorUnexpectedTokenType(CodeLoc loc, Token see);
    void errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see);
    void errorUnexpectedKeyword(CodeLoc loc, Keyword keyw);
    void errorUnexpectedNonLeafStart(CodeLoc loc);
    void errorUnexpectedIsNotTerminal(CodeLoc loc);
    void errorUnexpectedNotEmpty(CodeLoc loc);
    void errorUnexpectedIsTerminal(CodeLoc loc);
    void errorUnexpectedNotId(CodeLoc loc);
    void errorUnexpectedNotType(CodeLoc loc);
    void errorUnexpectedNotBool(CodeLoc loc);
    void errorChildrenNotEq(CodeLoc loc, std::size_t cnt, std::size_t see);
    void errorChildrenLessThan(CodeLoc loc, std::size_t cnt);
    void errorChildrenMoreThan(CodeLoc loc, std::size_t cnt);
    void errorChildrenNotBetween(CodeLoc loc, std::size_t lo, std::size_t hi, std::size_t see);
    void errorInvalidTypeDecorator(CodeLoc loc);
    void errorInvalidTypeArg(CodeLoc loc);
    void errorUndefType(CodeLoc loc, TypeTable::Id ty);
    void errorBadArraySize(CodeLoc loc, long int size);
    void errorNonUnOp(CodeLoc loc, Oper op);
    void errorNonBinOp(CodeLoc loc, Oper op);
    void errorNameTaken(CodeLoc loc, NamePool::Id name);
    void errorSymCnNoInit(CodeLoc loc, NamePool::Id name);
    void errorSymGlobalOwning(CodeLoc loc, NamePool::Id name, TypeTable::Id ty);
    void errorSymbolNotFound(CodeLoc loc, NamePool::Id name);
    void errorCnNoInit(CodeLoc loc, NamePool::Id name);
    void errorExprCannotPromote(CodeLoc loc);
    void errorExprCannotPromote(CodeLoc loc, TypeTable::Id into);
    void errorExprBadOps(CodeLoc loc, Oper op, bool unary, TypeTable::Id ty, bool eval);
    void errorExprBinDivByZero(CodeLoc loc);
    void errorExprBinLeftShiftOfNeg(CodeLoc loc, std::int64_t shift);
    void errorExprBinShiftByNeg(CodeLoc loc, std::int64_t shift);
    void errorExprCmpNeArgNum(CodeLoc loc);
    void errorExprAddrOfNonRef(CodeLoc loc);
    void errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotImplicitCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into);
    void errorExprCannotImplicitCastEither(CodeLoc loc, TypeTable::Id ty1, TypeTable::Id ty2);
    void errorExprIndexOnBadType(CodeLoc loc, TypeTable::Id ty);
    void errorExprIndexOutOfBounds(CodeLoc loc, std::int64_t ind, std::optional<std::uint64_t> len);
    void errorExprIndexOutOfBounds(CodeLoc loc, std::uint64_t ind, std::optional<std::uint64_t> len);
    void errorExprDerefOnBadType(CodeLoc loc, TypeTable::Id ty);
    void errorExprDerefNull(CodeLoc loc);
    void errorExprIndexNotIntegral(CodeLoc loc);
    void errorExprIndexNull(CodeLoc loc);
    void errorExprUnOnNull(CodeLoc loc);
    void errorExprAsgnNonRef(CodeLoc loc);
    void errorExprAsgnOnCn(CodeLoc loc);
    void errorExprIndexInvalidBase(CodeLoc loc);
    void errorExprMoveNoDrop(CodeLoc loc);
    void errorExprMoveInvokeArg(CodeLoc loc);
    void errorExprMoveCn(CodeLoc loc);
    void errorExitNowhere(CodeLoc loc);
    void errorExitPassingBlock(CodeLoc loc);
    void errorPassNonPassingBlock(CodeLoc loc);
    void errorNonEvalBlock(CodeLoc loc);
    void errorLoopNowhere(CodeLoc loc);
    void errorFuncNoRet(CodeLoc loc);
    void errorMacroNoRet(CodeLoc loc);
    void errorRetValue(CodeLoc loc);
    void errorRetNoValue(CodeLoc loc, TypeTable::Id shouldRet);
    void errorRetNonEval(CodeLoc loc);
    void errorRetNowhere(CodeLoc loc);
    void errorFuncNameTaken(CodeLoc loc, NamePool::Id name);
    void errorFuncCollisionNoNameMangle(CodeLoc loc, NamePool::Id name, CodeLoc codeLocOther);
    void errorFuncCollision(CodeLoc loc, NamePool::Id name, CodeLoc codeLocOther);
    void errorFuncNotEvalOrCompiled(CodeLoc loc);
    void errorMacroNameTaken(CodeLoc loc, NamePool::Id name);
    void errorMacroTypeBadArgNumber(CodeLoc loc);
    void errorMacroArgAfterVariadic(CodeLoc loc);
    void errorMacroArgPreprocessAndPlusEscape(CodeLoc loc);
    void errorMacroCollision(CodeLoc loc, NamePool::Id name, CodeLoc codeLocOther);
    void errorMacroCollisionVariadic(CodeLoc loc, NamePool::Id name, CodeLoc codeLocOther);
    void errorArgNameDuplicate(CodeLoc loc, NamePool::Id name);
    void errorFuncNotFound(CodeLoc loc, std::vector<TypeTable::Id> argTys, std::optional<NamePool::Id> name = std::nullopt);
    void errorFuncNotFound(CodeLoc loc, NamePool::Id name, TypeTable::Id ty);
    void errorFuncNoDef(CodeLoc loc);
    void errorFuncNoValue(CodeLoc loc);
    void errorFuncCallAmbiguous(CodeLoc loc, std::vector<CodeLoc> codeLocsCand);
    void errorMacroNotFound(CodeLoc loc, NamePool::Id name);
    void errorMacroNoValue(CodeLoc loc);
    void errorDataCnElement(CodeLoc loc);
    void errorDataRedefinition(CodeLoc loc, NamePool::Id name);
    void errorBlockBareNameType(CodeLoc loc);
    void errorBlockNotFound(CodeLoc loc, NamePool::Id name);
    void errorBlockNoPass(CodeLoc loc);
    void errorElementIndexData(CodeLoc loc, NamePool::Id name, TypeTable::Id ty);
    void errorLenOfBadType(CodeLoc loc, TypeTable::Id ty);
    void errorMissingTypeAttribute(CodeLoc loc);
    void errorMissingType(CodeLoc loc);
    void errorTypeCannotCompile(CodeLoc loc, TypeTable::Id ty);
    void errorNotEvalVal(CodeLoc loc);
    void errorNotEvalFunc(CodeLoc loc);
    void errorNotCompiledVal(CodeLoc loc);
    void errorNotCompiledFunc(CodeLoc loc);
    void errorBadTransfer(CodeLoc loc);
    void errorOwning(CodeLoc loc);
    void errorDropFuncBadSig(CodeLoc loc);
    void errorTransferNoDrop(CodeLoc loc);
    void errorTransferInvokeArgs(CodeLoc loc);
    void errorNonTypeAttributeType(CodeLoc loc);
    void errorAttributeNotFound(CodeLoc loc, NamePool::Id name);
    void errorAttributesSameName(CodeLoc loc, NamePool::Id name);
    void errorAttributeOwning(CodeLoc loc, NamePool::Id name);
    void errorNotGlobalScope(CodeLoc loc);
    void errorNotLocalScope(CodeLoc loc);
    void errorNotTopmost(CodeLoc loc);
    void errorNoMain();
    void errorMainNoDef();
    // placeholder error, should not stay in code
    void errorEvaluationNotSupported(CodeLoc loc);
    // placeholder error, should not stay in code
    void errorUnknown(CodeLoc loc);
    void errorInternal(CodeLoc loc);
};