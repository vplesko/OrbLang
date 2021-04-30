---
layout: default
title: Exit
---
# {{ page.title }}

The `exit` special form is used to exit a block if a condition resolves to `true`, thus skipping its remaining instructions.

```
import "std/io.orb";

fnc main () () {
    block {
        std.println 1;
        exit (== (std.scanI32) 0);
        std.println 2;
        exit (== (std.scanI32) 0);
        std.println 3;
    };
};
```

Normally, it exits the innermost block found. However, by specifying a block name, you may have it exit a different block instead.

```
    block block0 () {
        block block1 () {
            exit block0 (calculateCondition);
        };
    };
```

To unconditionally exit the block, simply set `true` as the condition.