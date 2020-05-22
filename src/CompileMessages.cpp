#include "CompileMessages.h"
#include <sstream>
#include <filesystem>
#include "NamePool.h"
using namespace std;

string CompileMessages::errorStringOfType(TypeTable::Id ty) const {
    string fallback("<unknown>");

    stringstream ss;

    if (symbolTable->getTypeTable()->isTypeDescr(ty)) {
        const TypeTable::TypeDescr &descr = symbolTable->getTypeTable()->getTypeDescr(ty);

        optional<NamePool::Id> base = symbolTable->getTypeTable()->getTypeName(descr.base);
        if (!base.has_value()) return fallback;

        ss << namePool->get(base.value());
        if (descr.cn) ss << " cn";
        for (size_t i = 0; i < descr.decors.size(); ++i) {
            switch (descr.decors[i].type) {
            case TypeTable::TypeDescr::Decor::D_ARR:
                ss << "[" << descr.decors[i].len << "]";
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
    } else {
        optional<NamePool::Id> name = symbolTable->getTypeTable()->getTypeName(ty);
        if (!name.has_value()) return fallback;

        ss << namePool->get(name.value());
    }

    return ss.str();
}

inline string toString(CodeLoc loc) {
    stringstream ss;
    ss << filesystem::relative(*loc.file);
    ss << ':' << loc.ln << ':' << loc.col << ':';
    return ss.str();
}

inline void CompileMessages::warn(const std::string &str) {
    status = max(status, S_WARN);
    (*out) << "warn: " << str << endl;
}

inline void CompileMessages::warn(CodeLoc loc, const std::string &str) {
    (*out) << toString(loc) << ' ';
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
    (*out) << toString(loc) << ' ';
    error(str);
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

void CompileMessages::errorImportNotFound(CodeLoc loc, std::string &path) {
    stringstream ss;
    ss << "Importing nonexistent file '" << path << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorImportCyclical(CodeLoc loc, std::string &path) {
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

void CompileMessages::errorBreakNowhere(CodeLoc loc) {
    error(loc, "Break statement has no enclosing loop to break from.");
}

void CompileMessages::errorContinueNowhere(CodeLoc loc) {
    error(loc, "Continue statement has no enclosing loop to continue into.");
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

void CompileMessages::errorFuncSigConflict(CodeLoc loc) {
    error(loc, "Function's signature conflicts with a previously defined function.");
}

void CompileMessages::errorFuncArgNameDuplicate(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Argument name '" << namePool->get(name) << "' used more than once in function proto.";
    error(loc, ss.str());
}

void CompileMessages::errorFuncNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    // TODO separate errors for not found and ambiguity
    ss << "Zero or more than one function with name '" << namePool->get(name) << "' satisfying the call signature has been found.";
    error(loc, ss.str());
}

void CompileMessages::errorMemberIndex(CodeLoc loc) {
    error(loc, "Invalid member index.");
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