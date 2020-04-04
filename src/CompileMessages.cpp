#include "CompileMessages.h"
#include <sstream>
#include <filesystem>
#include "NamePool.h"
using namespace std;

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

void CompileMessages::errorUnexpectedTokenType(CodeLoc loc, Token::Type exp, Token::Type see) {
    stringstream ss;
    ss << "Unexpected symbol found. Expected '" << getStringFor(exp) << "', instead found '" << getStringFor(see) << "'.";
    error(loc, ss.str());
}

void CompileMessages::errorUnknown(CodeLoc loc) {
    error(loc, "Unknown error occured.");
}