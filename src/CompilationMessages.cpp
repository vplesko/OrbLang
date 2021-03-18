#include "CompilationMessages.h"
#include <sstream>
#include <filesystem>
#include "NamePool.h"
using namespace std;

string CompilationMessages::errorStringOfTokenType(Token::Type tokTy) const {
    string fallback("<unknown>");

    stringstream ss;

    switch (tokTy) {
    case Token::T_NUM:
        ss << "numeric";
        break;
    case Token::T_FNUM:
        ss << "floating numeric";
        break;
    case Token::T_CHAR:
        ss << "char";
        break;
    case Token::T_BVAL:
        ss << "boolean";
        break;
    case Token::T_STRING:
        ss << "string";
        break;
    case Token::T_NULL:
        ss << "null";
        break;
    case Token::T_SEMICOLON:
        ss << "\';\'";
        break;
    case Token::T_COLON:
        ss << "\':\'";
        break;
    case Token::T_DOUBLE_COLON:
        ss << "\'::\'";
        break;
    case Token::T_BACKSLASH:
        ss << "\'\\\'";
        break;
    case Token::T_COMMA:
        ss << "\',\'";
        break;
    case Token::T_BRACE_L_REG:
        ss << "\'(\'";
        break;
    case Token::T_BRACE_R_REG:
        ss << "\')\'";
        break;
    case Token::T_BRACE_L_CUR:
        ss << "\'{\'";
        break;
    case Token::T_BRACE_R_CUR:
        ss << "\'}\'";
        break;
    case Token::T_ID:
        ss << "identifier";
        break;
    case Token::T_END:
        ss << "<file end>";
        break;
    default:
        ss << fallback;
        break;
    }

    return ss.str();
}

string CompilationMessages::errorStringOfToken(Token tok) const {
    stringstream ss;

    switch (tok.type) {
    case Token::T_NUM:
        ss << tok.num;
        break;
    case Token::T_FNUM:
        ss << tok.T_FNUM;
        break;
    case Token::T_CHAR:
        ss << '\'' << tok.ch << '\'';
        break;
    case Token::T_BVAL:
        ss << (tok.bval ? "true" : "false");
        break;
    case Token::T_STRING:
        ss << '\"' << stringPool->get(tok.stringId) << '\"';
        break;
    case Token::T_ID:
        ss << "\'" << namePool->get(tok.nameId) << "\'";
        break;
    default:
        ss << errorStringOfTokenType(tok.type);
        break;
    }

    return ss.str();
}

string CompilationMessages::errorStringOfKeyword(Keyword k) const {
    return namePool->get(getKeywordNameId(k));
}

