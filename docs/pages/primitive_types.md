---
layout: default
title: Primitive types
---
# {{ page.title }}

Primitive types are types that are built into the language itself. One group of these are signed integers: `i8`, `i16`, `i32`, and `i64`. The number in their name denotes how many bits are used in their storage, which affects the range of values they can contain.

Another related group are unsigned integers: `u8`, `u16`, `u32`, and `u64`. Similarly to signed integers, the number in their name denotes their bit size.

`bool` is the logical boolean type. It can only have two values, `true` or `false`.

Floating-point types are `f32` and `f64`. The number in their name also denotes their bit size.

`c8` is the character type, eg. `'A'` and `'\n'`. Their size is always a single byte.

`ptr` is the nondescript pointer. They are equivalent to `void*` from the C programming language. `null` literal is of type `ptr`.

There are three more primitive types: `id`, `type`, and `raw`. Explaining these would require familiarity with Orb concepts we haven't touched on yet, so we will return to them later.