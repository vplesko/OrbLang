#pragma once

#include <cstddef>

// TODO error type (probably aliased to std::optional)

template<typename T>
bool between(T x, T lo, T hi) {
    return x >= lo && x <= hi;
}

inline std::size_t leNiceHasheFunctione(std::size_t x, std::size_t y) {
    return (17*31+x)*31+y;
}
