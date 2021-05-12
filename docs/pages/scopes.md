---
layout: default
title: Scopes
---
# {{ page.title }}

Every symbol exists within a single scope where it is accessible. This scope is the same as the scope where it was declared. For example, declaring it inside a function body makes it accessible only within that function.

Control flow macros create new scopes, nested within the scope where they were invoked. For those that repeatedly execute a block, each block iteration has a separate scope.

```
fnc foo () () {
    sym x:i32; # x is now defined

    if true {
        sym y:f32; # y is now defined

        repeat 5 {
            # z from the previous iteration no longer exists

            sym z:bool; # z is now defined
        }; # z is no longer be accessible
    }; # y is no longer be accessible
}; # x is no longer be accessible
```

Symbols in a nested scope may have the same name as a symbol in an enclosing scope. Referring to that name will now fetch the symbol from the nested scope. This is called shadowing.

```
import "base.orb";
import "std/io.orb";

fnc main () () {
    sym (x 100);

    if true {
        sym (x 'A'); # previous x is now shadowed
        std.println x; # prints 'A'
    };

    std.println x; # prints 100
};
```

There is one special scope outside of any function called the global scope. Symbols from this scope are defined for the remainder of the program, since every other scope is nested within it.

```
import "std/io.orb";

sym (x "global");

fnc main () () {
    std.println x; # prints "global"
};
```