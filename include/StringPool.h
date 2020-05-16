#pragma once

#include <unordered_map>
#include <string>

class StringPool {
public:
    typedef unsigned Id;

private:
    Id next;

    std::unordered_map<Id, std::string> strings;
    std::unordered_map<std::string, Id> ids;

public:
    StringPool();

    Id add(const std::string &str);
    const std::string& get(Id id) const { return strings.at(id); }
};