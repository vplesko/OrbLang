#include "NamePool.h"
using namespace std;

NamePool::NamePool() {
    next = 0;
    main = add("main");
}

NamePool::Id NamePool::add(const string &name) {
    auto loc = ids.find(name);
    if (loc != ids.end())
        return loc->second;

    ids[name] = next;
    names[next] = name;
    next += 1;
    return next-1;
}