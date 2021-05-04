---
layout: default
title: Function types
---
# {{ page.title }}

Functions also have a type. Their type is defined by the types of their arguments and their return value.

For example, a function declared with `fnc square (x:i32) i32;` has the type which can be expressed as `(fnc (i32) i32)`. Our main functions so far have had the signature `fnc main () ()`, and their type can be expressed as `(fnc () ())`.

You may have symbols of function types and you may even pass them as arguments to functions. You may then call into them the same way as you would call into a function.

```
fnc accumulate (func:(fnc (i32) i32) a:(i32 10)) i32 {
    sym (sum 0);
    range i 10 {
        += sum (func ([] a i));
    };
    ret sum;
};
```