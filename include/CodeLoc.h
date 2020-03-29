#pragma once

#include <string>

typedef unsigned CodeIndex;

struct CodeLoc {
    const std::string *file;
    CodeIndex ln, col;
};