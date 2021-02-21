#pragma once

#include <optional>

struct LifetimeInfo {
    struct NestLevel {
        std::size_t callable, local;
    };

    bool noDrop = false;
    std::optional<NestLevel> nestLevel;
};