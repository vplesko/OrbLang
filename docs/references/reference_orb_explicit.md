---
layout: default
title: Explicit
---
# {{ page.title }}

Used to define explicit types.

## `explicit name<id> ty<type>`

Creates a new explicit type under the given name. This type is defined for the remainder of the program.

There must not be a function, macro, or symbol with the same in the global scope. There must not be a special form or a type with the same name, and it must not be one of `cn`, `*`, or `[]`.

`::global` must be placed on `explicit` if this instruction is not executed in the global scope.

> An explicit type is not implicitly castable to the type it is based on, but most special forms can treat it as if it was of that type. Eg. if an explicit type was based on a tuple, `lenOf` will be able to get its number of elements.