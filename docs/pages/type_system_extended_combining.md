---
layout: default
title: Combining
---
# {{ page.title }}

We've seen that `*` expresses a pointer, `[]` an array pointer, `cn` a constant type, and a number can express an array. These can be combined to express a more complex type. For example, `(i32 * 4)` is an array of four pointers to `i32` values.

Following this, you can have some very long type definitions. An easy way to read them is from right to the left. For example, `(i32 [] 4 *)` is a pointer (right-most `*`) of an array of four (from `4`) array pointers (from `[]`) of `i32` values (base type).

The base does not need to a primitive type. For example, `((i32 f32) [])` is an array pointer to tuples that have elements of types `i32` and `f32`.

`cn` affects what is found to the left of it up to the first pointer or array pointer.

For example, `(i32 * cn)` is a constant pointer (meaning it always points to the same value) to a non-constant `i32` value.

On the other hand, `(i32 cn *)` is a pointer to a constant `i32` value. The pointer itself is not constant.

`(i32 cn * cn)` combines the two - the pointer always points to the same value, and cannot be used to modify that value.

```
fnc main () () {
    sym x:i32;

    sym (p0:(i32 * cn) (& x));
    = (* p0) 0; # this is ok
    = p0 null; # error!

    sym (p1:(i32 cn *) (& x));
    = (* p1) 1; # error!
    = p1 null; # this is ok

    sym (p2:(i32 cn * cn) (& x));
    = (* p2) 2; # error!
    = p2 null; # error!
};
```
{: .code-error}

Casting a pointer to a constant value into a pointer to a non-constant value (eg. casting an `(i32 cn *)` into an `(i32 *)`) may result in undefined behaviour!