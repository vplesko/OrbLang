#include "StringPool.h"
using namespace std;

StringPool::StringPool() {
    next = 0;
}

StringPool::Id StringPool::add(const string &str) {
    auto loc = ids.find(str);
    if (loc != ids.end())
        return loc->second;

    ids[str] = next;
    strings[next] = make_pair(str, nullptr);
    next += 1;
    return next-1;
}