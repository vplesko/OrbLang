#include "CompileMessages.h"
#include <sstream>
#include <filesystem>
#include "NamePool.h"
using namespace std;

string CompileMessages::errorStringOfType(TypeTable::Id ty) const {
    string fallback("<unknown>");

    stringstream ss;

    if (symbolTable->getTypeTable()->isTypeDescr(ty)) {
        ss << '(';

        const TypeTable::TypeDescr &descr = getTypeTable()->getTypeDescr(ty);

        ss << errorStringOfType(descr.base);
        if (descr.cn) ss << " cn";
        for (size_t i = 0; i < descr.decors.size(); ++i) {
            ss << ' ';
            switch (descr.decors[i].type) {
            case TypeTable::TypeDescr::Decor::D_ARR:
                ss << descr.decors[i].len;
                break;
            case TypeTable::TypeDescr::Decor::D_ARR_PTR:
                ss << "[]";
                break;
            case TypeTable::TypeDescr::Decor::D_PTR:
                ss << "*";
                break;
            default:
                return fallback;
            }
            if (descr.cns[i]) ss << "cn";
        }

        ss << ')';
    } else if (getTypeTable()->isTuple(ty)) {
        ss << '(';

        const TypeTable::Tuple &tuple = getTypeTable()->getTuple(ty);

        for (size_t i = 0; i < tuple.members.size(); ++i) {
            if (i > 0) ss << ' ';
            ss << errorStringOfType(tuple.members[i]);
        }

        ss << ')';
    } else {
        optional<NamePool::Id> name = symbolTable->getTypeTable()->getTypeName(ty);
        if (!name.has_value()) return fallback;

        ss << namePool->get(name.value());
    }

    return ss.str();
}

inline string toString(CodeLoc loc, const StringPool *stringPool) {
    stringstream ss;
    const string &file = stringPool->get(loc.file);
    ss << filesystem::relative(file);
    ss << ':' << loc.ln << ':' << loc.col << ':';
    return ss.str();
}

inline void CompileMessages::warn(const std::string &str) {
    status = max(status, S_WARN);
    (*out) << "warn: " << str << endl;
}

inline void CompileMessages::warn(CodeLoc loc, const std::string &str) {
    (*out) << toString(loc, stringPool) << ' ';
    warn(str);
}

void CompileMessages::warnExprIndexOutOfBounds(CodeLoc loc) {
    warn(loc, "Attempting to index out of bounds of the array.");
}

inline void CompileMessages::error(const string &str) {
    status = max(status, S_ERROR);
    (*out) << "error: " << str << endl;
}

inline void CompileMessages::error(CodeLoc loc, const string &str) {
    (*out) << toString(loc, stringPool) << ' ';
    error(str);
}

void CompileMessages::errorInputFileNotFound(const string &path) {
    stringstream ss;
    ss << "Input file " << path << " does not exists.";
    error(ss.str());
}

void CompileMessages::errorBadToken(CodeLoc loc) {
    error(loc, "Could not parse token at this location.");
}

void CompileMessages::errorUnclosedMultilineComment(CodeLoc loc) {
    error(loc, "End of file reached, but a multiline comment was not closed.");
}

void CompileMessages::errorImportNotString(CodeLoc loc) {
    error(loc, "Import path not a string.");
}

