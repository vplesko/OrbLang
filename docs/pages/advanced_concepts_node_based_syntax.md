---
layout: default
title: Node-based syntax
---
# {{ page.title }}

Before continuing to next lessons, we must first explain something regarding Orb's syntax.

Let's look at a simple Orb program.

```
import "std/io.orb";

fnc foo (x:u32) u32 {
    ret (+ (* 31 x) 17);
};

fnc main () () {
    std.println (foo (std.scanU32));
};
```

In Orb, there is no difference between the two types of parenthesis - there never was. Furthermore, semicolons simply group up what was placed between them and the closest previous unclosed parenthesis.

So, the program from above could equivalently be rewritten as:

```
(import "std/io.orb")

(fnc foo (x:u32) u32 (
    (ret (+ (* 31 x) 17))
))

(fnc main () () (
    (std.println (foo (std.scanU32)))
))
```

Back when we talked about primitive types, we mentioned `raw` but never demonstrated it. As it turns out, we've been using this type the whole time - each parenthesised group we've written was actually a `raw` value.

Like with many other languages, Orb code can be represented as an Abstract Syntax Tree (AST). Unlike most languages, the code is a direct representation of that AST.

Orb ASTs consist of nodes. There are two types of nodes. Non-leaf nodes are parenthesised groups, which are also `raw` values. Leaf nodes are literals (remember that identifiers are also literals).

When a non-leaf is processed (either compiled or evaluated), the value of its starting element is processed first. How the compiler processes the node as a whole depends on what the starting element resolved to. For example, if it resolved to a function, the node represents a call to that function.

This explains why Orb uses prefix notation only and why it requires placing so many semicolons.