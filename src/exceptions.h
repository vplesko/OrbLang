#pragma once

/*
This file should contain all exceptions used in the project.
They should be catchable from main, which should exit with a proper return code.
*/

#include <optional>
#include "NamePool.h"

// TODO get rid of this?
struct ExceptionEvaluatorJump {
    std::optional<NamePool::Id> blockName;
    bool isLoop = false, isRet = false;
};