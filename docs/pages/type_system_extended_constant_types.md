---
layout: default
title: Constant types
---
# {{ page.title }}

A type can be expressed as constant by using the `cn` identifier. For example, `(i32 cn)` is a constant `i32`. Ref values of this type cannot be reassigned new values.

```
fnc main () () {
    sym (x:(i32 cn) 100);

    = x 101; # error!
    ++ x; # error!
};
```
{: .code-error}

If an element of an array or tuple is of a constant type, the entire type as a whole is considered constant.

```
fnc main () () {
    sym (x:((i32 cn) c8) (tup 100 'A'));

    = ([] x 0) 1; # error!
    = ([] x 1) 'A'; # this is ok
    = x (tup 999 'Z'); # error!
};
```
{: .code-error}

Symbols of a constant type must be explicitly initialized (default zero-initialization is not allowed).