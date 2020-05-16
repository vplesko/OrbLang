#pragma once

#include <string>

// TODO move different compile values in here

struct CompilerAction {
    enum class Kind {
        kNoAction,
        kImport
    };

    Kind kind;

    std::string file;

    CompilerAction() : kind(Kind::kNoAction) {}
};