#include "CompileMessages.h"
#include <sstream>
#include <filesystem>
#include "NamePool.h"
using namespace std;

string CompileMessages::errorStringOfType(TypeTable::Id ty) const {
    string fallback("<unknown>");

    stringstream ss;

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

void CompileMessages::errorNotSimple(CodeLoc loc) {
    error(loc, "Statement not one of: declaration, expression, empty.");
}

void CompileMessages::errorNotPrim(CodeLoc loc) {
    error(loc, "Expected an expression, could not parse one.");
}

void CompileMessages::errorNotTypeId(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Expected a type identifier, instead found '" << namePool->get(name) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorBadArraySize(CodeLoc loc, long int size) {
    stringstream ss;
    ss << "Array size must be a non-negative integer. Size " << size << " is invalid.";
    error(loc, ss.str());
}

void CompileMessages::errorSwitchNoBranches(CodeLoc loc) {
    error(loc, "Switch statements must contain at least one branch.");
}

void CompileMessages::errorSwitchMultiElse(CodeLoc loc) {
    error(loc, "Switch statements may have at most one else branch.");
}

void CompileMessages::errorSwitchNotIntegral(CodeLoc loc) {
    error(loc, "Switch statement can only match on integer and unsigned types.");
}

void CompileMessages::errorSwitchMatchDuplicate(CodeLoc loc) {
    error(loc, "Duplicate matching value in a switch statement.");
}

void CompileMessages::errorNotLastParam(CodeLoc loc) {
    error(loc, "No further parameters are allowed in this function signature.");
}

void CompileMessages::errorBadAttr(CodeLoc loc, Token::Attr attr) {
    stringstream ss;
    ss << "Attribute '" << errorString(attr) << "' is not recognized in this context.";
    error(loc, ss.str());
}

void CompileMessages::errorEmptyArr(CodeLoc loc) {
    error(loc, "Empty arrays are not allowed.");
}

void CompileMessages::errorNonUnOp(CodeLoc loc, Token op) {
    error(loc, "Expected a unary operation.");
}

void CompileMessages::errorNonBinOp(CodeLoc loc, Token op) {
    error(loc, "Expected a binary operation.");
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

void CompileMessages::errorDataNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' is already taken and cannot be used for a data type.";
    error(loc, ss.str());
}

void CompileMessages::errorDataMemberNameDuplicate(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' was already used for another member of this data type.";
    error(loc, ss.str());
}

void CompileMessages::errorDataNoMembers(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Data type '" << namePool->get(name) << "' has no members, but must have at least one.";
    error(loc, ss.str());
}

void CompileMessages::errorDataRedefinition(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Data type '" << namePool->get(name) << "' has already been defined.";
    error(loc, ss.str());
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

void CompileMessages::errorUndefinedType(CodeLoc loc) {
    error(loc, "Referencing undefined type.");
}

void CompileMessages::errorUnknown(CodeLoc loc) {
    error(loc, "Unknown error occured.");
}