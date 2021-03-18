#pragma once

#include <memory>
#include <unordered_map>
#include "NamePool.h"

class NodeVal;

struct AttrMap {
    std::unordered_map<NamePool::Id, std::unique_ptr<NodeVal>, NamePool::Id::Hasher> attrMap;

private:
    void copyFrom(const AttrMap &other);

public:
    AttrMap() = default;

    AttrMap(const AttrMap &other);
    void operator=(const AttrMap &other);

    ~AttrMap();
};