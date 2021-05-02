---
layout: default
title: SizeOf
---
# {{ page.title }}

Used to get the compiled memory size of a type or a value.

## `sizeOf oper`

Gets the memory size of a type or a value in bytes, after it would have been compiled.

If `oper` is not a `type`, operates on its type.

The given type must not be undefined.

The given type must be compilable.

The result depends on the machine architecture for which the program is compiled.

Returned size will be of an unsigned integer type.