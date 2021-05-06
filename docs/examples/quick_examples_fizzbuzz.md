---
layout: default
title: FizzBuzz
---
# {{ page.title }}

## `main.orb`

```
import "base.orb";
import "std/io.orb";

fnc main () () {
    range i 1 100 {
        if (== (% i 15) 0) {
            std.println "FizzBuzz";
        } (== (% i 3) 0) {
            std.println "Fizz";
        } (== (% i 5) 0) {
            std.println "Buzz";
        } {
            std.println i;
        };
    };
};
```