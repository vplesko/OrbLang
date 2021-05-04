---
layout: default
title: Misc
---
# {{ page.title }}

## `genId`

Returns a generated `id`. Returns a different value on each call.

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

## `base.assertIsOfType val ty<type>`

Raises an error if `val` is not of type `ty` or `ty cn`.

`val` must be a value with a type.

## `unref val`

Returns `val` as a non-ref value.

If `val` is owning, will result in undefined behaviour.

## `cond<bool> onTrue onFalse`

Conditionally processes either the `onTrue` or `onFalse` node depending on the value of `cond`.

## `cond<bool> onTrue`

Only processes the `onTrue` node if `cond` is `true`.

## `base.enumSize ty<type>`

Returns the number of initially created symbols of the `ty`, created by `enum`.

`ty` must have been created by `enum`.