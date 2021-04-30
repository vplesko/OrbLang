---
layout: default
title: Block
---
# {{ page.title }}

The `block` special form groups a set of instructions to be executed one after another.

```
import "std/io.orb";

fnc main () () {
    block {
        std.println 1;
        std.println 2;
        std.println 3;
    };
};
```

It creates a new scope, nested within the original one.

Blocks can be named, and we may specify a passing type on them. The meaning of this is explained in the next sections.

```
    # a block that passes an i32
    block i32 {
        # ...
    };

    # a block called myBlock
    block myBlock () {
        # ...
    };

    # a block called myBlock that passes an i32
    block myBlock i32 {
        # ...
    };
```