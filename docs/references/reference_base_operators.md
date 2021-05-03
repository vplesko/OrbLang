---
layout: default
title: Operators
---
# {{ page.title }}

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

Iteratively going through the arguments, if any resolves to `false`, finishes the instruction and returns `false`. Otherwise, returns `true`.

## `|| val0<bool> val1<bool> val<bool>...`

Calculates the logical disjunction on arguments, with short-circuiting.

Iteratively going through the arguments, if any resolves to `true`, finishes the instruction and returns `true`. Otherwise, returns `false`.