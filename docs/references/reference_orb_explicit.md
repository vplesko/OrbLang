---
layout: default
title: Explicit
---
# {{ page.title }}

Used to define explicit types.

## `explicit name<id> ty<type> -> type`

Creates a new explicit type under the given name. Returns the created type.

There must not be a function, macro, special form, type, or global symbol with the same. The name must not be one of `cn`, `*`, or `[]`.

```
explicit MyId std.Size;
```

`::global` must be placed on `explicit` if this instruction is not executed in the global scope.

> An explicit type is not implicitly castable to the type it is based on, but most special forms can treat it as if it was of that type. Eg. if an explicit type was based on a tuple, `lenOf` will be able to get its number of elements.