---
layout: default
title: Fibonacci
---
# {{ page.title }}

In this example, we calculate a number in the Fibonacci sequence. This is done in a function that can also be executed at compile-time. The algorithm is iterative and executes in linear time with constant memory usage.

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