---
layout: default
title: Mac
---
# {{ page.title }}

Used to express macro types, or to define macros.

## `mac argCnt<integer> =type`

Expresses a macro type of given number of arguments.

`argCnt` must not be negative.

`::variadic` on the list of argument types expresses a variadic macro.

## `mac name<id> ([argName<id>...]) body<block> =macro`

Defines a macro under the given name.

There must not be a function, special form, type, or global symbol with the same. The name must not be one of `cn`, `*`, or `[]`.

It is allowed to overload macros of the same name. There must not be conflicting function definitions among them.

There must not be multiple arguments of the same name.

When executing, the macro will execute the list of instructions in `body` under a new scope. This is always done through evaluation. Macros are expected to return a value after they finish execution.

`::global` must be placed on `mac` if this instruction is not executed in the global scope.

`::preprocess` on `argName` makes the argument not be escaped before processing on invocation.

`::plusEscape` on `argName` makes the argument be escaped an additional time before processing on invocation.

Arguments must not be marked both to not be escaped and to be escaped an additional time.

`::variadic` on the last argument name makes this a variadic macro. On invocation, all surplus arguments will be grouped as elements of a `raw` value that will be passed as the value of the variadic argument.