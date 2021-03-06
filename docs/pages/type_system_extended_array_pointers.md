---
layout: default
title: Array pointers
---
# {{ page.title }}

Array pointers are pointers that point to multiple values, which are contiguously laid out in memory.

Array pointers are indexed with `[]`. Unlike regular pointers, they cannot be dereferenced with `*`.

```
fnc main () () {
    sym array:(i32 100);

    # (& array) is of type (i32 100 *), so a cast is needed
    sym (arrayPtr (cast (i32 []) (& array)));

    # modifies the element of array
    = ([] arrayPtr 0) 1;
};
```

Array pointers contain no information on how many elements they point to. Indexing them with invalid indexes may result in undefined behaviour.

> Undefined behaviour means that the faulty code would not raise compilation errors, but the program execution cannot be expected to give desired results. You should never rely on undefined behaviour in your code.

Array pointers will usually point to memory allocated on the heap. Check out the standard library references for more convenient ways to deal with heap memory.