---
layout: default
title: Data types
---
# {{ page.title }}

Data types are named types which contain one or more named elements. They are defined in the global scope using the `data` special form.

```
data Person {
    name:String
    age:i32
};
```

You can get the elements of a data type using the `[]` special form. They get be indexed either by name or by a compile-time known numeric index.

```
fnc print (p:Person) () {
    std.println "Name: " ([] p name);
    std.println "Age: " ([] p age);

    #$
    This would also work:

    std.println "Name: " ([] p 0);
    std.println "Age: " ([] p 1);
    $#
};
```

Data types can be constructed using the `make` macro from **base.orb**.

```
    sym (p (make Person (name "Peter") (age 20)));
```