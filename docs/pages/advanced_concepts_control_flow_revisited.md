---
layout: default
title: Control flow - revisited
---
# {{ page.title }}

We've previously learned how to control the execution flow using macros from **base.orb**. These are based on more primitive constructs, which we will explore here.

Generally, you should rely on macros instead. Using these primitives directly to express logic can make for code that is hard to understand.

## `block`

The `block` special form groups a set of instructions to be executed one after another.

```
    block {
        std.println 1;
        std.println 2;
        std.println 3;
    };
```

It creates a new scope, nested within the original one.

Blocks can be named, and we may specify a passing type on them.

```
    # a block that passes an i32
    block i32 {
        # ...
    };

    # a block named myBlock
    block myBlock () {
        # ...
    };

    # a block named myBlock that passes an i32
    block myBlock i32 {
        # ...
    };
```

## `exit`

The `exit` special form is used to prematurely exit a block if a condition is `true`.

```
    block {
        std.println 1;
        exit (== (std.scanI32) 0);
        std.println 2;
        exit (== (std.scanI32) 0);
        std.println 3;
    };
```

Normally, it exits the innermost block found. However, by specifying a block name you may have it exit a different block instead.

```
    block block0 () {
        block block1 () {
            exit block0 (calculateCondition);
        };
    };
```

To unconditionally exit the block, simply set `true` as the condition.

## `loop`

The `loop` special form is used to jump back to the beginning of a block if a condition resolves to `true`.

```
    block {
        std.println "How much more do we have to go?";
        loop (> (std.scanI32) 0);
    };
```

Just like with `exit`, you may specify the name of a specific block to target instead of the innermost one.

## `pass`

The `pass` special form exits a block, passing a value from it, similar to how `ret` returns a value from a function.

This can only be done on blocks that have a passing type.

```
    block i32 {
        sym (x (std.scanI32));
        pass (/ (* x (+ x 1)) 2);
    };
```