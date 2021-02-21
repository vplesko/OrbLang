#include "LifetimeInfo.h"
using namespace std;

bool LifetimeInfo::NestLevel::greaterThan(LifetimeInfo::NestLevel other) const {
    return (callable < other.callable) || (callable == other.callable && local < other.local);
}