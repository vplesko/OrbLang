---
layout: default
title: Arrays
---
# {{ page.title }}

Arrays contain one or more values of the same type. For example, `(i32 4)` is an array of four `i32` elements.

Array elements are fetched using the `[]` special form, providing the element's index. Orb uses zero-based indexing.

```
fnc sumElems (a:(i32 4)) i32 {
    sym sum:i32;
    range i 4 {
        += sum ([] a i);
    };
    ret sum;
};
```

Arrays can contain other arrays. `((i32 4) 4)` is an array of four arrays of four elements of type `i32`. There is a total of sixteen `i32` values in this type. This type can be more simply expressed with `(i32 4 4)`.

`[]` is variadic - it can handle more than a single index.

```
fnc sumElemsOf3dMatrix (a:(f32 10 10 10)) f32 {
    sym sum:f32;
    range i 10 {
        range j 10 {
            range k 10 {
                += sum ([] a i j k);
            };
        };
    };
    ret sum;
};
```

Arrays are value types. In some languages, passing an array to a function is really passing a pointer to its elements. In Orb, however, all elements get copied, and modifying the argument does not modify the original array.

Arrays can be assigned new array values.

```
fnc main () () {
    sym a:(i32 4) b:(i32 4);

    range i 4 {
        = ([] a i) i;
    };

    = b a; # reassign b with the current elements of a
};
```

Arrays can be constructed using the `arr` macro from **base.orb**. This code declares `a` as an array of `i32` containing given values:

```
    sym (a (arr i32 10 11 12 13));
```