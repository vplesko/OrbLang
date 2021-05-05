---
layout: default
title: Operators - unary
---
# {{ page.title }}

Used to perform calculations and/or modify an operand.

## `+ oper<numeric>`

Returns a value equal to the value of `oper`.

## `- oper<integer or float>`

Returns the negated value of `oper`.

## `~ oper<integer>`

Returns the value of `oper` with all its bits complemented.

`oper` must of an integer type.

## `! oper<bool>`

Returns the negated value of `oper`.

## `* oper`

Returns the value to which `oper` points. This is known as dereferencing a pointer.

`oper` must be of a pointer type, excluding array pointers and `ptr`.

The pointed-to type must not be an undefined type.

If `oper` is an evaluated value, it must not be `null`. If `oper` is a compiled value equal to `null`, the compiled program will have undefined behaviour.

The result is a ref value.

## `& oper`

Returns a pointer to `oper`.

`oper` must be a ref value.

## `>> oper`

Resets `oper` to its zero state and returns its previous value as a non-ref value. This is known as moving a value.

`oper` must be a typed value. It must not be marked as `noDrop`. It must not be, nor be fetched from, an invocation argument.

If `oper` is a non-ref value, it is simply returned.

If `oper` is a ref value, it must not be of a constant type.

`::noZero` on `>>` will cause `oper` to not be reset to its zero state. It may be of a constant type.