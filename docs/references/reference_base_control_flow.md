---
layout: default
title: Control flow
---
# {{ page.title }}

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