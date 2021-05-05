---
layout: default
title: Message
---
# {{ page.title }}

Used to print information to the programmer while compiling.

## `message val... =()`

Prints information to the programmer based on arguments. The exact output depends on the compiler implementation.

Each argument must be an evaluated value of a numeric, char, boolean, identifier, or type type, or a null pointer, or a non-null array pointer which is a string.

```
    message "Debug point 0. Type of f is: " (typeOf f);
```

`::warning` on `message` is a signal to the compiler on the type of message this represents.

`::error` on `message` is a signal to the compiler on the type of message this represents. Additionally, it causes the compiling to be stopped and reported as a failure.

`message` must not be marked with both `warning` and `error`.

`::loc` on the first argument gives a hint to the compiler on how to display the information. The argument may in this case be of any type or an untyped value. If the first argument is marked with `::loc`, there must be a least two arguments.