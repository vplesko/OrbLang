---
layout: default
title: 'Reference: Base'
---
# {{ page.title }}

This section documents definitions found in **base.orb**. To use these definitions, you need to import this file.

Due to how importing works, you may place `import "base.orb";` at the start of your main Orb source file and not need to import it from any other file.

Declarations use the same syntax used in the reference for Orb special forms.

## `if cond<bool> then<block> [rest...]`

Conditionally executes one of the provided blocks of instructions based on the conditions provided.

Conditions and bodies are consecutive pairs of arguments. Optionally, the last argument may be unpaired, in which case it represents the fallback block to execute if no condition is satisfied.

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

## `switch val targets body<block> [rest...]`

Conditionally executes one of the provided blocks of instructions based on which target `val` is equal to.

Targets and bodies are consecutive pairs of arguments. Optionally, the last argument may be unpaired, in which case it represents the fallback block to execute if no target equals `val`.

Target nodes are `raw` values containing values to compare against.

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

---

## `for init cond<bool> step body<block>`

First, executes `init`. Then, repeatedly checks if `cond` is `true` and, if it is, executes instructions in `body`, then executes `step`; if `cond` is `false`, the instruction is finished.

```
    for (sym (i 0)) (< i n) (++ i) {
        doThing i;
    };
```

## `while cond<bool> body<block>`

Repeatedly executes instructions in `body` as long as `cond` is `true`.

```
    while cond {
        doThing;
    };
```

## `repeat body<block>`

Repeatedly executes instructions in `body`.

```
    repeat {
        doThing;

        if cond {
            break;
        };
    };
```

## `repeat n<integer> body<block>`

Executes instructions in `body` the number of times given by `n`.

If `n` is negative, will result in undefined behaviour.

```
    repeat 5 {
        doThing;
    };
```

## `range i<id> up<integer> body<block>`

Declares a symbol with the name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from zero to one less than `up`.

If `up` is not positive, will result in undefined behaviour.

```
    # 0, 1, 2, ..., 9
    range i 10 {
        doThing i;
    };
```

## `range i<id> lo<integer> hi<integer> body<block>`

Declares a symbol with the name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from `lo` to `hi`.

If `lo` is greater than `hi`, will result in undefined behaviour.

```
    # 5, 6, 7, 8, 9, 10
    range i 5 10 {
        doThing i;
    };
```

## `range i<id> lo<integer> hi<integer> delta<integer> body<block>`

Declares a symbol with the name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from `lo` to `hi`, but increasing by `delta` after each iteration.

If `lo` is greater than `hi`, will result in undefined behaviour.

```
    # 5, 8
    range i 5 10 3 {
        doThing i;
    };
```

## `rangeRev i<id> up<integer> body<block>`

Declares a symbol with the name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from one less than `up` down to zero.

If `up` is not positive, will result in undefined behaviour.

```
    # 9, 8, 7, ..., 0
    rangeRev i 10 {
        doThing i;
    };
```

## `rangeRev i<id> hi<integer> lo<integer> body<block>`

Declares a symbol with the name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from `hi` down to `lo`.

If `lo` is greater than `hi`, will result in undefined behaviour.

```
    # 10, 9, 8, 7, 6, 5
    rangeRev i 10 5 {
        doThing i;
    };
```

## `rangeRev i<id> hi<integer> lo<integer> delta<integer> body<block>`

Declares a symbol with the name `i`. Repeatedly executes instructions in `body` while iterating values of `i` from `hi` down to `lo`, but decreasing by `delta` after each iteration.

If `lo` is greater than `hi`, will result in undefined behaviour.

```
    # 10, 7
    rangeRev i 10 5 3 {
        doThing i;
    };
```

## `break`

Prematurely ends a `for`, `while`, `repeat`, `range`, or `rangeRev` loop.

## `continue`

Prematurely ends the current iteration of a `for`, `while`, `repeat`, `range`, or `rangeRev` loop.

---

## `++ val<integer>`

Increases the value of `val` by one and returns it.

```
    ++ i;
```

## `-- val<integer>`

Decreases the value of `val` by one and returns it.

```
    -- i;
```

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

Assigns a new value to `val` based on the calculation with `val` and `by` as operands and returns `val`.

```
    += x k;

    <<= n 1;
```

## `&& val0<bool> val<bool>... -> bool`

Calculates the logical conjunction on arguments (AND operator), with short-circuiting.

Iteratively going through the arguments, if any are `false`, finishes the instruction and returns `false`. Otherwise, returns `true`.

```
    if (&& b0 b1 b2) {
        doThing;
    };
```

## `|| val0<bool> val<bool>... -> bool`

Calculates the logical disjunction on arguments (OR operator), with short-circuiting.

Iteratively going through the arguments, if any are `true`, finishes the instruction and returns `true`. Otherwise, returns `false`.

```
    if (|| b0 b1 b2) {
        doThing;
    };
```

