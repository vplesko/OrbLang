#pragma once

#include <vector>
#include <string>
#include "CodeLoc.h"

class CompileMessages {
    std::vector<std::string> errors;

public:

    bool isPanic() const { return !errors.empty(); }

    const std::vector<std::string>& getErrors() const { return errors; }

    void errorUnknown(CodeLoc loc);
};