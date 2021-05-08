---
layout: default
title: Cast
---
# {{ page.title }}

## `cast ty<type> oper -> ty`

Returns the value of `oper` cast into `ty`.

`ty` must not be an undefined type.

If the type of `oper` is `ty`, `oper` is returned, preserving its non-value node properties.

```
    cast bool n;

    cast i32 (- x 10.0);
```