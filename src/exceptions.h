#pragma once

#include <optional>
#include "NamePool.h"

struct ExceptionEvaluatorJump {
    std::optional<NamePool::Id> blockName;
    bool isLoop = false, isRet = false;
};