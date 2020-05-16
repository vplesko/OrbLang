#pragma once

#include <unordered_map>
#include <string>

// TODO make a StringLiteralPool

class NamePool {
public:
    typedef unsigned Id;

private:
    Id next;
    Id main;

    std::unordered_map<Id, std::string> names;
    std::unordered_map<std::string, Id> ids;

public:
    NamePool();

    Id add(const std::string &name);
    const std::string& get(Id id) const { return names.at(id); }
    
    Id getMain() const { return main; }
};