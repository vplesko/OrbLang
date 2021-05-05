---
layout: default
title: IO
---
# {{ page.title }}

This section documents definitions found in **std/io.orb**.

## `std.print val...`

Prints each of provided values to the standard output.

Each value must be a numeric, a char, or a string.

If a null string is provided, the result is undefined behaviour.

```
    std.print x ' ' y;
```

## `std.println val...`

Does the same as `std.print` and then outputs an additional newline character.

```
    std.println "Value of x is " x;
```

## `std.scanI8 =i8`

## `std.scanI16 =i16`

## `std.scanI32 =i32`

## `std.scanI64 =i64`

## `std.scanU8 =u8`

## `std.scanU16 =u16`

## `std.scanU32 =u32`

## `std.scanU64 =u64`

## `std.scanF32 =f32`

## `std.scanF64 =f64`

Reads a value of the corresponding type from the standard input and returns it.

```
    sym (x (std.scanI32));
```