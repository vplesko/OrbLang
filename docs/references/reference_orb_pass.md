---
layout: default
title: Pass
---
# {{ page.title }}

Used to pass a value from a passing block, exiting it in the process.

## `pass val =()`

## `pass name<id> val =()`

Passes a value from the target block, also exiting that block.

If `name` is given, the target is the innermost enclosing block of that name. Otherwise, the target is the innermost enclosing block. It must be a passing block.

If `val` is a symbol in the current scope, it is moved.

The value to be passed is implicitly cast to the passing type of the target block.

```
    block i32 {
        sym x:i32;
        # ...

        pass x;
    };

    block b0 bool {
        block {
            # ...
            pass b0 true;
        };

        pass false;
    };
```