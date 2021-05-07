---
layout: default
title: Pointers
---
# {{ page.title }}

Pointers refer to an element that may have been defined elsewhere. For example, `(i32 *)` is a pointer to an `i32`.

You may use the unary `&` to get a pointer to a ref value.

Unary `*` is used to dereference a pointer, ie. to fetch the value it points to. The result will be a ref value.

```
fnc main () () {
    sym (x 0);

    sym (p (& x)); # p points to x
    = (* p) 1; # x is now 1
};
```

If you want to pass a value to a function and have it modified, you must pass a pointer to it instead.

```
fnc setToNextVal (x:(i32 *)) () {
    = (* x) (complexCalculation);
};

fnc main () () {
    sym (x 0);
    setToNextVal (& x);
};
```