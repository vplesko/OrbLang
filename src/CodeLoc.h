#pragma once

#include <string>
#include "StringPool.h"

typedef std::size_t CodeIndex;

struct CodeLoc {
    StringPool::Id file;
    CodeIndex ln, col;
};