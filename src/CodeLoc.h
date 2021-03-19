#pragma once

#include <string>
#include "StringPool.h"

typedef std::size_t CodeIndex;

struct CodeLocPoint {
    StringPool::Id file;
    CodeIndex ln, col;
};

struct CodeLoc {
    CodeLocPoint start;
    CodeLocPoint end; // points to a position after code loc
};