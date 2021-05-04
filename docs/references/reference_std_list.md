---
layout: default
title: List
---
# {{ page.title }}

This section documents definitions found in **std/List.orb**.

`std.List` is a class of resizable array types, which store their elements on heap, and automatically deallocate all used memory.

These types must only be used by compiled symbols.

Length refers to the number of elements currently held by `list`. If `list` is not allocated, its length is zero.

Capacity refers to the capacity of `list`, that is how many elements can its current memory allocation hold. If `list` is not allocated, its capacity is zero.

## `std.getElemTy list`

Gets the type of elements of the array pointed to by `list`.

## `std.getLen list`

Returns the number of elements in the array `list` points to, or zero if it doesn't point to an array.

## `std.getCap list`

Returns the element capacity of the underlying storage `list` points to, or zero if it doesn't point to an array.

## `std.isEmpty list`

Returns whether the array `list` points to is empty, or if it doesn't point to an array.

## `std.[] list ind`

Returns the element at index `ind` in the array `list` points to. If `list` does not point to an array, or `ind` is out of bounds, the result is undefined behaviour.

## `std.getFront list`

Returns the first element in the array `list` points to. If `list` does not point to an array, or the array is empty, the result is undefined behaviour.

## `std.getLast list`

Returns the last element in the array `list` points to. If `list` does not point to an array, or the array is empty, the result is undefined behaviour.

## `std.getArrPtr list`

Gets the underlying array pointer `list` uses to point to its array as a non-ref value. If `list` does not point to an array, the returned array pointer will be null.

## `std.range it<id> list body<block>`

Iterates through the elements of `list`, executing `body` for each. The current iteration element can be fetched by surrounding `it` in parenthesis, as `(it)`.

Causing `list` to be resized, reassigned, or reallocated in `body` results in undefined behaviour.

## `std.rangeRav it<id> list body<block>`

Same as `std.range`, but iterates through the elements in reverse order.

## `std.List elemTy<type>`

Defines and returns a `std.List` type which points to an array of `elemTy`. If the same type was already defined, returns it only.

## `std.makeList elemTy<type> len`

Defines a `std.List` type pointing to an array of `elemTy`, then returns a `std.List` value pointing to a zero-initialized array of `elemTy` stored on heap.

## `std.makeListWith elemTy<type> elem...`

Defines a `std.List` type pointing to an array of `elemTy`. Constructs and returns a `std.List` value pointing to an array containing given elements. The initial size of the array is equal to the number of elements provided. Each element is implicitly cast into `elemTy`.

## `std.resize list len`

Resizes `list` to contain exactly `len` elements. If `len` is greater than the current length, appended elements will be zero-initialized.

## `std.reserve list len`

Increases the capacity of `list` to be able to contain `len` elements. If `len` is less than or equal to the current capacity, has no effect.

Does not change the length of `list`.

## `std.push list elem...`

Appends elements to the array of `list`. Each element is implicitly cast into the element type of the array of `list`.

## `std.pop list`

Removes the last element from the array `list` points to. If `list` does not point to an array, or the array is empty, the result is undefined behaviour.

## `std.clear list`

Removes all elements from `list`.