---
layout: default
title: Messaging
---
# {{ page.title }}

Debugging evaluated code may be difficult, which is were the `message` special form comes in handy. It is used to print messages to the programmer during compilation.

```
    message "Debug point 1, foo=" foo;

    message "Type of var is " (typeOf var);
```

It can only handle some types of arguments, and they must be evaluated values. On the plus side, it is variadic.

You may use it as `message::warning` to give a warning to the programmer. Or, you may use it as `message::error` to return a compilation error, terminating the compilation process.

The first argument can be marked with `::loc`. This means that the code location the compiler prints to the programmer will be the code location where the argument was found. This argument will not be printed in the message and does not need to be an evaluated value.

```
    message::error x::loc "Value of argument was invalid.";
```

This is mostly useful when writing macros.