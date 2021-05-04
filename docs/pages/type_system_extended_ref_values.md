---
layout: default
title: Ref values
---
# {{ page.title }}

In Orb, there is a distinction between ref and non-ref values. Loading a symbol or an element of a symbol yields a ref value. Ref values can be reassigned with new values.

```
fnc main () () {
    sym x:i32;
    = x 1; # this is ok

    sym a:(i32 4);
    = ([] a 0) x; # this is ok
};
```

Trying to assign to a non-ref value results in an error.

```
fnc main () () {
    = 1 0; # error!
};
```
{: .code-error}