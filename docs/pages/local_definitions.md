---
layout: default
title: Local definitions
---
# {{ page.title }}

To define or declare a function outside of the global scope, use the `::global` attribute. Furthermore, `fnc` always returns the new function as a value, so you may store it in a symbol and use later.

```
import "std/io.orb";

fnc main () () {
    sym (helloFnc (fnc::global hello () () {
        std.println "Hello!";
    }));

    helloFnc;
};
```

The same applies to macros, data types, and explicit types.