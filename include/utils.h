#pragma once

#include <string>
#include <functional>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
constexpr bool isOsWindows = true;
#else
constexpr bool isOsWindows = false;
#endif

template<typename T>
bool between(T x, T lo, T hi) {
    return x >= lo && x <= hi;
}

struct UnescapePayload {
    std::string unescaped;
    std::size_t nextIndex;
    bool success;

    UnescapePayload(std::size_t nextInd) : nextIndex(nextInd), success(false) {}
    UnescapePayload(const std::string &unesc_, std::size_t nextInd) :
        unescaped(unesc_), nextIndex(nextInd), success(true) {}
};

// Unescapes a string starting from the index after the one given and
// ending at the index of the next quote (after unescaping).
// The given index of a quote is assumed to be correct.
//
// On success, returns the unescaped string and the index one after the closing quote.
// On fail, returns the index one after the last successfully unescaped char.
//
// Unescape sequences are: \', \", \?, \\, \a, \b, \f, \n, \r, \t, \v, \0,
// and \xNN (where N is a hex digit in [0-9a-fA-F]).
UnescapePayload unescape(const std::string &str, std::size_t indexStartingQuote, bool isSingleQuote);

class DeferredCallback {
    std::function<void(void)> func;

public:
    DeferredCallback(std::function<void(void)> func_) : func(func_) {}

    DeferredCallback(const DeferredCallback&) = delete;
    void operator=(const DeferredCallback&) = delete;

    virtual ~DeferredCallback() { func(); }
};

std::size_t leNiceHasheFunctione(std::size_t x, std::size_t y);