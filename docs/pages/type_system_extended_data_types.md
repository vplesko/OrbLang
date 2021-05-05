---
layout: default
title: Data types
---
# {{ page.title }}

Data types are types that contain one or more named elements. They are defined in the global scope using the `data` special form.

```
data Person {
    name:String
    age:i32
};
```

Data types can also be declared, without specifying their elements. These data types are considered undefined types.

```
data MyNode;
```

> Furthermore, any type which contains an element of an undefined type is also an undefined type. Pointers and array pointers to undefined types are considered defined types.

You can get the elements of a data type using the `[]` special form. They can be indexed either by name or by a compile-time known numeric index.

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