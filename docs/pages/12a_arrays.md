---
layout: default
title: Arrays
---
# {{ page.title }}

Arrays contain one or more values of the same type. For example, `(i32 4)` expresses a type which is an array of four `i32` elements.

Array elements are fetched using the `[]` special form, providing the element's index. Orb uses 0-based indexing.

```
fnc sumElems (a:(i32 4)) i32 {
    sym sum:i32;
    range i 4 {
        += sum ([] a i);
    };
    ret sum;
};
```

Arrays can contain other arrays. `((i32 4) 4)` represents an array of four arrays of four elements of type `i32`. There is a total of sixteen `i32` values in this type. This type can be more simply expressed with `(i32 4 4)`.

`[]` is variadic - it can handle more than a single index.

```
fnc sumElemsOf3dMatrix (a:(f32 10 10 10)) f32 {
    sym sum:f32;
    range x 10 {
        range y 10 {
            range z 10 {
                += sum ([] a x y z);
            };
        };
    };
    ret sum;
};
```

Arrays are value types. In some languages, passing an array to a function is really passing a pointer to its elements. In Orb, however, all of the elements are passed and modifying the argument does not modify the starting array.

This also means that array symbols can be assigned new values.

```
fnc main () () {
    sym a:(i32 4) b:(i32 4);
    # construct elements of a and b

    = b a; # reassign b with the current value of a
};
```

Arrays can be constructed using the `arr` macro from **base.orb**.

```
    sym (a (arr i32 0 1 2 3));
```