#pragma once

#include <string>
#include "StringPool.h"

// TODO move different compile values in here

struct CompilerAction {
    enum class Kind {
        kNoAction,
        kImport
    };

    Kind kind;

    StringPool::Id file;

    CompilerAction() : kind(Kind::kNoAction) {}
};