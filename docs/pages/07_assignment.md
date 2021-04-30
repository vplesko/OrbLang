---
layout: default
title: Assignment
---
# {{ page.title }}

The real power of symbols comes from being able to assign them new values after initialization.

```
fnc main () () {
    sym (x 1) (y 2);
    = x y; # x is now 2

    sym (z 3);
    = x y z; # x and y are now 3
};
```

There are some additional operators (macros defined in **base.orb**) you may use to modify symbols.

```
import "base.orb";

fnc main () () {
    sym (x 1);
    += x 2; # x is now 3
    -= x 1; # x is now 2
    *= x 6; # x is now 12
    /= x 2; # x is now 6
    %= x 4; # x is now 2
    <<= x 1; # x is now 4
    >>= x 1; # x is now 2
    &= x 0b0011; # x is now 2
    |= x 0b0011; # x is now 3
    ^= x 0b1010; # x is now 9
    ++ x; # x is now 10
    -- x; # x is now 9
};
```