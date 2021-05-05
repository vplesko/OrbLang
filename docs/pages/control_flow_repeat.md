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

If the number is not given, the block will keep executing.

```
    repeat {
        handleInput;
        update;
        render;
    };
```