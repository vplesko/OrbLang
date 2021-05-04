---
layout: default
title: Comments
---
# {{ page.title }}

There are two types of comments: single-line and multi-line. Single-line comments start with a `#` character.

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
    this is a multi-line comment

    this will not be printed
    std.println "inside the comment";
    $#

    std.println "not inside a comment";
};
```