void CompileMessages::errorImportNotFound(CodeLoc loc, const string &path) {
    stringstream ss;
    ss << "Importing nonexistent file '" << path << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorImportCyclical(CodeLoc loc, const string &path) {
    stringstream ss;
    ss << "Importing the file '" << path << "' at this point introduced a cyclical dependency.";
    error(loc, ss.str());
}

void CompileMessages::errorUnexpectedTokenType(CodeLoc loc, Token::Type exp) {
    stringstream ss;
    ss << "Unexpected symbol found. Expected '" << errorString(exp) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token::Type see) {
    stringstream ss;
    ss << "Unexpected symbol found. Expected '" << errorString(exp) << "', instead found '" << errorString(see) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see) {
    stringstream ss;
    ss << "Unexpected symbol found. Expected '" << errorString(exp) << "', instead found '" << errorString(see) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorUnexpectedTokenType(CodeLoc loc, vector<Token::Type> exp, Token see) {
    stringstream ss;
    ss << "Unexpected symbol found. Expected ";
    for (size_t i = 0; i < exp.size(); ++i) {
        if (i > 0) ss << " or ";
        ss << "'" << errorString(exp[i]) << "'";
    }
    ss << ", instead found '" << errorString(see) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorUnexpectedKeyword(CodeLoc loc, Token::Type keyw) {
    stringstream ss;
    ss << "Unexpected keyword '" << errorString(keyw) << "' found.";
    error(loc, ss.str());
}

void CompileMessages::errorUnexpectedIsNotTerminal(CodeLoc loc) {
    error(loc, "Non-terminal was not expected at this location.");
}

void CompileMessages::errorUnexpectedIsTerminal(CodeLoc loc) {
    error(loc, "Terminal was not expected at this location.");
}

void CompileMessages::errorUnexpectedNotKeyword(CodeLoc loc) {
    error(loc, "Result does not present a keyword.");
}

void CompileMessages::errorUnexpectedNotId(CodeLoc loc) {
    error(loc, "Result does not present an id.");
}

void CompileMessages::errorUnexpectedNotFunc(CodeLoc loc) {
    error(loc, "Result does not present a function.");
}

void CompileMessages::errorUnexpectedNotType(CodeLoc loc) {
    error(loc, "Result does not present a type.");
}

void CompileMessages::errorUnexpectedNotBlock(CodeLoc loc) {
    error(loc, "Block was expected at this location.");
}

void CompileMessages::errorUnexpectedNotAttribute(CodeLoc loc) {
    error(loc, "Result does not present an attribute.");
}

void CompileMessages::errorUnexpectedNodeValue(CodeLoc loc) {
    error(loc, "Unexpected node value found.");
}

void CompileMessages::errorChildrenNotEq(CodeLoc loc, std::size_t cnt) {
    stringstream ss;
    ss << "Number of children notes must be exactly " << cnt << ".";
    error(loc, ss.str());
}

void CompileMessages::errorChildrenLessThan(CodeLoc loc, std::size_t cnt) {
    stringstream ss;
    ss << "Number of children notes must be at least " << cnt << ".";
    error(loc, ss.str());
}

void CompileMessages::errorChildrenMoreThan(CodeLoc loc, std::size_t cnt) {
    stringstream ss;
    ss << "Number of children notes must be at most " << cnt << ".";
    error(loc, ss.str());
}

void CompileMessages::errorChildrenNotBetween(CodeLoc loc, std::size_t lo, std::size_t hi) {
    stringstream ss;
    ss << "Number of children notes must be between " << lo << " and " << hi << ".";
    error(loc, ss.str());
}

void CompileMessages::errorInvalidTypeDecorator(CodeLoc loc) {
    error(loc, "Invalid type decorator.");
}

void CompileMessages::errorBadArraySize(CodeLoc loc, long int size) {
    stringstream ss;
    ss << "Array size must be a non-negative integer. Size " << size << " is invalid.";
    error(loc, ss.str());
}

void CompileMessages::errorNotLastParam(CodeLoc loc) {
    error(loc, "No further parameters are allowed in this function signature.");
}

void CompileMessages::errorBadAttr(CodeLoc loc, Token::Attr attr) {
    stringstream ss;
    ss << "Attribute '" << errorString(attr) << "' is not recognized in this context.";
    error(loc, ss.str());
}

void CompileMessages::errorNonUnOp(CodeLoc loc, Token::Oper op) {
    stringstream ss;
    ss << "Operation '" << errorString(op) << "' is not unary.";
    error(loc, ss.str());
}

void CompileMessages::errorNonBinOp(CodeLoc loc, Token::Oper op) {
    stringstream ss;
    ss << "Operation '" << errorString(op) << "' is not binary.";
    error(loc, ss.str());
}

void CompileMessages::errorVarNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Variable (or other entity) with name '" << namePool->get(name) << "' already exists in this scope.";
    error(loc, ss.str());
}

void CompileMessages::errorVarNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Variable with name '" << namePool->get(name) << "' not found.";
    error(loc, ss.str());
}

void CompileMessages::errorCnNoInit(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Constant with name '" << namePool->get(name) << "' is not initialized.";
    error(loc, ss.str());
}

void CompileMessages::errorCnNoInit(CodeLoc loc) {
    error(loc, "Constant is not initialized.");
}

void CompileMessages::errorExprNotBaked(CodeLoc loc) {
    error(loc, "Expression cannot be evaluated at compile time.");
}

void CompileMessages::errorExprCannotPromote(CodeLoc loc, TypeTable::Id into) {
    stringstream ss;
    ss << "Expression cannot be promoted to expected type '" << errorStringOfType(into) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorExprUntyMismatch(CodeLoc loc) {
    error(loc, "Binary operations cannot be performed on untyped operands of mismatching types.");
}

void CompileMessages::errorExprUntyBinBadOp(CodeLoc loc, Token::Oper op) {
    stringstream ss;
    ss << "Binary operation '" << errorString(op) << "' is not defined for untyped values of this type.";
    error(loc, ss.str());
}

void CompileMessages::errorExprCompareStringLits(CodeLoc loc) {
    error(loc, "String literals cannot directly be compared for pointer (in)equality.");
}

void CompileMessages::errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into) {
    stringstream ss;
    ss << "Expression of type '" << errorStringOfType(from) <<
        "' cannot be cast into type '" << errorStringOfType(into) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorExprCannotImplicitCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into) {
    stringstream ss;
    ss << "Expression of type '" << errorStringOfType(from) <<
        "' cannot be implicitly cast into type '" << errorStringOfType(into) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorExprCannotImplicitCastEither(CodeLoc loc, TypeTable::Id ty1, TypeTable::Id ty2) {
    stringstream ss;
    ss << "Expressions of type '" << errorStringOfType(ty1) << "' and '" << errorStringOfType(ty2) <<
        "' cannot both be implicitly cast to one of the two types.";
    error(loc, ss.str());
}

void CompileMessages::errorExitNowhere(CodeLoc loc) {
    error(loc, "Exit statement has no enclosing block to exit from.");
}

void CompileMessages::errorLoopNowhere(CodeLoc loc) {
    error(loc, "Loop statement has no enclosing block to loop in.");
}

void CompileMessages::errorRetNoValue(CodeLoc loc, TypeTable::Id shouldRet) {
    stringstream ss;
    ss << "Ret statement has no return value, but should return value of type '" <<
        errorStringOfType(shouldRet) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorFuncNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' is already taken and cannot be used for a function.";
    error(loc, ss.str());
}

void CompileMessages::errorMacroNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' is already taken and cannot be used for a macro.";
    error(loc, ss.str());
}

void CompileMessages::errorSigConflict(CodeLoc loc) {
    error(loc, "Signature conflicts with a previously defined function or macro.");
}

void CompileMessages::errorArgNameDuplicate(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Argument name '" << namePool->get(name) << "' used more than once.";
    error(loc, ss.str());
}

void CompileMessages::errorFuncNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    // TODO if untyped vals get removed, print the signature for attempted function call
    ss << "No functions with name '" << namePool->get(name) << "' satisfying the call signature have been found.";
    error(loc, ss.str());
}

