---
layout: default
title: Array pointers
---
# {{ page.title }}

Array pointers are pointers that point to multiple values, which are contigously laid out. They may point to a start of an array, where the size of that array may not be specified.

Where pointers were dereferenced with `*`, array pointers can only be indexed with `[]`.

```
fnc main () () {
    sym a:(i32 100);

    # (& a) is of type (i32 100 *), so cast is needed
    sym (p (cast (i32 []) (& a)));
    = ([] p 0) 1;
};
```