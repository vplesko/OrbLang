---
layout: default
title: Mac
---
# {{ page.title }}

Used to express macro types, or to define macros.

## `mac argCnt`

Expresses a macro type of given number of arguments.

`argCnt` must be a non-negative interger.

`::variadic` on the list of argument types expresses a variadic macro.

## `mac name<id> ([argName<id>...]) body`

Defines a macro under the given name. This macro is defined for the remainder of the program.

There must not be a function or symbol with the same in the global scope. There must not be a special form or a type with the same name, and it must not be one of `cn`, `*`, or `[]`.

There must not be conflicting macro definitions of the same name.

There must not be multiple arguments of the same name.

`body` is a `raw` value containing a list of instructions. When executing, the macro will execute the list of instructions in `body` under a new scope. Macros are expected to return a value after they finish execution.

`::global` must be placed on `mac` if this instruction is not executed in the global scope.

`::preprocess` on `argName` makes the argument not be escaped before processing on invocation.

`::plusEscape` on `argName` makes the argument be escaped an additional time before processing on invocation.

Arguments must not be marked to not be escaped and to be escaped an additional time.

`::variadic` on the last argument name makes this a variadic macro.