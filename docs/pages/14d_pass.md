---
layout: default
title: Pass
---
# {{ page.title }}

The `pass` special form exits a block, passing a value from it. This can only be done on blocks which have a passing type.

```
import "std/io.orb";

fnc main () () {
    std.println (block i32 {
        sym (x (std.scanI32));
        pass (/ (* x (+ x 1)) 2);
    });
};
```