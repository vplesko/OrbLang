#include "CompileMessages.h"
#include <sstream>
#include <filesystem>
using namespace std;

inline string toString(CodeLoc loc) {
    stringstream ss;
    ss << filesystem::relative(*loc.file);
    ss << ':' << loc.ln << ':' << loc.col << ':';
    return ss.str();
}

void CompileMessages::errorUnknown(CodeLoc loc) {
    stringstream ss;
    ss << toString(loc) << ' ' << "Unknown error occured";
    errors.push_back(ss.str());
}