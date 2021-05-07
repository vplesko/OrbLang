#pragma once

#include <cstddef>
#include <cstdint>

// TODO error type (probably aliased to std::optional)

template<typename T>
bool between(T x, T lo, T hi) {
    return x >= lo && x <= hi;
}

inline std::size_t leNiceHasheFunctione(std::size_t x, std::size_t y) {
    return (17*31+x)*31+y;
}

/*
TODO make wrapping work on any C++ compiler

According to the C standard:
1) casting signed to unsigned is well defined as modulo 2^N,
2) operations on unsigneds wrap around,
3) casting unsigned to signed when the value doesn't fit is dependant on the compiler implementation (though is not UB).

3) means that the functions below may not wrap around the same on all compilers.
*/
inline std::int64_t addWithWrap(std::int64_t x, std::int64_t y) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(x)+static_cast<std::uint64_t>(y));
}

inline std::int64_t subWithWrap(std::int64_t x, std::int64_t y) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(x)-static_cast<std::uint64_t>(y));
}

inline std::int64_t mulWithWrap(std::int64_t x, std::int64_t y) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(x)*static_cast<std::uint64_t>(y));
}

inline std::int64_t shlWithWrap(std::int64_t x, std::int64_t y) {
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(x)<<static_cast<std::uint64_t>(y));
}
