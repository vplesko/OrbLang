---
layout: default
title: Primes
---
# {{ page.title }}

## `main.orb`

```
import "std/io.orb";

fnc main () () {
    std.print "Primes:";

    sym arr:(bool 101);
    range i 2 100 {
        if ([] arr i) {
            continue;
        };

        std.print ' ' i;

        range j (* i i) 100 i {
            = ([] arr j) true;
        };
    };

    std.println;
};
```