string CompilationMessages::errorStringOfType(TypeTable::Id ty) const {
    string fallback("<unknown>");

    stringstream ss;

    if (typeTable->isTypeDescr(ty)) {
        ss << '(';

        const TypeTable::TypeDescr &descr = typeTable->getTypeDescr(ty);

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
            if (descr.cns[i]) ss << " cn";
        }

        ss << ')';
    } else if (typeTable->isTuple(ty)) {
        ss << '(';

        const TypeTable::Tuple &tuple = typeTable->getTuple(ty);

        for (size_t i = 0; i < tuple.members.size(); ++i) {
            if (i > 0) ss << ' ';
            ss << errorStringOfType(tuple.members[i]);
        }

        ss << ')';
    } else if (typeTable->isCustom(ty)) {
        ss << namePool->get(typeTable->getCustom(ty).name);
    } else if (typeTable->isDataType(ty)) {
        ss << namePool->get(typeTable->getDataType(ty).name);
    } else if (typeTable->isCallable(ty)) {
        ss << '(';

        const TypeTable::Callable &callable = typeTable->getCallable(ty);

        if (callable.isFunc) {
            ss << "fnc";

            ss << " (";
            for (size_t i = 0; i < callable.getArgCnt(); ++i) {
                if (i > 0) ss << ' ';
                ss << errorStringOfType(callable.getArgType(i));
                if (callable.getArgNoDrop(i)) ss << "::noDrop";
            }
            ss << ")";
            if (callable.variadic) ss << "::variadic";

            ss << ' ';
            if (callable.hasRet()) ss << errorStringOfType(callable.retType.value());
            else ss << "()";
        } else {
            ss << "mac";

            ss << ' ' << callable.getArgCnt();
            if (callable.variadic) ss << "::variadic";
        }

        ss << ')';
    } else {
        optional<NamePool::Id> name = typeTable->getTypeName(ty);
        if (name.has_value()) {
            ss << namePool->get(name.value());
        } else {
            optional<string> str = typeTable->makeBinString(ty, namePool, false);
            if (str.has_value()) {
                ss << str.value();
            } else {
                return fallback;
            }
        }
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

inline void CompilationMessages::info(const std::string &str) {
    (*out) << "info: " << str << endl;
}

inline void CompilationMessages::info(CodeLoc loc, const std::string &str) {
    (*out) << toString(loc, stringPool) << ' ';
    info(str);
}

inline void CompilationMessages::warn(const std::string &str) {
    status = max(status, S_WARN);
    (*out) << "warn: " << str << endl;
}

inline void CompilationMessages::warn(CodeLoc loc, const std::string &str) {
    (*out) << toString(loc, stringPool) << ' ';
    warn(str);
}

inline void CompilationMessages::error(const string &str) {
    status = max(status, S_ERROR);
    (*out) << "error: " << str << endl;
}

inline void CompilationMessages::error(CodeLoc loc, const string &str) {
    (*out) << toString(loc, stringPool) << ' ';
    error(str);
}

void CompilationMessages::userMessage(CodeLoc loc, std::int64_t x) {
    stringstream ss;
    ss << x;
    info(loc, ss.str());
}

void CompilationMessages::userMessage(CodeLoc loc, std::uint64_t x) {
    stringstream ss;
    ss << x;
    info(loc, ss.str());
}

void CompilationMessages::userMessage(CodeLoc loc, StringPool::Id str) {
    info(loc, stringPool->get(str));
}

void CompilationMessages::errorInputFileNotFound(const string &path) {
    stringstream ss;
    ss << "Input file " << path << " does not exists.";
    error(ss.str());
}

void CompilationMessages::errorBadToken(CodeLoc loc) {
    error(loc, "Could not parse token at this location.");
}

void CompilationMessages::errorUnclosedMultilineComment(CodeLoc loc) {
    error(loc, "End of file reached, but a multiline comment was not closed.");
}

void CompilationMessages::errorImportNotString(CodeLoc loc) {
    error(loc, "Import path not a string.");
}

void CompilationMessages::errorImportNotFound(CodeLoc loc, const string &path) {
    stringstream ss;
    ss << "Importing nonexistent file '" << path << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorImportCyclical(CodeLoc loc, const string &path) {
    stringstream ss;
    ss << "Importing the file '" << path << "' at this point introduced a cyclical dependency.";
    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedTokenType(CodeLoc loc, Token::Type exp) {
    stringstream ss;
    ss << "Unexpected symbol found." <<
        " Expected " << errorStringOfTokenType(exp) << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token::Type see) {
    stringstream ss;
    ss << "Unexpected symbol found." <<
        " Expected " << errorStringOfTokenType(exp) <<
        ", found " << errorStringOfTokenType(see) << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see) {
    stringstream ss;
    ss << "Unexpected symbol found." <<
        " Expected " << errorStringOfTokenType(exp) <<
        ", found " << errorStringOfToken(see) << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedTokenType(CodeLoc loc, vector<Token::Type> exp, Token see) {
    stringstream ss;
    ss << "Unexpected symbol found.";

    ss << " Expected one of: [";
    for (size_t i = 0; i < exp.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << errorStringOfTokenType(exp[i]);
    }
    ss << "]";

    ss << ", found " << errorStringOfToken(see) << ".";

    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedKeyword(CodeLoc loc, Keyword keyw) {
    stringstream ss;
    ss << "Unexpected keyword found: " << errorStringOfKeyword(keyw) << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedIsNotTerminal(CodeLoc loc) {
    error(loc, "Non-terminal was not expected at this location.");
}

void CompilationMessages::errorUnexpectedIsTerminal(CodeLoc loc) {
    error(loc, "Terminal was not expected at this location.");
}

void CompilationMessages::errorUnexpectedNotId(CodeLoc loc) {
    error(loc, "Result does not present an id.");
}

void CompilationMessages::errorUnexpectedNotType(CodeLoc loc) {
    error(loc, "Result does not present a type.");
}

void CompilationMessages::errorChildrenNotEq(CodeLoc loc, std::size_t cnt) {
    stringstream ss;
    ss << "Number of children nodes must be exactly " << cnt << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorChildrenLessThan(CodeLoc loc, std::size_t cnt) {
    stringstream ss;
    ss << "Number of children nodes must be at least " << cnt << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorChildrenMoreThan(CodeLoc loc, std::size_t cnt) {
    stringstream ss;
    ss << "Number of children nodes must be at most " << cnt << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorChildrenNotBetween(CodeLoc loc, std::size_t lo, std::size_t hi) {
    stringstream ss;
    ss << "Number of children nodes must be between " << lo << " and " << hi << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorInvalidTypeDecorator(CodeLoc loc) {
    error(loc, "Invalid type decorator.");
}

void CompilationMessages::errorBadArraySize(CodeLoc loc, long int size) {
    stringstream ss;
    ss << "Array size must be a non-negative integer. Size " << size << " is invalid.";
    error(loc, ss.str());
}

void CompilationMessages::errorNotLastParam(CodeLoc loc) {
    error(loc, "No further parameters are allowed in this function signature.");
}

void CompilationMessages::errorNonUnOp(CodeLoc loc, Oper op) {
    error(loc, "Operation is not unary.");
}

void CompilationMessages::errorNonBinOp(CodeLoc loc, Oper op) {
    error(loc, "Operation is not binary.");
}

void CompilationMessages::errorSymNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Symbol '" << namePool->get(name) << "' already exists in this scope.";
    error(loc, ss.str());
}

void CompilationMessages::errorSymNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Symbol '" << namePool->get(name) << "' not found.";
    error(loc, ss.str());
}

void CompilationMessages::errorCnNoInit(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Constant with name '" << namePool->get(name) << "' is not initialized.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprCannotPromote(CodeLoc loc) {
    error(loc, "Expression cannot be promoted.");
}

void CompilationMessages::errorExprCannotPromote(CodeLoc loc, TypeTable::Id into) {
    stringstream ss;
    ss << "Expression cannot be promoted to expected type '" << errorStringOfType(into) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprEvalBinBadOp(CodeLoc loc) {
    error(loc, "Binary operation is not defined for evaluation values of this type.");
}

void CompilationMessages::errorExprCannotCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into) {
    stringstream ss;
    ss << "Expression of type '" << errorStringOfType(from) <<
        "' cannot be cast into type '" << errorStringOfType(into) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprCannotImplicitCast(CodeLoc loc, TypeTable::Id from, TypeTable::Id into) {
    stringstream ss;
    ss << "Expression of type '" << errorStringOfType(from) <<
        "' cannot be implicitly cast into type '" << errorStringOfType(into) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprCannotImplicitCastEither(CodeLoc loc, TypeTable::Id ty1, TypeTable::Id ty2) {
    stringstream ss;
    ss << "Expressions of type '" << errorStringOfType(ty1) << "' and '" << errorStringOfType(ty2) <<
        "' cannot both be implicitly cast to one of the two types.";
    error(loc, ss.str());
}

void CompilationMessages::errorExitNowhere(CodeLoc loc) {
    error(loc, "Exit statement has no enclosing block to exit from.");
}

void CompilationMessages::errorExitPassingBlock(CodeLoc loc) {
    error(loc, "Attempting to exit a passing block.");
}

void CompilationMessages::errorPassNonPassingBlock(CodeLoc loc) {
    error(loc, "Attempting to pass a value from a non-passing block.");
}

void CompilationMessages::errorLoopNowhere(CodeLoc loc) {
    error(loc, "Loop statement has no enclosing block to loop in.");
}

void CompilationMessages::errorRetNoValue(CodeLoc loc, TypeTable::Id shouldRet) {
    stringstream ss;
    ss << "Ret statement has no return value, but should return value of type '" <<
        errorStringOfType(shouldRet) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorFuncNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' is already taken and cannot be used for a function.";
    error(loc, ss.str());
}

void CompilationMessages::errorMacroNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' is already taken and cannot be used for a macro.";
    error(loc, ss.str());
}

void CompilationMessages::errorArgNameDuplicate(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Argument name '" << namePool->get(name) << "' used more than once.";
    error(loc, ss.str());
}

void CompilationMessages::errorFuncNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "No functions with name '" << namePool->get(name) << "' satisfying the call signature have been found.";
    error(loc, ss.str());
}

void CompilationMessages::errorMacroNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "No macros with name '" << namePool->get(name) << "' satisfying the invocation signature have been found.";
    error(loc, ss.str());
}

void CompilationMessages::errorFuncNoValue(CodeLoc loc) {
    error(loc, "Trying to call a function with no value.");
}

void CompilationMessages::errorMacroNoValue(CodeLoc loc) {
    error(loc, "Trying to invoke a macro with no value.");
}

void CompilationMessages::errorDataCnMember(CodeLoc loc) {
    error(loc, "Data members may not be constant.");
}

void CompilationMessages::errorBlockNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "No enclosing blocks with name '" << namePool->get(name) << "' have been found.";
    error(loc, ss.str());
}

void CompilationMessages::errorBlockNoPass(CodeLoc loc) {
    error(loc, "End of passing block reached without a value being passed.");
}

void CompilationMessages::errorMemberIndex(CodeLoc loc) {
    error(loc, "Invalid member index.");
}

void CompilationMessages::errorLenOfBadType(CodeLoc loc) {
    error(loc, "Cannot get length of this type. lenOf can operate on tuple, array, and raw types.");
}

void CompilationMessages::errorExprIndexOnBadType(CodeLoc loc) {
    error(loc, "Cannot index on this value.");
}

void CompilationMessages::errorExprIndexOnBadType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Cannot index on type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprIndexOutOfBounds(CodeLoc loc) {
    error(loc, "Attempting to index out of bounds of the array.");
}

void CompilationMessages::errorExprDerefOnBadType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Cannot dereference on type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprAddressOfNoRef(CodeLoc loc) {
    error(loc, "Cannot take address of non ref values.");
}

void CompilationMessages::errorExprIndexNotIntegral(CodeLoc loc) {
    error(loc, "Index must be of integer or unsigned type.");
}

void CompilationMessages::errorExprUnBadType(CodeLoc loc) {
    error(loc, "Unary operation is not allowed on this value.");
}

void CompilationMessages::errorExprUnOnNull(CodeLoc loc) {
    error(loc, "Operation is not allowed on null literal.");
}

void CompilationMessages::errorExprAsgnNonRef(CodeLoc loc) {
    error(loc, "Assignment must assign to ref values.");
}

void CompilationMessages::errorExprAsgnOnCn(CodeLoc loc) {
    error(loc, "Assignment cannot assign to constant types.");
}

void CompilationMessages::errorExprDotInvalidBase(CodeLoc loc) {
    error(loc, "Invalid expression on left side of dot operator.");
}

void CompilationMessages::errorMissingTypeAttribute(CodeLoc loc) {
    error(loc, "Type attribute was expected on this node.");
}

void CompilationMessages::errorNotGlobalScope(CodeLoc loc) {
    error(loc, "This is only allowed in global scope.");
}

void CompilationMessages::errorNotTopmost(CodeLoc loc) {
    error(loc, "This is only allowed at the topmost code level.");
}

void CompilationMessages::errorNoMain() {
    error("No main function defined.");
}

void CompilationMessages::errorEvaluationNotSupported(CodeLoc loc) {
    error(loc, "This evaluation is not supported.");
}

void CompilationMessages::errorUnknown(CodeLoc loc) {
    error(loc, "Unknown error occured.");
}

void CompilationMessages::errorInternal(CodeLoc loc) {
    error(loc, "An internal error occured. You should not be seeing this.");
}