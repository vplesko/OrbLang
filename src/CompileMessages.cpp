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

inline void CompileMessages::error(CodeLoc loc, const string &str) {
    stringstream ss;
    ss << toString(loc) << ' ' << str;
    errors.push_back(ss.str());
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

void CompileMessages::errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into) {
    stringstream ss;
    ss << "Expression of type '" << errorStringOfType(from) <<
        "' cannot be implicitly cast into type '" << errorStringOfType(into) << "'.";
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

void CompileMessages::errorUnknown(CodeLoc loc) {
    error(loc, "Unknown error occured.");
}