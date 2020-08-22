#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>

class NamePool {
public:
    typedef unsigned Id;

private:
    Id next;

    std::unordered_map<Id, std::string> names;
    std::unordered_map<std::string, Id> ids;

    std::unordered_set<Id> meaningfuls, keywords, opers;

public:
    NamePool();

    Id add(const std::string &name);
    const std::string& get(Id id) const { return names.at(id); }

    void addMeaningful(Id id) { meaningfuls.insert(id); }
    void addKeyword(Id id) { keywords.insert(id); }
    void addOper(Id id) { opers.insert(id); }

    bool isMeaningful(Id id) const { return meaningfuls.contains(id); }
    bool isKeyword(Id id) const { return keywords.contains(id); }
    bool isOper(Id id) const { return opers.contains(id); }
    bool isReserved(Id id) const;
};