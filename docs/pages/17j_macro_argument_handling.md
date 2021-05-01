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

By default, macro arguments are escaped and then processed, before the invocation begins. By specifying `::preprocess` on a macro argument, it will not be escaped, but only processed.

This is actually the fix to our problem.

```
mac printThrice (x::preprocess delim::preprocess) {
    ret \(std.println ,x ,delim ,x ,delim ,x);
};
```

Whenever a macro argument may appear multiple times in the returned code snippet, it should be marked with `::preprocess`. This means that the code snippet used as the argument at the place of invocation will be executed before going into the argument.

So, in our program above, `x` will be moved only once, and the resulting non-ref value will be used for printing all three times.

There are other useful argument settings. `::plusEscape` will escape the argument twice, and then process it and invoke the macro.

`::variadic` can be used on the last macro argument to mark the argument as variadic. This argument will be able to take arbitrarily many arguments. All surplus arguments will be packed into a `raw` value and this value will be the last argument presented to the macro.

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

`::variadic` can be combined with the other ones, as eg. `::(variadic preprocess)`.

The type of `foo` can be expressed with `(mac 3::variadic)`.