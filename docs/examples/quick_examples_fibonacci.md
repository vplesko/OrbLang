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

    sym (fib (arr u32 0 1 1));

    repeat (- n 1) {
        = ([] fib 2) (+ ([] fib 0) ([] fib 1));
        
        = ([] fib 0) ([] fib 1);
        = ([] fib 1) ([] fib 2);
    };

    ret ([] fib 2);
};

fnc main () () {
    # this number will be calculated at compile-time
    std.println (fibonacci 20);

    std.println (fibonacci (std.scanU32));
};
```