---
layout: default
title: For
---
# {{ page.title }}

For takes four arguments:
 - initial instruction,
 - condition,
 - step instruction,
 - block of instructions.

It starts by executing the initial instruction. Then, it repeatedly executes the block of instructions and the step instruction as long as the condition is met.

```
    for (sym (i 0)) (< i 10) (++ i) {
        std.println i " is a digit.";
    };
```