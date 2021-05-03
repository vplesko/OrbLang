---
layout: default
title: Constructors
---
# {{ page.title }}

## `alias name<id> ty<type>`

Makes `name` refer to `ty`.

```
    alias myArray (i32 4);
```

## `base.makeRawWith val...`

Creates a `raw` with arguments as its elements.

> This is different from building a `raw` by escaping a node, since here the arguments will not be escaped.

## `arr ty<type> val...`

Constructs an array of `ty`. Length of the array is equal to the number of arguments in `val...`.

Each value in `val...` implicitly cast to `ty` and added as an element in the array.

## `tup val...`

Constructs an tuple with the given values as elements.

## `make ty<type> pair...`

Constructs a value of data type `ty` and sets its elements depending on the provided pairs.

`ty` must be a data type. Each pair must be a `raw` of two elements - an element name and value.

For each pair, the element in the data type under that name will be assigned with the given value.

## `lam body<block>`

## `lam retTy body<block>`

## `lam args retTy body<block>`

Defines and returns a function.

If `args` is given, it will be used to define the arguments of the function. Otherwise, the function will take no arguments.

If `retTy` is given, it will be used to define the return type of the function. Otherwise, the function will not be a returning function.

`body` will be used as the body of the function.

## `pat body<block>`

## `pat args body<block>`

Defines and returns a macro.

If `args` is given, it will be used to define the arguments of the macro. Otherwise, the macro will take no arguments.

`body` will be used as the body of the macro.

## `enum name<id> vals`

## `enum name<id> ty<type> vals`

Defines an explicit type and creates values of that type.

If `ty` is provided it will be used as the base type of the created type. Otherwise, the base type will be `i32`.

`vals` is a list of symbols of this type to define. Each entry must be either an `id` or a pair of `id` and a value. The created symbol will use that `id` for its name. If the value is provided, it will be cast to `ty` and used to initialize that symbol.

If the value is not provided, and this is the first entry, its value will be zero. Otherwise, its value will be one more than the value of the previous entry.

Each symbol will be of constant type.