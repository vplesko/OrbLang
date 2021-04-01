#pragma once

#include <unordered_map>
#include <string>
#include "llvm/IR/Constant.h"

class StringPool {
public:
    // TODO do the same thing as with NamePool::Id
    typedef unsigned Id;

private:
    Id next;

    std::unordered_map<Id, std::pair<std::string, llvm::Constant*>> strings;
    std::unordered_map<std::string, Id> ids;

public:
    StringPool();

    Id add(const std::string &str);
    const std::string& get(Id id) const { return strings.at(id).first; }

    llvm::Constant* getLlvm(Id id) const { return strings.at(id).second; }
    void setLlvm(Id id, llvm::Constant *c) { strings[id].second = c; }
};