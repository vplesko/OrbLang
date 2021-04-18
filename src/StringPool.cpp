#include "StringPool.h"
#include <iostream>
using namespace std;

StringPool::StringPool() {
    next.id = 0;
}

StringPool::Id StringPool::add(const string &str) {
    auto loc = ids.find(str);
    if (loc != ids.end())
        return loc->second;

    ids[str] = next;
    strings[next] = make_pair(str, nullptr);

    Id ret = next;
    next.id += 1;

    return ret;
}

void StringPool::printAll() const {
    vector<pair<Id::IdType, string>> collected;
    for (const auto &it : strings) {
        collected.push_back({it.first.id, it.second.first});
    }
    sort(collected.begin(), collected.end());

    for (const auto &it : collected) {
        cout << it.first << "\t\"" << it.second << "\"" << endl;
    }
}