---
layout: default
title: Symbols
---
# {{ page.title }}

Symbols (also known as variables) are used to store values. To declare a symbol, use the `sym` special form.

```
import "std/io.orb";

fnc main () () {
    sym (x (std.scanI32));
    std.println x;
};
```

Here we declare a symbol `x` and initialize it with an `i32` value read from the standard input. In this case, the type of `x` will be equal to the type of its initialization value. Then, we print `x` to the standard output.

We can control the type of our symbol by specifying it after `:`. The initialization value will be implicitly cast to that type.

```
    sym (x:i64 (std.scanI32));
```

We can omit the initialization, but then we have to specify the type. This will zero-initialize our symbol.

```
    sym i:i32  # i is 0
        u:u32  # u is 0
        f:f32  # f is 0.0
        b:bool # b is false
        c:c8   # c is '\0'
        p:ptr; # p is null
```

Notice how a single `sym` instruction can be used to declare multiple symbols.