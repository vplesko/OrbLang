---
layout: default
title: Fibonacci
---
# {{ page.title }}

## `main.orb`

```
import "base.orb";
import "std/io.orb";

fnc fibonacci::evaluable (n:u32) u32 {
    if (< n 2) {
        ret n;
    };

    sym (a (arr u32 0 1 1));

    repeat (- n 1) {
        = ([] a 2) (+ ([] a 0) ([] a 1));
        
        = ([] a 0) ([] a 1);
        = ([] a 1) ([] a 2);
    };

    ret ([] a 2);
};

fnc main () () {
    std.println (fibonacci 20);
};
```