---
layout: default
title: Loop
---
# {{ page.title }}

Used to conditionally restart a block execution from its first instruction.

## `loop cond<bool>`

## `loop name<id> cond<bool>`

Restarts the execution of the target block if `cond` is `true`. The new execution starts from the first instruction in the block. The current scope of the block is closed, and a new scope is opened.

If `name` is given, the target is the innermost enclosing block of that name. Otherwise, the target is the innermost enclosing block.

> Looping is allowed on passing blocks.