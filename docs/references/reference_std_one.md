---
layout: default
title: One
---
# {{ page.title }}

This section documents definitions found in **std/One.orb**.

`std.One` is a class of smart pointer types, which automatically manage memory for an object stored on heap, without the need to manually deallocate it.

These types must only be used by compiled symbols.

## `std.getValTy one`

Gets the type pointed to by `one`.

## `std.One valTy<type>`

Defines and returns a `std.One` type which points to `valTy`. If the same type was already defined, returns it only.

## `std.makeOne valTy<type>`

Defines a `std.One` type pointing to `valTy`, then returns a `std.One` value pointing to a zero initialized `valTy` stored on heap.

## `std.* one`

Returns the value `one` points to as a ref value.

## `std.-> one ind`

Indexes the value `one` points to with index `ind` and returns that element as a ref value.

## `std.isNull one`

Returns whether `one` does not point to a value.

## `std.getPtr one`

Returns the underlying pointer `one` uses to point to its value as a non-ref value. If `one` does not point to a value, the returned pointer will be null.