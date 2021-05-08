---
layout: default
title: Primes
---
# {{ page.title }}

In this example, we print all primes between 1 and 100. The limit can easily be changed by modifying the symbol `n`.

The algorithm used is the sieve of Eratosthenes, with the optimization of starting from prime's square.

## `main.orb`

```
import "std/io.orb";

eval (sym (n 100));

fnc main () () {
    std.print "Primes:";

    # all initialized as false
    sym isComposite:(bool (+ n 1));

    range i 2 n {
        if ([] isComposite i) {
            continue;
        };

        std.print ' ' i;

        range j (* i i) n i {
            = ([] isComposite j) true;
        };
    };

    std.println;
};
```