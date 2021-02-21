#pragma once

#include <optional>

struct LifetimeInfo {
    struct NestLevel {
        std::size_t callable;
        std::size_t local;

        bool callableGreaterThan(NestLevel other) const { return callable < other.callable; }
        bool greaterThan(NestLevel other) const;
    };

    bool noDrop = false;
    std::optional<NestLevel> nestLevel;
};