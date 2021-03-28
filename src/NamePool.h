#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>

class NamePool {
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
    Id main;

    std::unordered_map<Id, std::string, Id::Hasher> names;
    std::unordered_map<std::string, Id> ids;

public:
    NamePool();

    Id add(const std::string &name);
    const std::string& get(Id id) const { return names.at(id); }

    Id addMain(const std::string &name);
    Id getMainId() const { return main; }
    const std::string& getMain() const { return names.at(main); }

    // for debugging
    void printAll() const;
};