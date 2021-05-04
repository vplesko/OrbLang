---
layout: default
title: Data
---
# {{ page.title }}

Used to define or declare data types.

## `data name<id>`

## `data name<id> (elem...)`

## `data name<id> (elem...) drop`

`elem` is `(name<id>:ty<type>)`.

Creates a new data type under the given name. This type is defined for the remainder of the program.

There must not be a function, macro, or symbol with the same in the global scope. There must not be a special form or a type with the same name, and it must not be one of `cn`, `*`, or `[]`.

If elements aren't specified, this instruction declares a data type, instead of defining it. The same data type may be declared any number of times, even after it is defined. It must not be defined more than once.

`ty` must not be a constant type. It must not be an an undefined type.

`drop` is either a function which takes a single argument of this data type marked as `::noDrop` or an empty `raw`. If it's a function, that function will be used to drop values of this data type.

`::global` must be placed on `data` if this instruction is not executed in the global scope.

`::noZero` on `name` allows the compiler to omit zero-initialization of that element when zero-initializing values of this data type.

Non-type attributes on `(elems...)` will be stored as type attributes of this data type.