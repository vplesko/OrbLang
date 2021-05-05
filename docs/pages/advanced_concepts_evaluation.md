---
layout: default
title: Evaluation
---
# {{ page.title }}

There are two ways to process Orb code - through compilation or through evaluation. Compilation means that the code will be compiled into the binary executable. Evaluation is the execution of code at compile-time. Until now, all of our code was compiled.

To switch from compilation to evaluation, use the `eval` special form.

```
import "base.orb";

fnc main () () {
    eval (repeat 10 {
        # ...
    });
};
```

Values and symbols are either evaluated or compiled.

```
fnc main () () {
    # x is an evaluated symbol
    eval (sym x:i32);

    # y is a compiled symbol
    sym y:i32;
};
```

You may even wrap entire function definitions in `eval` to create evaluated functions. These will only be callable through evaluation.

```
eval (fnc foo () () {
    # ...
});
```

As you can see, there is a clear separation between what is evaluated and what is compiled. While you can switch from compilation to evaluation using `eval`, you cannot go the other way round.

Orb has several rules that make evaluated values more convenient to use. When the compilation needs to work with an evaluated value, it will implicitly compile it first. Worth noting, the resulting compiled value will be non-ref.

```
import "std/io.orb";

fnc print (x:i32) () {
    std.println "x=" x;
};

fnc main () () {
    eval (sym (x 100));
    # x gets compiled as the function is called
    print x;

    # literals are also evaluated values, so this one also gets compiled
    print 99;
};
```

Operator instructions will implicitly be evaluated if their arguments are evaluated values.

```
import "std/io.orb";

fnc main () () {
    eval (sym x:i32 (y 10));
    = x y; # no need to wrap in eval

    # + is evaluated, and then the result is compiled
    std.println (+ x y);
};
```

A function can be marked as both evaluated and compiled by appending `::evaluable` to its name. The compiler is smart enough to know when a call to such a function can be executed at compile-time, without you needing to wrap the call in `eval`.

```
import "std/io.orb";

fnc square::evaluable (x:i32) i32 {
    ret (* x x);
};

fnc main () () {
    sym (x 10);
    # the call to square is compiled
    std.println (square x);

    eval (sym (y 11));
    # the call to square is evaluated
    std.println (square y);
};
```

> `::` is used to place attributes on a node. Attributes will be explained later.

We mentioned earlier that array sizes and tuple element indexes must be known at compile-time. This actually means that they must be evaluated values. They do not need to be literals, though.

```
fnc main () () {
    eval (sym (n 10));

    sym a:(i32 n);
};
```

Another useful special form is `isEval`, which is used to find out whether a value or current code is evaluated or not.

```
fnc main () () {
    sym x:i32;
    eval (sym y:i32);

    # for values
    isEval x; # false
    isEval y; # true

    # for current execution
    isEval;        # false
    eval (isEval); # true
};
```