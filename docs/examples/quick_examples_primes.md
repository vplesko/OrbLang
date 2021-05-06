---
layout: default
title: Primes
---
# {{ page.title }}

In this example, we print all primes between 1 and 100. The limit can easily be changed by modifying the symbol `n`.

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