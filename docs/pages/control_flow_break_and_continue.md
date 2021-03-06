---
layout: default
title: Break and continue
---
# {{ page.title }}

`break` can be used to prematurely end a `while`, `for`, `range`, `rangeRev`, or `repeat` instruction.

```
    while true {
        sym (x (std.scanI32));
        if (== x 0) {
            break;
        };

        std.println x;
    };
```

`continue` can be used to skip over the remaining instructions in the current iteration.

```
    std.println "Composite numbers:";
    range i 0 100 {
        if (isPrime i) {
            continue;
        };

        std.print '\t' i;

        if (isSquare i) {
            std.print " (square)";
        } (isPerfect i) {
            std.print " (perfect)";
        };

        std.println;
    };
```