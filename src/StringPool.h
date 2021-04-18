#pragma once

#include <unordered_map>
#include <string>
#include "llvm/IR/Constant.h"

class StringPool {
public:
    struct Id {
        typedef unsigned IdType;

        IdType id;

        friend bool operator==(const Id &l, const Id &r)
        { return l.id == r.id; }

        friend bool operator!=(const Id &l, const Id &r)
        { return !(l == r); }

        struct Hasher {
            std::size_t operator()(const Id &id) const {
                return std::hash<IdType>()(id.id);
            }
        };
    };

private:
    Id next;

    std::unordered_map<Id, std::pair<std::string, llvm::Constant*>, Id::Hasher> strings;
    std::unordered_map<std::string, Id> ids;

public:
    StringPool();

    Id add(const std::string &str);
    const std::string& get(Id id) const { return strings.at(id).first; }

    llvm::Constant* getLlvm(Id id) const { return strings.at(id).second; }
    void setLlvm(Id id, llvm::Constant *c) { strings[id].second = c; }

    // for debugging
    void printAll() const;
};