void CompileMessages::errorFuncAmbigious(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "More than one function with name '" << namePool->get(name) << "' satisfying the call signature have been found.";
    error(loc, ss.str());
}

void CompileMessages::errorMacroNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    // TODO print the signature for attempted macro invocation
    ss << "No macros with name '" << namePool->get(name) << "' satisfying the invocation signature have been found.";
    error(loc, ss.str());
}

void CompileMessages::errorMemberIndex(CodeLoc loc) {
    error(loc, "Invalid member index.");
}

void CompileMessages::errorTupleValueMember(CodeLoc loc) {
    error(loc, "Unexpected value for a tuple member.");
}

void CompileMessages::errorExprCallVariadUnty(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Attempting to call variadic function '" << namePool->get(name) << "' with an untyped value as variadic argument.";
    error(loc, ss.str());
}

void CompileMessages::errorExprIndexOnBadType(CodeLoc loc) {
    error(loc, "Cannot index on this value.");
}

void CompileMessages::errorExprIndexOnBadType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Cannot index on type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorExprDerefOnBadType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Cannot dereference on type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorExprAddressOfNoRef(CodeLoc loc) {
    error(loc, "Cannot take address of non ref values.");
}

void CompileMessages::errorExprIndexNotIntegral(CodeLoc loc) {
    error(loc, "Index must be of integer or unsigned type.");
}

void CompileMessages::errorExprUnBadType(CodeLoc loc, Token::Oper op) {
    stringstream ss;
    ss << "Unary operation '" << errorString(op) << "' is not allowed on this value.";
    error(loc, ss.str());
}

void CompileMessages::errorExprUnBadType(CodeLoc loc, Token::Oper op, TypeTable::Id ty) {
    stringstream ss;
    ss << "Unary operation '" << errorString(op) << "' is not allowed on type '"
        << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorExprUnOnCn(CodeLoc loc, Token::Oper op) {
    stringstream ss;
    ss << "Operation '" << errorString(op) << "' modifies its operand and cannot be called on a constant type.";
    error(loc, ss.str());
}

void CompileMessages::errorExprUnOnNull(CodeLoc loc, Token::Oper op) {
    stringstream ss;
    ss << "Operation '" << errorString(op) << "' is not allowed on null literal.";
    error(loc, ss.str());
}

void CompileMessages::errorExprAsgnNonRef(CodeLoc loc, Token::Oper op) {
    stringstream ss;
    ss << "Operation '" << errorString(op) << "' and other assignment operations must assign to ref values.";
    error(loc, ss.str());
}

void CompileMessages::errorExprAsgnOnCn(CodeLoc loc, Token::Oper op) {
    stringstream ss;
    ss << "Operation '" << errorString(op) << "' and other assignment operations cannot assign to constant types.";
    error(loc, ss.str());
}

void CompileMessages::errorExprDotInvalidBase(CodeLoc loc) {
    error(loc, "Invalid expression on left side of dot operator.");
}

void CompileMessages::errorExprNotValue(CodeLoc loc) {
    error(loc, "Result does not present a value.");
}

void CompileMessages::errorMismatchTypeAnnotation(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Type annotation '" << errorStringOfType(ty) << "' does not match the type of the value.";
    error(loc, ss.str());
}

void CompileMessages::errorMismatchTypeAnnotation(CodeLoc loc) {
    stringstream ss;
    ss << "Type annotation does not match the type of the value.";
    error(loc, ss.str());
}

void CompileMessages::errorMissingTypeAnnotation(CodeLoc loc) {
    error(loc, "Type annotation was expected on this node.");
}

void CompileMessages::errorNotGlobalScope(CodeLoc loc) {
    error(loc, "This is only allowed in global scope.");
}

void CompileMessages::errorUnknown(CodeLoc loc) {
    error(loc, "Unknown error occured.");
}

void CompileMessages::errorInternal(CodeLoc loc) {
    error(loc, "An internal error occured. You should not be seeing this.");
}