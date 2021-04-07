#include "CompilationMessages.h"
#include <sstream>
#include <filesystem>
#include <fstream>
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

string CompilationMessages::errorStringOfOper(Oper op) const {
    return namePool->get(getOperNameId(op));
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

        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0) ss << ' ';
            ss << errorStringOfType(tuple.elements[i]);
        }

        ss << ')';
    } else if (typeTable->isFixedType(ty)) {
        ss << namePool->get(typeTable->getFixedType(ty).name);
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

string toString(CodeLoc loc, const StringPool *stringPool) {
    stringstream ss;
    const string &file = stringPool->get(loc.start.file);
    ss << filesystem::relative(file).string();
    ss << ':' << loc.start.ln << ':' << loc.start.col << ':';
    return ss.str();
}

void CompilationMessages::heading(CodeLoc loc) {
    (*out) << terminalSetBold() << toString(loc, stringPool) << ' ' << terminalReset();
}

void CompilationMessages::bolded(const string &str) {
    (*out) << terminalSetBold() << str << terminalReset();
}

void CompilationMessages::info() {
    raise(S_INFO);
    (*out) << terminalSet(TerminalColor::C_BLACK, true) << "info: " << terminalReset();
}

void CompilationMessages::info(const string &str) {
    info();
    bolded(str);
    (*out) << endl;
}

void CompilationMessages::info(CodeLoc loc, const string &str) {
    heading(loc);
    info(str);
    displayCodeSegment(loc);
}

void CompilationMessages::warning() {
    raise(S_WARNING);
    (*out) << terminalSet(TerminalColor::C_MAGENTA, true) << "warning: " << terminalReset();
}

void CompilationMessages::warning(const string &str) {
    warning();
    bolded(str);
    (*out) << endl;
}

void CompilationMessages::warning(CodeLoc loc, const string &str) {
    heading(loc);
    warning(str);
    displayCodeSegment(loc);
}

void CompilationMessages::error() {
    raise(S_ERROR);
    (*out) << terminalSet(TerminalColor::C_RED, true) << "error: " << terminalReset();
}

void CompilationMessages::error(const string &str) {
    error();
    bolded(str);
    (*out) << endl;
}

void CompilationMessages::error(CodeLoc loc, const string &str) {
    heading(loc);
    error(str);
    displayCodeSegment(loc);
}

void CompilationMessages::displayCodeSegment(CodeLoc loc) {
    const string &filename = stringPool->get(loc.start.file);
    ifstream file(filename);
    if (!file.is_open()) return;

    string line;
    for (CodeIndex i = 0; i < loc.start.ln; ++i) getline(file, line);
    (*out) << line << endl;

    bool multiline = loc.start.ln < loc.end.ln;
    CodeIndex start = loc.start.col-1;
    CodeIndex end = multiline ? line.size() : loc.end.col-1;
    if (end <= start) end = start+1;
    while (end > start && isspace(line[end-1])) --end;

    (*out) << terminalSet(TerminalColor::C_GREEN, false);
    for (CodeIndex i = 0; i < start; ++i) (*out) << ' ';
    (*out) << '^';
    for (CodeIndex i = start+1; i < end; ++i) (*out) << '~';
    (*out) << terminalReset() << endl;
}

bool CompilationMessages::userMessageStart(CodeLoc loc, Status s) {
    if (s != S_INFO && s != S_WARNING && s != S_ERROR) return false;

    heading(loc);

    if (s == S_INFO) {
        info();
    } else if (s == S_WARNING) {
        warning();
    } else if (s == S_ERROR) {
        error();
    }

    return true;
}

void CompilationMessages::userMessageEnd() {
    (*out) << endl;
}

void CompilationMessages::userMessage(int64_t x) {
    (*out) << x;
}

void CompilationMessages::userMessage(uint64_t x) {
    (*out) << x;
}

void CompilationMessages::userMessage(double x) {
    (*out) << x;
}

void CompilationMessages::userMessage(char x) {
    (*out) << x;
}

void CompilationMessages::userMessage(bool x) {
    (*out) << (x ? "true" : "false");
}

void CompilationMessages::userMessage(NamePool::Id x) {
    (*out) << namePool->get(x);
}

void CompilationMessages::userMessage(TypeTable::Id x) {
    (*out) << errorStringOfType(x);
}

void CompilationMessages::userMessageNull() {
    (*out) << "null";
}

void CompilationMessages::userMessage(StringPool::Id str) {
    (*out) << stringPool->get(str);
}

void CompilationMessages::hintForgotCloseNode() {
    info("Did you forget to terminate a node with ')', '}', or ';'?");
}

void CompilationMessages::hintAttrDoubleColon() {
    info("Node attributes are prepended with '::'.");
}

void CompilationMessages::hintTransferWithMove() {
    info("You may transfer ownership by first using '>>' operator on the source value.");
}

void CompilationMessages::hintAttrGlobal() {
    info("You may use this instruction in local scopes by providing attribute 'global' on the special form name.");
}

void CompilationMessages::hintDropFuncSig() {
    info("Drop functions must take a single argument of the type they are dropping, marked as 'noDrop'.");
}

void CompilationMessages::hintGlobalCompiledLoad() {
    info("Compiled values cannot be loaded outside of functions. Consider using evaluated symbols for this purpose.");
}

void CompilationMessages::hintBlockSyntax() {
    info("When defining a block provide either the passed type; or block name and passed type; or neither. Both can be replaced with an empty raw value.");
}

void CompilationMessages::hintIndexTempOwning() {
    info("Cannot get owning elements of a value that is about to be dropped. Consider storing this value in a symbol first.");
}

void CompilationMessages::hintUnescapeEscaped() {
    info("This code was escaped. To counteract that, unescape it by prepending ','.");
}

void CompilationMessages::warnUnusedSpecial(CodeLoc loc, SpecialVal spec) {
    optional<Keyword> k = getKeyword(spec.id);

    stringstream ss;
    ss << "Unused special found in a block body";
    if (k.has_value()) {
        ss << ": " << errorStringOfKeyword(k.value());
    }
    ss << ".";
    warning(loc, ss.str());

    hintForgotCloseNode();
}

void CompilationMessages::warnUnusedFunc(CodeLoc loc) {
    warning(loc, "Unused function value found in a block body.");
    hintForgotCloseNode();
}

void CompilationMessages::warnUnusedMacro(CodeLoc loc) {
    warning(loc, "Unused macro value found in a block body.");
    hintForgotCloseNode();
}

void CompilationMessages::warnBlockNameIsType(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Block defined with the name '" << namePool->get(name) << "', which was also the name of a type.";
    warning(loc, ss.str());
    hintBlockSyntax();
}

void CompilationMessages::warnMacroArgTyped(CodeLoc loc) {
    warning(loc, "Found a type attribute on a macro argument.");
}

void CompilationMessages::errorInputFileNotFound(const string &path) {
    stringstream ss;
    ss << "Input file " << path << " does not exists.";
    error(ss.str());
}

void CompilationMessages::errorBadToken(CodeLoc loc) {
    error(loc, "Could not parse token.");
}

void CompilationMessages::errorBadLiteral(CodeLoc loc) {
    error(loc, "Could not parse literal.");
}

void CompilationMessages::errorUnclosedMultilineComment(CodeLoc loc) {
    error(loc, "End of file reached, but a multiline comment was not closed.");
}

void CompilationMessages::errorImportNotString(CodeLoc loc) {
    error(loc, "Import path not a string.");
}

void CompilationMessages::errorImportNotFound(CodeLoc loc, const string &path) {
    stringstream ss;
    ss << "Tried to import a nonexistent file '" << path << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorImportCyclical(CodeLoc loc, const string &path) {
    stringstream ss;
    ss << "To import the file '" << path << "' at this point would introduce a cyclical dependency.";
    error(loc, ss.str());
}

void CompilationMessages::errorMessageMultiLevel(CodeLoc loc) {
    error(loc, "Attempted to set multiple message levels.");
}

void CompilationMessages::errorMessageBadType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Cannot print a message on values of type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedTokenType(CodeLoc loc, Token see) {
    if (see.type == Token::T_END) {
        error(loc, "Unexpected end of file found.");
        hintForgotCloseNode();
    } else {
        stringstream ss;
        ss << "Unexpected token found: " << errorStringOfToken(see) << ".";
        error(loc, ss.str());
    }
}

void CompilationMessages::errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token see) {
    stringstream ss;
    ss << "Unexpected token found." <<
        " Expected " << errorStringOfTokenType(exp) <<
        ", found " << errorStringOfToken(see) << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedKeyword(CodeLoc loc, Keyword keyw) {
    stringstream ss;
    ss << "Unexpected keyword found: " << errorStringOfKeyword(keyw) << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorUnexpectedNonLeafStart(CodeLoc loc) {
    error(loc, "Unexpected starting node when processing a non-leaf node.");
}

void CompilationMessages::errorUnexpectedIsNotTerminal(CodeLoc loc) {
    error(loc, "Non-terminal was not expected at this location.");
}

void CompilationMessages::errorUnexpectedIsTerminal(CodeLoc loc) {
    error(loc, "Terminal was not expected at this location.");
}

void CompilationMessages::errorUnexpectedNotEmpty(CodeLoc loc) {
    error(loc, "Expected an empty raw value.");
}

void CompilationMessages::errorUnexpectedNotId(CodeLoc loc) {
    error(loc, "Value does not represent an id.");
}

void CompilationMessages::errorUnexpectedNotType(CodeLoc loc) {
    error(loc, "Value does not represent a type.");
}

void CompilationMessages::errorUnexpectedNotBool(CodeLoc loc) {
    error(loc, "Value does not represent a boolean.");
}

void CompilationMessages::errorChildrenNotEq(CodeLoc loc, size_t cnt, size_t see) {
    stringstream ss;
    ss << "Number of children nodes must be exactly " << cnt << ".";
    error(loc, ss.str());

    if (see > cnt) hintForgotCloseNode();
}

void CompilationMessages::errorChildrenLessThan(CodeLoc loc, size_t cnt) {
    stringstream ss;
    ss << "Number of children nodes must be at least " << cnt << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorChildrenMoreThan(CodeLoc loc, size_t cnt) {
    stringstream ss;
    ss << "Number of children nodes must be at most " << cnt << ".";
    error(loc, ss.str());

    hintForgotCloseNode();
}

void CompilationMessages::errorChildrenNotBetween(CodeLoc loc, size_t lo, size_t hi, size_t see) {
    stringstream ss;
    ss << "Number of children nodes must be between " << lo << " and " << hi << ".";
    error(loc, ss.str());

    if (see > hi) hintForgotCloseNode();
}

void CompilationMessages::errorInvalidTypeDecorator(CodeLoc loc) {
    error(loc, "Invalid type decorator.");
}

void CompilationMessages::errorInvalidTypeArg(CodeLoc loc) {
    error(loc, "Unexpected value found while processing a type.");
}

void CompilationMessages::errorUndefType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Type '" << errorStringOfType(ty) << "' was not fully defined.";
    error(loc, ss.str());
}

void CompilationMessages::errorBadArraySize(CodeLoc loc, long int size) {
    stringstream ss;
    ss << "Array size must be a non-negative integer. Size " << size << " is invalid.";
    error(loc, ss.str());
}

void CompilationMessages::errorNonUnOp(CodeLoc loc, Oper op) {
    stringstream ss;
    ss << "Operation '" << errorStringOfOper(op) << "' is not unary.";
    error(loc, ss.str());
}

void CompilationMessages::errorNonBinOp(CodeLoc loc, Oper op) {
    stringstream ss;
    ss << "Operation '" << errorStringOfOper(op) << "' is not binary.";
    error(loc, ss.str());
}

void CompilationMessages::errorNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' was already taken.";
    error(loc, ss.str());
}

void CompilationMessages::errorSymCnNoInit(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Attempted to declare a constant symbol '" << namePool->get(name) << "' without an initial value.";
    error(loc, ss.str());
}

void CompilationMessages::errorSymGlobalOwning(CodeLoc loc, NamePool::Id name, TypeTable::Id ty) {
    stringstream ss;
    ss << "Attempted to declare a global owning symbol '" << namePool->get(name) << "', of type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorSymbolNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Symbol with name '" << namePool->get(name) << "' not found.";
    error(loc, ss.str());
}

void CompilationMessages::errorCnNoInit(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Constant with name '" << namePool->get(name) << "' was not initialized.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprCannotPromote(CodeLoc loc) {
    error(loc, "Evaluated value cannot be compiled.");
}

void CompilationMessages::errorExprCannotPromote(CodeLoc loc, TypeTable::Id into) {
    stringstream ss;
    ss << "Evaluated value cannot be compiled and cast to type '" << errorStringOfType(into) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprBadOps(CodeLoc loc, Oper op, bool unary, TypeTable::Id ty, bool eval) {
    stringstream ss;
    ss << (unary ? "Unary" : "Binary") << " operation '" << errorStringOfOper(op) << "' is not defined for" << (eval ? " evaluated" : "") << " operands of type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprBinDivByZero(CodeLoc loc) {
    error(loc, "Attempted division by zero.");
}

void CompilationMessages::errorExprCmpNeArgNum(CodeLoc loc) {
    error(loc, "Attempted to use '!=' operator on more than two operands.");
}

void CompilationMessages::errorExprAddrOfNonRef(CodeLoc loc) {
    error(loc, "Attempted to get a pointer to non-ref value.");
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
    error(loc, "Exit instruction had no enclosing block to exit from.");
}

void CompilationMessages::errorExitPassingBlock(CodeLoc loc) {
    error(loc, "Attempted to exit a passing block.");
}

void CompilationMessages::errorPassNonPassingBlock(CodeLoc loc) {
    error(loc, "Attempted to pass a value from a non-passing block.");
}

void CompilationMessages::errorNonEvalBlock(CodeLoc loc) {
    error(loc, "Cannot evaluate this instruction on compiled block.");
}

void CompilationMessages::errorLoopNowhere(CodeLoc loc) {
    error(loc, "Loop instruction had no enclosing block to loop in.");
}

void CompilationMessages::errorFuncNoRet(CodeLoc loc) {
    error(loc, "Function body ended without a ret instruction.");
}

void CompilationMessages::errorMacroNoRet(CodeLoc loc) {
    error(loc, "Macro execution ended without a ret instruction.");
}

void CompilationMessages::errorRetValue(CodeLoc loc) {
    error(loc, "Ret instruction had a return value in a non-returning function.");
}

void CompilationMessages::errorRetNoValue(CodeLoc loc, TypeTable::Id shouldRet) {
    stringstream ss;
    ss << "Ret instruction had no return value, but should return value of type '" <<
        errorStringOfType(shouldRet) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorRetNonEval(CodeLoc loc) {
    error(loc, "Cannot evaluate this instruction on unevaluable function.");
}

void CompilationMessages::errorRetNowhere(CodeLoc loc) {
    error(loc, "Ret instruction used outside of functions and macros.");
}

void CompilationMessages::errorFuncNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' was already taken and cannot be used for a function.";
    error(loc, ss.str());
}

void CompilationMessages::errorFuncCollisionNoNameMangle(CodeLoc loc, NamePool::Id name, CodeLoc codeLocOther) {
    stringstream ss;
    ss << "Another function named '" << namePool->get(name) << "' already exists. Cannot have multiple functions of the same name if name mangling is disabled.";
    error(loc, ss.str());
    info(codeLocOther, "Other function found here.");
}

void CompilationMessages::errorFuncCollision(CodeLoc loc, NamePool::Id name, CodeLoc codeLocOther) {
    stringstream ss;
    ss << "Collision with another function of same name '" << namePool->get(name) << "'.";
    error(loc, ss.str());
    info(codeLocOther, "Other function found here.");
}

void CompilationMessages::errorFuncNotEvalOrCompiled(CodeLoc loc) {
    error(loc, "Function set as neither evaluable nor compilable.");
}

void CompilationMessages::errorMacroNameTaken(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Name '" << namePool->get(name) << "' was already taken and cannot be used for a macro.";
    error(loc, ss.str());
}

void CompilationMessages::errorMacroTypeBadArgNumber(CodeLoc loc) {
    error(loc, "Argument number in a macro type was not a non-negative integer.");
}

void CompilationMessages::errorMacroArgAfterVariadic(CodeLoc loc) {
    error(loc, "Found macro argument(s) after a variadic argument.");
}

void CompilationMessages::errorMacroArgPreprocessAndPlusEscape(CodeLoc loc) {
    error(loc, "Macro argument set as both preprocess and plus-escape.");
}

void CompilationMessages::errorMacroCollision(CodeLoc loc, NamePool::Id name, CodeLoc codeLocOther) {
    stringstream ss;
    ss << "Collision with another macro of same name '" << namePool->get(name) << "'.";
    error(loc, ss.str());
    info(codeLocOther, "Other macro found here.");
}

void CompilationMessages::errorMacroCollisionVariadic(CodeLoc loc, NamePool::Id name, CodeLoc codeLocOther) {
    stringstream ss;
    ss << "Collision with another macro of same name '" << namePool->get(name) << "' due to ambiguity caused by variadic argument(s).";
    error(loc, ss.str());
    info(codeLocOther, "Other macro found here.");
}

void CompilationMessages::errorArgNameDuplicate(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Argument name '" << namePool->get(name) << "' used more than once.";
    error(loc, ss.str());
}

void CompilationMessages::errorFuncNotFound(CodeLoc loc, vector<TypeTable::Id> argTys, optional<NamePool::Id> name) {
    stringstream ss;
    ss << "No functions";
    if (name.has_value()) ss << " with name '" << namePool->get(name.value()) << "'";
    ss << " satisfying the call signature have been found.";
    error(loc, ss.str());

    ss = stringstream();
    ss << "Provided argument types were:";
    for (TypeTable::Id argTy : argTys) {
        ss << endl << "\t" << errorStringOfType(argTy);
    }
    info(ss.str());
}

void CompilationMessages::errorFuncNotFound(CodeLoc loc, NamePool::Id name, TypeTable::Id ty) {
    stringstream ss;
    ss << "No functions with name '" << namePool->get(name) << "' of type '" << errorStringOfType(ty) << "' have been found.";
    error(loc, ss.str());
}

void CompilationMessages::errorFuncNoDef(CodeLoc loc) {
    error(loc, "Attempted to call a function with no definition.");
}

void CompilationMessages::errorFuncNoValue(CodeLoc loc) {
    error(loc, "Tried to call a function with no value.");
}

void CompilationMessages::errorFuncCallAmbiguous(CodeLoc loc, vector<CodeLoc> codeLocsCand) {
    error(loc, "Function call is ambiguous, multiple potential candidates found.");
    for (CodeLoc codeLocFunc : codeLocsCand) {
        info(codeLocFunc, "Potential candidate found here.");
    }
}

void CompilationMessages::errorMacroNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "No macros with name '" << namePool->get(name) << "' satisfying the invocation signature have been found.";
    error(loc, ss.str());
}

void CompilationMessages::errorMacroNoValue(CodeLoc loc) {
    error(loc, "Tried to invoke a macro with no value.");
}

void CompilationMessages::errorDataCnElement(CodeLoc loc) {
    error(loc, "Data elements may not be constant.");
}

void CompilationMessages::errorDataRedefinition(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Attempted to redefine data type '" << namePool->get(name) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorBlockBareNameType(CodeLoc loc) {
    error(loc, "Bare blocks cannot have names nor pass types. They are simply unscoped sequences of instructions.");
}

void CompilationMessages::errorBlockNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "No enclosing blocks with name '" << namePool->get(name) << "' have been found.";
    error(loc, ss.str());
}

void CompilationMessages::errorBlockNoPass(CodeLoc loc) {
    error(loc, "End of passing block reached without a value being passed.");
}

void CompilationMessages::errorElementIndexData(CodeLoc loc, NamePool::Id name, TypeTable::Id ty) {
    stringstream ss;
    ss << "Invalid element index '" << namePool->get(name) << "' on data type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorLenOfBadType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Cannot get length of type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprIndexOnBadType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Cannot index on type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprIndexOutOfBounds(CodeLoc loc, int64_t ind, optional<uint64_t> len) {
    stringstream ss;
    ss << "Attempted to index with the index of " << ind;
    if (len.has_value()) ss << ", when length is " << len.value();
    ss << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorExprIndexOutOfBounds(CodeLoc loc, uint64_t ind, optional<uint64_t> len) {
    stringstream ss;
    ss << "Attempted to index with the index of " << ind;
    if (len.has_value()) ss << ", when length is " << len.value();
    ss << ".";
    error(loc, ss.str());
}

void CompilationMessages::errorExprDerefOnBadType(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Cannot dereference on type '" << errorStringOfType(ty) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorExprDerefNull(CodeLoc loc) {
    error(loc, "Attempted to dereference a null pointer.");
}

void CompilationMessages::errorExprIndexNotIntegral(CodeLoc loc) {
    error(loc, "Index must be of integer or unsigned type.");
}

void CompilationMessages::errorExprIndexNull(CodeLoc loc) {
    error(loc, "Attempted to index a null array pointer.");
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

void CompilationMessages::errorExprIndexInvalidBase(CodeLoc loc) {
    error(loc, "Invalid expression on left side of '[]' operator.");
}

void CompilationMessages::errorExprMoveNoDrop(CodeLoc loc) {
    error(loc, "Attempted to move a value marked as 'noDrop'.");
}

void CompilationMessages::errorExprMoveInvokeArg(CodeLoc loc) {
    error(loc, "Attempted to move an invocation argument.");
}

void CompilationMessages::errorExprMoveCn(CodeLoc loc) {
    error(loc, "Attempted to move a constant value.");
}

void CompilationMessages::errorMissingTypeAttribute(CodeLoc loc) {
    error(loc, "Type attribute was expected on this node.");
}

void CompilationMessages::errorMissingType(CodeLoc loc) {
    error(loc, "Value does not possess a type.");
}

void CompilationMessages::errorTypeCannotCompile(CodeLoc loc, TypeTable::Id ty) {
    stringstream ss;
    ss << "Type '" << errorStringOfType(ty) << "' cannot be compiled.";
    error(loc, ss.str());
}

void CompilationMessages::errorNotEvalVal(CodeLoc loc) {
    error(loc, "Expected an evaluated value.");
}

void CompilationMessages::errorNotEvalFunc(CodeLoc loc) {
    error(loc, "Expected an evaluated function.");
}

void CompilationMessages::errorNotCompiledVal(CodeLoc loc) {
    error(loc, "Expected a compiled value.");
}

void CompilationMessages::errorNotCompiledFunc(CodeLoc loc) {
    error(loc, "Expected a compiled function.");
}

void CompilationMessages::errorBadTransfer(CodeLoc loc) {
    error(loc, "Invalid transfer of owning value.");
    hintTransferWithMove();
}

void CompilationMessages::errorOwning(CodeLoc loc) {
    error(loc, "Expected a non-owning value.");
}

void CompilationMessages::errorTransferNoDrop(CodeLoc loc) {
    error(loc, "Attempted to transfer a value marked as 'noDrop'.");
}

void CompilationMessages::errorNonTypeAttributeType(CodeLoc loc) {
    error(loc, "Non-type attributes cannot be named 'type'.");
}

void CompilationMessages::errorAttributeNotFound(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Attribute named '" << namePool->get(name) << "' not found.";
    error(loc, ss.str());
}

void CompilationMessages::errorAttributesSameName(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Attribute name '" << namePool->get(name) << "' used more than once.";
    error(loc, ss.str());
}

void CompilationMessages::errorAttributeOwning(CodeLoc loc, NamePool::Id name) {
    stringstream ss;
    ss << "Attempted to set an owning attribute '" << namePool->get(name) << "'.";
    error(loc, ss.str());
}

void CompilationMessages::errorDropFuncBadSig(CodeLoc loc) {
    error(loc, "Bad signature for a drop function.");
}

void CompilationMessages::errorNotGlobalScope(CodeLoc loc) {
    error(loc, "Instruction only allowed in global scope.");
}

void CompilationMessages::errorNotLocalScope(CodeLoc loc) {
    error(loc, "Instruction only allowed inside functions.");
}

void CompilationMessages::errorNotTopmost(CodeLoc loc) {
    error(loc, "This is only allowed at the topmost code level.");
}

void CompilationMessages::errorNoMain() {
    error("No main function found.");
}

void CompilationMessages::errorMainNoDef() {
    error("Main function was declared, but not defined.");
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