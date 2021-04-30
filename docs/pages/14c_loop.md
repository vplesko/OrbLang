---
layout: default
title: Loop
---
# {{ page.title }}

The `loop` special form is used to jump back to the beginning of a block if a condition resolves to `true`.

```
import "std/io.orb";

fnc main () () {
    block {
        std.println "How much more do we have to go?";
        loop (> (std.scanI32) 0);
    };
};
```

Just like `exit`, it normally targets the innermost block, but you may specify the name of a specific block to target instead.

```
    block block0 () {
        block block1 () {
            loop block0 (calculateCondition);
        };
    };
```

To make it work unconditionally, simply set `true` as the condition.