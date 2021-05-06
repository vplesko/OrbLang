---
layout: default
title: One
---
# {{ page.title }}

This section documents definitions found in **std/One.orb**.

`std.One` is a class of smart pointer types, which automatically manage memory for an object stored on the heap, without the need to manually deallocate it.

These types must only be used by compiled symbols.

## `std.One valTy<type> =type`

## `std.One valTy<type> defineDrop<bool> =type`

Defines a `std.One` type pointing to `valTy`, if one wasn't previously defined.

Returns that type.

If `defineDrop` is provided and is `false`, will only declare the drop function of this `std.One` type. This is used when `valTy` has a nested `std.One` element that somehow points to another `valTy`, creating a circular dependency in drop definitions.

You must afterwards define the drop function using `std.defineDrop (std.One valTy)`.

```
data Chain {
    value:i32
    next:(std.One Chain false)
};

# define the drop function for this std.One
std.defineDrop (std.One Chain);
```

## `std.makeOne valTy<type> =std.One`

Defines a `std.One` type pointing to `valTy`, if one wasn't previously defined.

Returns a `std.One` pointing to a zero-initialized `valTy` stored on the heap.

## `std.makeOneWith val =std.One`

Defines a `std.One` type pointing to the type of `val`, if one wasn't previously defined.

Returns a `std.One` pointing to a copy of `val` stored on the heap.

## `std.* one<std.One>`

Returns the value `one` points to as a ref value.

## `std.-> one<std.One> ind`

Indexes the value `one` points to with index `ind` and returns that element as a ref value.

## `std.getValTy one<std.One> =type`

Gets the type pointed to by `one`.

## `std.isNull one<std.One> =bool`

Returns whether `one` does not currently point to a value on the heap.

## `std.getPtr one<std.One> =pointer`

Returns the underlying pointer `one` uses to point to its value as a non-ref value.

If `one` does not point to a value, the returned pointer will be null.