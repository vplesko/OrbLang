---
layout: default
title: Operators
---
# {{ page.title }}

Operators perform calculations on values. As you will see, they are written in prefix notation - operators before the arguments. In fact, the entire language uses prefix notation. This fundamentally relates to how Orb is defined, but that will be explored in *Advanced concepts: Node-based syntax*.

For now, try to get accustomed to this syntax. It has a benefit, however - a lot of the operators are variadic. This means they can handle more than two arguments, which can at times allow for shorter code.

Let's look at the arithmetic operators first.

```
import "std/io.orb";

fnc main () () {
    std.println (+ 1 2); # 3
    std.println (- 8 6); # 2
    std.println (* 15 5); # 75
    std.println (/ 12 3); # 4

    # remainder
    std.println (% 12 5); # 2

    # / on integers is integer division
    std.println (/ 10 4); # 2
    std.println (/ 10.0 4.0); # 2.5

    std.println (+ 1 2 3 4 5); # 15
    std.println (- 10 1 2 3); # 4

    # unary operators
    std.println (+ 10); # 10
    std.println (- 10); # -10
};
```

Bitwise operators perform operations on individual bits. (The binary form is not a requirement.)

```
import "std/io.orb";

fnc main () () {
    # bitwise AND
    std.println (& 0b1100 0b1010); # 0b1000 or 8

    # bitwise OR
    std.println (| 0b1100 0b1010); # 0b1110 or 14

    # bitwise XOR
    std.println (^ 0b1100 0b1010); # 0b0110 or 6

    # bitwise NOT (unary)
    std.println (~ 0b1100); # -13

    # shift left
    std.println (<< 0b1100 2); # 0b11_0000 or 48

    # shift right
    std.println (>> 0b1100 2); # 0b0011 or 3
};
```

Comparison operators compare consecutive arguments and return a boolean value.

```
fnc main () () {
    == 1 1; # true
    == 1 2; # false
    != 1 2; # true
    < 1 2;  # true
    < 1 1;  # false
    <= 1 2; # true
    > 1 2;  # false
    >= 1 2; # false

    < 1 2 3 4 5; # true
    < 1 2 9 3 4; # false
};
```

It should be noted that `!=` is not variadic - it always takes exactly two arguments.

A useful property of comparison operators is short-circuiting. This means that if the final boolean result is already determined, the remaining arguments will not be executed.

```
import "std/io.orb";

fnc main () () {
    < 1 2 0 3 (std.scanI32);
};
```

`std.scanI32` is a function that attempts to read a 32-bit integer value from the standard input. Normally, it would pause the execution of our program until this value is read. Here, that call will never be executed, since it is already clear that the comparison result is `false`.

Finally, we have the logical operators.

```
import "base.orb";

fnc main () () {
    ! true;  # false
    ! false; # true

    && true true;  # true
    && true false; # false

    || false true;  # true
    || false false; # false
};
```

Unlike all the other operators we've seen so far, `&&` and `||` are not special forms - they are macros. They are defined in **base.orb**, which is why we needed to import this file. We'll learn more about macros in *Advanced concepts: Macros*.

Just like comparison operators, `&&` and `||` perform short-circuiting.