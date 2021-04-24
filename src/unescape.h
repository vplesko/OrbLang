#pragma once

#include <string>

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
