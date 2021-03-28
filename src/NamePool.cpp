#include "NamePool.h"
#include <algorithm>
#include <iostream>
#include <vector>
using namespace std;

NamePool::NamePool() {
    next.id = 0;
}

NamePool::Id NamePool::add(const string &name) {
    auto loc = ids.find(name);
    if (loc != ids.end())
        return loc->second;

    ids[name] = next;
    names[next] = name;

    NamePool::Id ret = next;
    next.id += 1;

    return ret;
}

NamePool::Id NamePool::addMain(const std::string &name) {
    main = add(name);
    return main;
}

void NamePool::printAll() const {
    vector<pair<NamePool::Id::IdType, string>> collected;
    for (const auto &it : names) {
        collected.push_back({it.first.id, it.second});
    }
    sort(collected.begin(), collected.end());

    for (const auto &it : collected) {
        cout << it.first << '\t' << it.second << endl;
    }
}