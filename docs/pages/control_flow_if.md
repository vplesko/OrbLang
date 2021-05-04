---
layout: default
title: If
---
# {{ page.title }}

`if` will execute a block of instructions if a condition is `true`.

```
    if (!= x 42) {
        std.println "Wrong answer!";
    };
```

A second block may be given to be executed if the condition is `false`.

```
    if (== (% x 2) 0) {
        std.println "x is even.";
    } {
        std.println "x is odd.";
    };
```

More arguments may be provided as pairs of conditions and blocks of instructions, with an optional final block to be executed if no condition is met.

```
    if (< x 0) {
        std.println "x is negative.";
    } (== x 0) {
        std.println "x is zero.";
    } {
        std.println "x is positive.";
    };
```