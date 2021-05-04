---
layout: default
title: Repeat
---
# {{ page.title }}

`repeat` executes a given block of instructions a given number of times.

```
    repeat 3 {
        std.println "Hello!";
    };
```

Not specifying a number causes the block to be repeatedly executed indefinitely.