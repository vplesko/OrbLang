#include "utils.h"
using namespace std;

pair<char, bool> nextCh(const string &str, size_t &index) {
    if (index >= str.size()) return {char(), false};
    return {str[index++], true};
}

pair<int, bool> nextHex(const string &str, size_t &index) {
    pair<char, bool> ch = nextCh(str, index);
    if (ch.second == false) return {0, false};

    pair<int, bool> hex;
    hex.second = true;
    if (between(ch.first, '0', '9')) {
        hex.first = ch.first - '0';
    } else if (between(ch.first, 'a', 'f')) {
        hex.first = 10 + (ch.first - 'a');
    } else if (between(ch.first, 'A', 'F')) {
        hex.first = 10 + (ch.first - 'A');
    } else {
        hex.second = false;
    }

    return hex;
}

UnescapePayload unescape(const string &str, std::size_t indexStartingQuote, bool isSingleQuote) {
    string out;
    size_t ind = indexStartingQuote+1;
    size_t afterLastSuccessful;
    while (true) {
        afterLastSuccessful = ind;

        pair<char, bool> ch = nextCh(str, ind);
        if (ch.second == false) break;

        if (ch.first == '\'') {
            if (isSingleQuote) {
                afterLastSuccessful = ind;
                return UnescapePayload(out, afterLastSuccessful);
            } else {
                break;
            }
        } else if (ch.first == '\"') {
            if (!isSingleQuote) {
                afterLastSuccessful = ind;
                return UnescapePayload(out, afterLastSuccessful);
            } else {
                break;
            }
        } else if (isspace(ch.first) && ch.first != ' ') {
            break;
        } else if (ch.first == '\\') {
            ch = nextCh(str, ind);
            if (ch.second == false) break;

            if (ch.first == 'x') {
                pair<int, bool> hex1 = nextHex(str, ind);
                if (hex1.second == false) break;

                pair<int, bool> hex2 = nextHex(str, ind);
                if (hex2.second == false) break;

                out.push_back(16*hex1.first+hex2.first);
            } else {
                switch (ch.first) {
                case '\'':
                    out.push_back('\'');
                    break;
                case '\"':
                    out.push_back('\"');
                    break;
                case '\?':
                    out.push_back('\?');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                case 'a':
                    out.push_back('\a');
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'v':
                    out.push_back('\v');
                    break;
                default:
                    return UnescapePayload(afterLastSuccessful);
                }
            }
        } else {
            out.push_back(ch.first);
        }
    }
    return UnescapePayload(afterLastSuccessful);
}