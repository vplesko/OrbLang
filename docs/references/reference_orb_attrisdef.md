---
layout: default
title: AttrIsDef
---
# {{ page.title }}

Used to check whether a node has an attribute with the given name.

## `attr?? val name<id> -> bool`

Returns whether using `attrOf` on `val` with `name` would result in a value (and not raise an error).

```
    attr?? x mark;
```