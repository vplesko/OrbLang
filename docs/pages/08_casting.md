---
layout: default
title: Casting
---
# Casting

In situations where we have a value of one type, but require a value of a different type, we need to cast it. Sometimes, Orb will do this automatically. This is referred to as implicit casting.

```
import "std/io.orb";

fnc main () () {
    sym (x:i32 (std.scanI8));
    + x 1:i16;
};
```

Here, `x` is an `i32` symbol being initialized with an `i8` value. Orb will implicitly cast it up to the type of `x`.

On the next line, we attempt to add `x` and an `i16` value, which are of different types. For this addition to be performed, they need to be cast into a single type. In this case, it is `1:i16` which will be cast up to `i32`. 

Orb is very strict when it comes to implicit casts. If a cast could result in a loss of information, it will not be allowed and you will get a compilation error. In addition to that, it is generally not possible to implicitly cast between types of different groups (eg. signed to unsigned, or floating to signed).

The following code will raise compilation errors:

```
import "std/io.orb";

fnc main () () {
    sym x:i32;
    = x 0.0; # error!
    = x (<< 1:i64 40); # error!

    sym y:u32;
    = y x; # error!

    sym b:bool;
    = x b; # error!
};
```
{: .code-error}

There are some exceptions to these rules, when the value is known at the time of compilation and guaranteed to not result in information loss. Even then, some of the strictness still holds.

In the cases where implicit casting is forbidden, we can cast explicitly by using the `cast` special form.

```
import "std/io.orb";

fnc main () () {
    sym x:i32;
    = x (cast i32 0.0);
    = x (cast i32 true);

    sym y:u32;
    = y (cast u32 x);
};
```