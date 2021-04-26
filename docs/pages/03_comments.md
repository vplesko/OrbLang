---
layout: default
title: Comments
---
# Comments

It is possible to comment out parts of Orb code and the compiler will just skip over them. There are two types of comments: single-line and multi-line.

Single-line comments start with a `#` character.

```
import "std/io.orb";

fnc main () () {
    # this will not be printed
    # std.println "commented out";

    std.println "not commented out";
};
```

Multi-line comments are enclosed within `#$` and `$#`.

```
import "std/io.orb";

fnc main () () {
    #$
    This is a multi-line comment.
    This entire section will not be processed by the compiler.

    std.println "inside the comment";
    $#

    std.println "not inside a comment";
};
```