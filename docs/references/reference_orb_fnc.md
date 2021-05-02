---
layout: default
title: Fnc
---
# {{ page.title }}

Used to express function types, or to define or declare functions.

## `fnc ([argTy<type>...]) retTy`

Expresses a function type of given argument types and return type.

`retTy` is either a `type` or an empty `raw`. If it is a `type`, that will be the return type. If it is an empty `raw`, the function type is not a returning function.

`::variadic` on the list of argument types expresses a variadic function.

`::noDrop` on `argTy` marks the argument as not being owned by the function.

## `fnc name<id> ([arg...]) retTy`

## `fnc name<id> ([arg...]) retTy body`

`arg` is `(name<id>:ty<type>)`.

Declares of defines a function under the given name. This function is declared or defined for the remainder of the program.

If body isn't provided, this instruction declares a function, instead of defining it.

There must not be a macro or symbol with the same in the global scope. There must not be a special form or a type with the same name, and it must not be one of `cn`, `*`, or `[]`.

There must not be multiple arguments of the same name.

`ty` must not be an undefined type if this is a function definition.

`retTy` is either a `type` or an empty `raw`. If it is a `type`, that will be the return type. If it is a `type`, this is a returning function and is expected to return a value of that type after it finishes execution.

`retTy` must not be an undefined type if this is a function definition.

`body` is a `raw` value containing a list of instructions. When executing, the function will execute the list of instructions in `body` under a new scope.

There must not be conflicting function definitions of the same name. All functions declarations and the definition (where there is one) of the same signature must be conforming.

`::global` must be placed on `fnc` if this instruction is not executed in the global scope.

`::noNameMangle` on `name` forces the compiler to not mangle the name of the compiled function. All functions of this name must have the same signature (there cannot be more than one definition in that case). Functions named `main` and variadic functions always have name mangling disabled.

`::evaluable` on `name` makes this an evaluated function. By default, a function is evaluated if it is defined by evaluation.

`::compilable` on `name` makes this a compiled function. By default, a function is evaluated if it is defined by compilation.

Functions must not be marked as neither evaluable or compilable.

`::variadic` on the list of arguments makes this a variadic function.

`::noDrop` on `name` marks the argument as not being owned by the function.