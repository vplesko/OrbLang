#pragma once

/*
This file should contain all exceptions defined in the project.
main should not allow any of them to fall through and should exit with the proper return code.
*/

#include <optional>
#include "NamePool.h"

// TODO get rid of this?
struct ExceptionEvaluatorJump {
    std::optional<NamePool::Id> blockName;
    bool isLoop = false, isRet = false;
};