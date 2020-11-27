#pragma once

#include <vector>

class NodeVal;

struct RawVal {
    std::vector<NodeVal> children;
    RawVal *ref = nullptr;

    bool isEmpty() const { return getChildrenCnt() == 0; }

    NodeVal& getChild(std::size_t ind) { return children[ind]; }
    const NodeVal& getChild(std::size_t ind) const { return children[ind]; }
    void addChild(NodeVal c);
    void addChildren(std::vector<NodeVal> c);
    std::size_t getChildrenCnt() const { return children.size(); }

    static RawVal copyNoRef(const RawVal &k);
};