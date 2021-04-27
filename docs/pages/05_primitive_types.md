---
layout: default
title: Primitive types
---
# Primitive types

By now, we've already learned how to perform various calculations, and we've seen a few types being specified here and there, but those types were never explained in detail. Let's fix that right now by introducing primitive types.

Primitive types are types which are built into the language itself. One group of these are signed integers, and these are: `i8`, `i16`, `i32`, and `i64`. These can be positive, negative, or zero. The number in their name denotes how many bits are used in their storage, which affects the range of values they can contain.

Another related group are unsigned integers. They are: `u8`, `u16`, `u32`, and `u64`. These cannot be negative. Similarly to signed integers, the number in their name denotes their bit size.

`bool` is the logical boolean type. It can only have two values, `true` or `false`.

Floating point types are `f32` and `f64`. These are real numbers. The number in their name also denotes their bit size.

`c8` is the character type. Example values are `'A'` and `'\n'`. Their size is always a single byte.

`ptr` is the nondescript pointer. If you are familiar with the C programming language, they are equivalent to `void*`. Other pointer types will be explained later. `null` literal is of type `ptr`.

There are three more primitive types: `id`, `type`, and `raw`. Explaining these would require familiarity with Orb concepts we haven't touched on yet, so we will return to them later.