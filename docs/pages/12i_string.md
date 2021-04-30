---
layout: default
title: String
---
# {{ page.title }}

`String` is an alias defined in **base.orb** equal to the type of string literals `(c8 cn [])`.

```
import "base.orb";
import "std/io.orb";

fnc print (str:String) () {
    std.println "Printing string: " str;
};

fnc main () () {
    print "Hello!";
    print "Enjoy your stay!";
};
```