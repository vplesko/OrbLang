---
layout: default
title: 'Reference: base'
---
# {{ page.title }}

This section documents definitions found in **base.orb**.

## `genId`

Returns a generated `id`. Returns a different value on each call.

## `alias name<id> ty<type>`

Makes `name` refer to `ty`.

```
    alias myArray (i32 4);
```

## `String`

Alias for the type of string literals.

## `passthrough node`

Returns `node` (the node is processed).

> This has a very fringe use-case which may appear when writing a macro that returns a special form which escapes one of its `id` arguments.

## `process node`

Additonally processes the result of the `node` node.

## `escape node`

Escapes `node`.

> This is useful when a macro needs to return a value and tell its invoker to escape it.

## `base.isRaw val`

Returns whether `val` is a `raw`.

`val` must be a value with a type.

## `base.makeRawWith val...`

Creates a `raw` with arguments as its elements.

> This is different from building a `raw` by escaping a node, since here the arguments will not be escaped.

## `base.assertIsOfType val ty<type>`

Raises an error if `val` is not of type `ty` or `ty cn`.

`val` must be a value with a type.

## `cond<bool> onTrue onFalse`

Conditionally processes either the `onTrue` or `onFalse` node depending on the value of `cond`.

## `cond<bool> onTrue`

Only processes the `onTrue` node if `cond` is `true`.

## `if cond<bool> then<block> rest...`

Conditionally executes one of the provided blocks of instructions based on the conditions provided.

Conditions and bodies are consecutive pairs of arguments. Optional, the last argument may be unpaired, in which case it represents the fallback block to execute if no condition is satisfied.

Will not process all condition nodes if a previous condition has been satisfied (short-circuiting).

```
    if cond0 {
        doThing0;
    } cond1 {
        doThing1;
    } {
        fallback;
    };
```

## `while cond<bool> body<block>`

Repeatedly executes instructions in `body` as long as `cond` resolves to `true`.

```
    while cond {
        doThing;
    };
```

## `for init cond<bool> step body<block>`

First, executes `init`. Then, repeatedly checks if `cond` is `true` and, if it is, executes instructions in `body`, then executes `step`; if `cond` is `false`, the instruction is finished.

```
    for (sym (i 0)) (< i n) (++ i) {
        doThing i;
    };
```

## `break`

Prematurely ends a `for`, `while`, `range`, `rangeRev`, or `repeat` loop.

## `continue`

Prematurely ends the current iteration of a `for`, `while`, `range`, `rangeRev`, or `repeat` loop.

## `range i<id> up<numeric> body<block>`

Declares a symbol with name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from 0 to one less than `up`.

If `up` is not positive, will result in undefined behaviour.

```
    # 0, 1, 2, ..., 9
    range i 10 {
        doThing i;
    };
```

## `range i<id> lo<numeric> hi<numeric> body<block>`

Declares a symbol with name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from `lo` to `hi`.

If `lo` is greater than `hi`, will result in undefined behaviour.

```
    # 5, 6, 7, 8, 9, 10
    range i 5 10 {
        doThing i;
    };
```

## `range i<id> lo<numeric> hi<numeric> delta<numeric> body<block>`

Declares a symbol with name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from `lo` to `hi`, but increasing by `delta` after each iteration.

If `lo` is greater than `hi`, will result in undefined behaviour.

```
    # 5, 8
    range i 5 10 3 {
        doThing i;
    };
```

## `rangeRev i<id> up<numeric> body<block>`

Declares a symbol with name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from one less than `up` down to 0.

If `up` is not positive, will result in undefined behaviour.

```
    # 9, 8, 7, ..., 0
    rangeRev i 10 {
        doThing i;
    };
```

## `range i<id> hi<numeric> lo<numeric> body<block>`

Declares a symbol with name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from `hi` down to `lo`.

If `lo` is greater than `hi`, will result in undefined behaviour.

```
    # 10, 9, 8, 7, 6, 5
    rangeRev i 10 5 {
        doThing i;
    };
```

## `range i<id> hi<numeric> lo<numeric> delta<numeric> body<block>`

Declares a symbol with name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from `hi` down to `lo`, but decreasing by `delta` after each iteration.

If `lo` is greater than `hi`, will result in undefined behaviour.

```
    # 10, 7
    rangeRev i 10 5 3 {
        doThing i;
    };
```

## `repeat n<numeric> body<block>`

Executes instructions in `body` a number of times given by `n`.

If `n` is negative, will result in undefined behaviour.

```
    repeat 5 {
        doThing;
    };
```

## `switch val targets body [rest...]`

Conditionally executes one of the provided blocks of instructions based on which target `val` is equal to.

Targets and bodies are consecutive pairs of arguments. Optional, the last argument may be unpaired, in which case it represents the fallback block to execute if no target equals `val`.

Targets are lists of values.

Will not process target nodes if a target equal to `val` has already been found (short-circuiting).

```
    switch (x)
        (0 1) {
            doThing0;
        } (2 3 4) {
            doThing1;
        } {
            fallback;
        };
```

## `++ val<numeric>`

Increases the value of `val` by one.

`val` must be a ref value.

## `-- val<numeric>`

Decreases the value of `val` by one.

`val` must be a ref value.

## `+= val by`

## `-= val by`

## `*= val by`

## `/= val by`

## `%= val by`

## `>>= val by`

## `<<= val by`

## `&= val by`

## `|= val by`

## `^= val by`

Assigns a new value to `val` based on the calculation with `val` and `by` as operands.

`val` must be a ref value.

## `-> val m...`

Dereferences `val` and performs `[]` on the result with `m` as the index. Then, repeats this for the previous result and the next argument, until all arguments have been used.

## `&& val0<bool> val1<bool> val<bool>...`

Calculates the logical conjunction on arguments, with short-circuiting.

Iteratively going through the arguments, if any resolves to `false`, finishes the instruction and returns `false`. Otehrwise, returns `true`.

## `|| val0<bool> val1<bool> val<bool>...`

Calculates the logical disjunction on arguments, with short-circuiting.

Iteratively going through the arguments, if any resolves to `true`, finishes the instruction and returns `true`. Otehrwise, returns `false`.

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

## `base.enumSize ty<type>`

Returns the number of initially created symbols of the `ty`, created by `enum`.

`ty` must have been created by `enum`.