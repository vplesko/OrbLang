---
layout: default
title: Ret
---
# {{ page.title }}

Used to return from a function of macro, optionally with a value.

## `ret`

## `ret val`

Returns from a function or a macro.

Instruction must not be used outside of functions and macros.

If `val` is given, it will be used as the return value. In that case, this instruction must not be used in a non-returning function.

If `val` is a symbol in the current scope, it is moved.

If the instruction is used in a returning function, `val` is implicitly cast to its returning type.