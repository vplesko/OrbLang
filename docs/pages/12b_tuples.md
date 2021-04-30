---
layout: default
title: Tuples
---
# {{ page.title }}

Tuples contain two or more elements, not necessarily of the same type. For example, `(i32 c8 bool)` is a tuple containing an `i32`, a char, and a boolean.

They are also indexed with `[]`, but unlike with arrays, indexes must be known at compile-time.

```
fnc valOrZero (x:(i32 bool)) i32 {
    if ([] x 1) {
        ret ([] x 0);
    };
    ret 0;
};
```

Tuples can be constructed using the `tup` macro from **base.orb**.

```
    sym (t (tup 100 'A'));
```