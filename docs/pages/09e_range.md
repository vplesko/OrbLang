---
layout: default
title: Range
---
# Range

`range` instructions iterate a value on a given range. They can be used with a single, or both range bounds.

```
    # 0, 1, 2, ..., 9
    range i 10 {
        std.println i " is a digit.";
    };

    # 65, 66, 67, ..., 94, 95
    range i 65 90 {
        std.println (cast c8 i) " is a letter.";
    };
```

You can also specify a delta by which the iterator will be increased after each iteration.

```
    # 0, 3, 6, 9, ..., 99
    range i 0 100 3 {
        std.println i " is divisible by 3.";
    };
```

`rangeRev` is similar, but iterates through its values in reverse.

```
    # 99, 98, ..., 1, 0
    rangeRev i 100 {
        std.println i " seconds remaining!";
    };

    # 20, 19, 18, 17, 16, 15
    rangeRev i 20 15 {
        std.println i;
    };

    # 100, 95, 90, ..., 10, 5
    rangeRev i 100 1 5 {
        std.println i;
    };
```