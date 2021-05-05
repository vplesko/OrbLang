---
layout: default
title: Sym
---
# {{ page.title }}

Used to register new symbols in the current scope.

## `sym entry...`

`entry` is one of:

 1. `name<id>:ty<type>`
 2. `(name<id>:ty<type>)`
 3. `(name<id> init)`
 4. `(name<id>:ty<type> init)`

Registers symbols under the given name in the current scope.

In 1., 2., and 4., the type of symbol will be `ty`. In 3., it will be the type of `init`.

In 1. and 2., the symbol will be zero-initialized and `ty` must not be a constant type. In 4., `init` will be implicitly cast into `ty`.

There must not be a function, macro, special form, type, or symbol in the same scope with the same. The name must not be one of `cn`, `*`, or `[]`.

`ty` must not be an undefined type.

`::evaluated` on `name` will result in an evaluted symbol being registered.

`::noZero` on `name` in 1. and 2. allows the compiler to omit zero-initialization of the symbol.