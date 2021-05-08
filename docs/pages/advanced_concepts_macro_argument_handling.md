---
layout: default
title: Macro argument handling
---
# {{ page.title }}

Another danger to look out for when writing macros is that code may end up being executed multiple times. We've seen this in our `printThrice` macro.

```
import "std/io.orb";

mac printThrice (x delim) {
    ret \(std.println ,x ,delim ,x ,delim ,x);
};

fnc main () () {
    sym (x "danger!");

    printThrice (>> x) ' ';
};
```
{: .code-error}

By default, macro arguments are escaped and then processed, before the invocation begins. By specifying `::preprocess` on a macro argument, it will not be escaped, only processed.

This is actually the fix to our problem.

```
mac printThrice (x::preprocess delim::preprocess) {
    ret \(std.println ,x ,delim ,x ,delim ,x);
};
```

Whenever a macro argument may appear multiple times in the returned code snippet, it should be marked with `::preprocess`. This means that the code snippet used as the argument will be executed before the invocation starts.

So, in our program above, `x` will be moved only once, and the resulting non-ref value will be printed three times.

There are other useful argument settings. `::plusEscape` will escape the argument twice, and then process it and invoke the macro.

`::variadic` can be used on the last macro argument to mark it as variadic. This argument will be able to take arbitrarily many arguments. They will be packed into a `raw` value as elements and this `raw` will be the last argument presented to the macro.

```
mac foo (a b rest::variadic) {
    # ...
};

fnc main () () {
    # a is 1
    # b is 2
    # rest is \(3 4 5)
    foo 1 2 3 4 5;

    # a is 1
    # b is 2
    # rest is \(3)
    foo 1 2 3;

    # a is 1
    # b is 2
    # rest is \()
    foo 1 2;
};
```

To make the last argument variadic and `preprocess`, mark it with `::(variadic preprocess)`.