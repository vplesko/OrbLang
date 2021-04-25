---
layout: default
title: Comments
---
# Comments

It is possible to comment out parts of Orb code and the compiler will just skip over them. There are two types of comments: single-line and multi-line.

Single-line comments start with a `#` character:

```
import "std/io.orb";

fnc main () () {
    # this is not processed by the compiler
    # std.println a-faulty-value;

    std.println "a good string";
};
```

Multi-line comments are enclosed within `#$` and `$#` sequences:

```
import "std/io.orb";

fnc main () () {
    #$
    This is a multi-line comment.
    This entire section will not be processed by the compiler.

    std.println something or other;
    std.println still inside the comment;
    $#

    std.println "not in a comment";
};
```