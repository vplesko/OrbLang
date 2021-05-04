---
layout: default
title: Type and value inspection
---
# {{ page.title }}

## `typeOf`

The `typeOf` special form is used to get the type of a value at compile-time. The result is an evaluated value of type `type`.

```
import "std/io.orb";

fnc main () () {
    sym x:bool;
    typeOf x; # bool

    typeOf (std.scanI32); # i32
};
```

## `lenOf`

The `lenOf` special form can be used to get the number of elements in an array, a tuple, a data type, or a `raw` value. It can be used either on a type or a value of that type.

Alternatively, it can get the length of an evaluated string value, including the null character at the end.

```
fnc main () () {
    lenOf (i32 f32 bool); # 3

    sym x:(i32 10);
    lenOf x; # 10

    lenOf "ola!"; # 5
};
```

## `sizeOf`

The `sizeOf` special form returns the memory size in bytes of a type. It can only be used on types that can be compiled. Note that the result depends on the machine architecture for which the program is being compiled.

```
    sizeOf ((i32 c8) 4); # 32 on most modern machines
```

## `??`

The `??` special form returns whether there is an existing definition with a given name. If it returns `true`, a lookup on that name will result in a typed value.

```
data Foo;

fnc main () () {
    sym x:i32;
    ?? x; # true

    ?? main; # true

    ?? c8; # true

    ?? Foo; # true

    ?? nonexistent; # false

    ?? sym; # false
};
```