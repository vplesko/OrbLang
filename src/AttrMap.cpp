#include "AttrMap.h"
#include "NodeVal.h"
using namespace std;

void AttrMap::copyFrom(const AttrMap &other) {
    attrMap.clear();
    for (const auto &loc : other.attrMap) {
        attrMap.insert({loc.first, make_unique<NodeVal>(*loc.second)});
    }
}

AttrMap::AttrMap(const AttrMap &other) {
    copyFrom(other);
}

AttrMap& AttrMap::operator=(const AttrMap &other) {
    if (this != &other) {
        copyFrom(other);
    }
    return *this;
}

AttrMap::~AttrMap() {}