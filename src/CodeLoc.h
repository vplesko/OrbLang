#pragma once

#include <string>
#include "StringPool.h"

typedef std::size_t CodeIndex;

struct CodeLocPoint {
    CodeIndex ln, col;
};

struct CodeLoc {
    StringPool::Id file;
    CodeLocPoint start;
    CodeLocPoint end; // points to a position after code loc
};