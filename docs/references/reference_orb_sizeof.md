---
layout: default
title: SizeOf
---
# {{ page.title }}

Used to get the compiled memory size of a type or a value.

## `sizeOf oper =unsigned`

Gets the memory size of a type or a value in bytes, after it would have been compiled.

If `oper` is not a `type`, operates on its type instead.

The given type must not be undefined and must be compilable.

The result depends on the machine architecture for which the program is compiled.