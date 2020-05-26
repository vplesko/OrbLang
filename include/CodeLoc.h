#pragma once

#include <string>
#include "StringPool.h"

typedef unsigned CodeIndex;

struct CodeLoc {
    StringPool::Id file;
    CodeIndex ln, col;
};