#include "CodeLoc.h"
using namespace std;

ostream& operator<<(std::ostream &out, CodeLocPoint locPnt) {
    out << "{ln=" << locPnt.ln << ", col=" << locPnt.col << "}";
    return out;
}

ostream& operator<<(std::ostream &out, CodeLoc loc) {
    out << "CodeLoc(file=" << loc.file << ", start=" << loc.start << ", end=" << loc.end << ")";
    return out;
}