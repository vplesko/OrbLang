---
layout: default
title: Block
---
# {{ page.title }}

Used to execute a list of instructions under its own scope.

## `block body`

## `block ty body`

## `block name ty body`

Executes the list of instructions in `body` under a new scope.

`ty` is either a `type` or an empty `raw`. It must not be an undefined type. If it is a `type` the block is a passing block and is expected to pass a value of that type after it finishes execution.

`name` either an `id` or an empty `raw`. If it is an `id` the block is named with that identifier. There must not be a type with the same name as the block.

`body` is a `raw` value containing a list of instructions.

`::bare` on `block` will create a bare block instead. Bare blocks cannot be named and cannot have passing types. Bare blocks do not create their own scope. They are not considered possible targets for the purposes of special forms which target a specific block.