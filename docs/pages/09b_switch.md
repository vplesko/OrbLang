---
layout: default
title: Switch
---
# Switch

`switch` compares a given value with sets of other values. Depending on which set it is found in, the corresponding block of instructions will be executed.

```
    switch x
        (1 2 3) {
            std.println "bad";
        } (4 5 6) {
            std.println "mediocre";
        } (7 8 9) {
            std.println "good";
        } (10) {
            std.println "perfect";
        };
```

If the value is not found in any set, no blocks will be executed. It is a good practice to add a default block, which gets executed in those cases.

```
    switch x
        (2 3 5 7 11 13 17 19) {
            std.println "prime number";
        } (0 1 4 9 16) {
            std.println "square number";
        } (6) {
            std.println "perfect number";
        } {
            std.println "uninteresting number";
        };
```