---
layout: default
title: Symbols
---
# Symbols

Symbols (also known as variables) are used to store values. They work about the same to what you might be used to in other languages.

To declare a symbol, use the `sym` special form.

```
import "std/io.orb";

fnc main () () {
    sym (x (std.scanI32));
    std.println x;
};
```

Here we declare a symbol `x` and initialize it with a `i32` value which is read from the standard input. In this case, the type of `x` will be equal to the type of its initialization value. After that, we print the value of `x` to the standard output.

We can control the type of our symbol by specifying it. The initalizer value will be implicitly cast to that type.

```
    sym (x:i64 (std.scanI32));
```

Alternatively, we can omit the initialization, in which case we must specify the type. Doing so will zero-initialize our symbol.

```
fnc main () () {
    sym i:i32 # i is 0
        u:u32 # u is 0
        f:f32 # f is 0.0
        b:bool # b is false
        c:c8 # c is '\0'
        p:ptr; # p is null
};
```

Also, notice that a single `sym` instruction can be used to declare multiple symbols.

Using `sym` outside of functions, in global scope, would declare a global symbol (or global variable). These exist in global scope, so they can be referenced for the remainder of the program.

```
import "std/io.orb";

sym x:i32;

fnc main () () {
    std.println x;
};
```