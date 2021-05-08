---
layout: default
title: LenOf
---
# {{ page.title }}

Used to get the length of a type or a value.

## `lenOf oper -> unsigned`

Fetches the length of `oper`.

`oper` must be a typed value.

If `oper` is a `type`, it must be an array, tuple, or data type. In that case, returns the number of elements in that type.

If `oper` is a `raw`, returns its number of elements.

If `oper` is an evaluated value of type `(c8 cn [])` or `(c8 cn [] cn)`, returns the length of its string value. This includes the null character at the end. The value must not be null.

Otherwise, if `oper` is not a `type`, returns one of the above applied to its type.

```
fnc main () () {
    lenOf (i32 f32 bool); # 3

    sym x:(i32 10);
    lenOf x; # 10

    lenOf "ola!"; # 5
};
```