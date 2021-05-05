---
layout: default
title: IsDef
---
# {{ page.title }}

Used to check whether there is a definition under a given name.

## `?? name<id> =bool`

Returns whether a lookup on the given name would result in a typed value.

If there is a symbol, function, macro, or type called `name`, returns `true`, otherwise `false`.