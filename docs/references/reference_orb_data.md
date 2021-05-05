---
layout: default
title: Data
---
# {{ page.title }}

Used to define or declare data types.

## `data name<id> =type`

## `data name<id> (elem<id:type>...) =type`

## `data name<id> (elem<id:type>...) drop<function or ()> =type`

Creates a new data type under the given name. Returns the created type.

There must not be a function, macro, special form, type, or global symbol with the same. The name must not be one of `cn`, `*`, or `[]`.

If elements aren't given, this instruction declares a data type, instead of defining it. The same data type may be declared any number of times, even after it is defined. It must not be defined more than once.

Element types must not be constant or undefined types.

```
data Complex {
    x:f32
    y:f32
};

data BinNode {
    x:i32
    l:(BinNode *)
    r:(BinNode *)
} (lam (this:BinNode::noDrop) () {
    free (cast ptr r);
    free (cast ptr l);
});
```

If provided, `drop` must be a function that takes a single argument of this data type marked as `::noDrop`. That function will be used to drop values of this data type.

`::global` must be placed on `data` if this instruction is not executed in the global scope.

`::noZero` on an elements name allows the compiler to omit zero-initialization of that element when zero-initializing values of this data type.

Non-type attributes on the elements node will be stored as type-specific attributes of this data type.