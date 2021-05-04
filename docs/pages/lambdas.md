---
layout: default
title: Lambdas
---
# {{ page.title }}

You can declare nameless (or anonymous) functions in any scope by using `lam` macros from **base.orb**.

Lambdas are still functions and can be passed as arguments to other functions.

```
fnc callOnEach (func:(fnc (i32) ()) a:(i32 3)) () {
    range i 3 {
        func ([] a i);
    };
};

fnc main () () {
    callOnEach (lam (x:i32) () { std.println x; }) (arr i32 3 2 1);

    callOnEach (lam (x:i32) () { std.println (cast c8 x); }) (arr i32 65 66 67);
};
```

You may omit one `lam` argument to create a function that takes no arguments. For example, `(lam i32 { ret 0; })` takes no arguments and always returns zero.

Ommiting anotehr argument results in a function of type `(fnc () ())`, eg. `(lam { std.println; })`.