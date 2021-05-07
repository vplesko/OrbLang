---
layout: default
title: Tuples
---
# {{ page.title }}

Tuples contain two or more elements, not necessarily of the same type. For example, `(i32 c8 bool)` is a tuple containing an `i32`, a character, and a boolean.

They are also indexed with `[]`, but unlike with arrays, indexes must be known at compile-time.

```
fnc valOrZero (x:(bool i32)) i32 {
    if ([] x 0) {
        ret ([] x 1);
    };

    ret 0;
};
```

Tuples can be constructed using the `tup` macro from **base.orb**.

```
    sym (t (tup 100 'A' true));
```