---
layout: default
title: Combining
---
# {{ page.title }}

We've seen that `*` expresses a pointer, `[]` an array pointer, `cn` a constant type, and a number can express an array. These can be combined to express a more complex type. For example, `(i32 * 4)` is an array of four pointers to `i32` values.

Following this, you can have some very long type definitions. An easy way to read them is from right to the left. For example, `(i32 [] 4 *)` is a pointer (right-most `*`) of an array of `4` array pointers (reading `[]`) of `i32` values (base).

The base does not need to a primitive type. For example, `((i32 f32) 4 [])` is an array pointer to arrays of four tuples, which have elements of types `i32` and `f32`.

`cn` affects what is found immediately to the left of it. For example, `(i32 cn *)` is a pointer to a constant `i32` value. The pointer itself is not constant.

This means that the pointer can be reassigned with a new value, but the value it points to cannot be modified through dereferencing that pointer.

```
fnc main () () {
    sym x:i32;

    sym (p:(i32 cn *) (& x));

    = (* p) 1; # error!

    = p null; # this is ok
};
```
{: .code-error}

On the other hand, `(i32 * cn)` is a constant pointer (meaning it always points to the same value) to a non-constant `i32` value.

```
fnc main () () {
    sym x:i32;

    sym (p:(i32 * cn) (& x));

    = (* p) 1; # this is ok now

    = p null; # error!
};
```
{: .code-error}

`(i32 cn * cn)` combines the two - the pointer always points to the same value, and cannot be used to modify that value.

Casting a pointer to a constant value into a pointer to a non-constant value (eg. casting a `(i32 cn *)` to a `(i32 *)`) may result in undefined behaviour!

> Undefined behaviour means that the faulty code would not raise compilation errors, but the program execution cannot be expected to give desired results. You should never rely on undefined behaviour in your code.