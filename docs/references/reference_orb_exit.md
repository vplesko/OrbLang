---
layout: default
title: Exit
---
# {{ page.title }}

Used to conditionally exit a block prematurely.

## `exit cond<bool> =()`

## `exit name<id> cond<bool> =()`

Exits the target block if `cond` is `true`.

If `name` is given, the target is the innermost enclosing block of that name. Otherwise, the target is the innermost enclosing block.

The target block must not be a passing block.