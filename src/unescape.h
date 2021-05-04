#pragma once

#include <string>

struct UnescapePayload {
    enum Status {
        Success,
        SuccessUnclosed,
        Failure
    };

    std::string unescaped;
    std::size_t nextIndex;
    Status status;

    explicit UnescapePayload(std::size_t nextInd) : nextIndex(nextInd), status(Status::Failure) {}
    UnescapePayload(const std::string &unesc_, std::size_t nextInd, Status s = Status::Success) :
        unescaped(unesc_), nextIndex(nextInd), status(s) {}
};

// Unescapes a string starting from the given index and
// ending at the index of the next quote (after unescaping).
// If end of string encountered before closed quote, reports a special result code.
//
// On success, returns the unescaped string and the last successfully unescaped char.
// On fail, returns the index one after the last successfully unescaped char.
//
// Unescape sequences are: \', \", \?, \\, \a, \b, \f, \n, \r, \t, \v, \0,
// and \xNN (where N is a hex digit in [0-9a-fA-F]).
UnescapePayload unescape(const std::string &str, std::size_t indexStarting, bool isSingleQuote);
