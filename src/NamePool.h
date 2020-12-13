#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>

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

    Id addMain(const std::string &name);
    Id getMainId() const { return main; }
    const std::string& getMain() const { return names.at(main); }
};