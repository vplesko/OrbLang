---
layout: default
title: Cast
---
# {{ page.title }}

## `cast ty<type> oper`

Returns the value of `oper` cast into `ty`.

`ty` must not be an undefined type (eg. a data type that was only declared).

If the type of `oper` is `ty`, `oper` is returned, preserving its non-value node properties.