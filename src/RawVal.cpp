#include "RawVal.h"
#include "NodeVal.h"
using namespace std;

void RawVal::addChild(NodeVal c) {
    children.push_back(move(c));
}

void RawVal::addChildren(std::vector<NodeVal> c) {
    children.reserve(children.size()+c.size());
    for (auto &it : c) {
        addChild(move(it));
    }
}

RawVal RawVal::copyNoRef(const RawVal &k) {
    RawVal r(k);
    r.ref = nullptr;
    return r;
}