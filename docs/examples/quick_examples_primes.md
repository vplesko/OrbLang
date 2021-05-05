---
layout: default
title: Primes
---
# {{ page.title }}

## `main.orb`

```
import "std/io.orb";

eval (sym (n 100));

fnc main () () {
    std.print "Primes:";

    sym arr:(bool (+ n 1));
    range i 2 n {
        if ([] arr i) {
            continue;
        };

        std.print ' ' i;

        range j (* i i) n i {
            = ([] arr j) true;
        };
    };

    std.println;
};
```