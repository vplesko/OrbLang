#include "NamePool.h"
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