## `-> val m...`

Dereferences `val` and performs `[]` on the result with `m` as the index. Then, repeats this for the previous result and the next argument, until all arguments have been used.

```
    sym x:(Person *);
    # = x ...;

    std.println (-> x name);
```

---

## `alias name<id> ty<type>`

Makes `name` refer to `ty`.

```
    alias myArray (i32 4);
```

## `String`

Alias for the type of string literals.

## `arr ty<type> val...`

Constructs an array of `ty`. The length of the array is equal to the number of arguments in `val...`.

Each value in `val...` is implicitly cast to `ty` and copied to the array.

```
    sym (array (arr i32 10 11 12 13 14 15));
```

## `tup val...`

Constructs a tuple with the given values as elements.

```
    sym (tuple (tup 100.0 true));
```

## `make ty<type> pair...`

Constructs a value of data type `ty` and assigns its elements depending on the provided pairs.

`ty` must be a data type. Each pair must be a `raw` of two elements - an element name and value.

For each pair, the element in the data type under that name will be assigned with the given value.

```
    make Person (name 'Alice') (age 32);
```

## `lam body -> function`

## `lam retTy body -> function`

## `lam args retTy body -> function`

Defines and returns a function.

If `args` is given, it will be used to define the arguments of the function. Otherwise, the function will take no arguments.

If `retTy` is given, it will be used to define the return type of the function. Otherwise, the function will not be a returning function.

`body` will be used as the body of the function.

```
    lam { std.println; };

    lam i32 { ret (* 2 (std.scanI32)); };

    lam (x:i32) i32 { ret (* x x); };
```

## `pat body -> macro`

## `pat args body -> macro`

Defines and returns a macro.

If `args` is given, it will be used to define the arguments of the macro. Otherwise, the macro will take no arguments.

`body` will be used as the body of the macro.

```
    pat { ret \(block {}); };

    pat (a) { ret \(sym ,a:i32); };
```

## `enum name<id> vals`

## `enum name<id> ty<type> vals`

Defines an explicit type (an enum type) and creates values of that type.

If `ty` is provided it will be used as the base type. Otherwise, the base type will be `i32`.

`vals` is a list of symbols of this type to define. Each entry must be either an `id` or a pair of `id` and a value. The created symbol will use that `id` for its name. If the value is provided, it will be cast to `ty` and used to initialize that symbol.

If the value is not provided, and this is the first entry, its value will be zero. Default values of entries past the first are one more than the value of the previous entry.

Each symbol will be declared constant.

```
enum CoinSide {
    Head
    Tails
};

fnc flipCoin () CoinSide {
    if (< (myRandGen) 0.5) {
        ret CoinSide.Head;
    };
    ret CoinSide.Tails;
};

enum HttpStatus u32 {
    (Ok 200)
    Created
    Accepted
    (BadRequest 400)
    Unauthorized
    PaymentRequired
    Forbidden
    NotFound
    (InternalError 500)
};
```

## `base.getEnumSize ty<type> -> unsigned`

Returns the number of enum entries (initially created symbols of the `ty`, created by `enum`).

```
    base.getEnumSize CoinSide; # 2
```

---

## `genId -> id`

Generates and returns an `id`. Returns a different value on each call.

```
    eval (sym (s:id (genId)));
```

## `cond<bool> onTrue onFalse`

Conditionally processes either the `onTrue` or `onFalse` node depending on the value of `cond`.

`cond` must be an evaluated value.

```
    cond verbose
        (std.println "index=" ind " value=" ([] array ind))
        (std.println ([] array ind));
```

## `cond<bool> onTrue`

Only processes the `onTrue` node if `cond` is `true`.

`cond` must be an evaluated value.

```
    cond enableDebugAsserts
        (if (< ind 0) { exit 1; });
```

## `passthrough node`

Returns `node` (ie. the node is processed).

> This has a very fringe use-case which may appear when writing a macro that returns a special form that escapes one of its `id` arguments.

## `process node`

Does an extra processesing of the result of the `node` node.

## `escape node`

Escapes `node`.

> This is useful when a macro needs to return a value and tell its invoker to escape it.

## `unref val`

Returns `val` as a non-ref value.

## `base.isOfType val ty<type> -> bool`

Returns whether `val` is of type `ty` or `ty cn`.

`val` must be a typed value.

## `base.isRaw val -> bool`

Returns whether `val` is of type `raw` or `raw cn`.

`val` must be a typed value.

## `base.isEmptyRaw val -> bool`

Returns whether `val` is an empty `raw` value (ie. `()`).

`val` must be a typed value.

## `base.assertIsOfType val ty<type>`

Raises an error if `val` is not of type `ty` or `ty cn`.

## `base.makeRawWith val... -> raw`

Creates a `raw` with arguments as its elements.

> This is different from building a `raw` by escaping a node, since here the arguments will not be escaped.