---
layout: default
title: Block
---
# {{ page.title }}

Used to execute a list of instructions under a separate scope.

## `block body<block>`

## `block ty<type or ()> body<block>`

## `block name<id or ()> ty<type or ()> body<block>`

Executes the list of instructions in `body` under a new scope.

If a type `ty` is provided, the block is a passing block and is expected to pass a value of that type after it finishes execution. It must not be an undefined type. The passing value is the return value of the special form.

If an identifier `name` is provided, it will be used as the name of the block. There must not exist a type with that name.

```
    block blockFoo i32 {
        sym (x (std.scanI32));
        = x (* x (+ x 1));
        pass x;
    };
```

`::bare` on `block` will create a bare block instead. Bare blocks cannot be named and cannot be passing blocks. Bare blocks do not create their own scope. They are not considered possible targets for the purposes of special forms which target a specific block.