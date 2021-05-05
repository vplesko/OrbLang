---
layout: default
title: List
---
# {{ page.title }}

This section documents definitions found in **std/List.orb**.

`std.List` is a class of resizable array types, which store their elements on the heap and will automatically deallocate all used memory.

These types must only be used by compiled symbols.

Length refers to the number of elements currently held by a list. If the list is not allocated, its length is zero.

Capacity refers to the capacity of a list, ie. how many elements can its current memory allocation hold. If the list is not allocated, its capacity is zero. Capacity is always greater than or equal to the length.

## `std.List elemTy<type> =type`

Defines a `std.List` type pointing to an array of `elemTy`, if one wasn't previously defined.

Returns that type.

## `std.makeList elemTy<type> len<unsigned> =std.List`

Defines a `std.List` type pointing to an array of `elemTy`, if one wasn't previously defined.

Returns a `std.List` pointing to a zero-initialized array of `len` amount of `elemTy` stored on the heap.

## `std.makeListWith elemTy<type> elem... =std.List`

Defines a `std.List` type pointing to an array of `elemTy`, if one wasn't previously defined.

Constructs and returns a `std.List` pointing to an array stored on the heap, containing given elements. The initial length of the array is equal to the number of elements provided.

Each element is implicitly cast into `elemTy`.

## `std.getElemTy list<std.List> =type`

Gets the type of elements of the array pointed to by `list`.

## `std.getLen list<std.List> =std.Size`

Returns the current length of `list`.

## `std.getCap list<std.List> =std.Size`

Returns the current capacity of `list`.

## `std.isEmpty list<std.List> =bool`

Returns `true` if the array `list` points to is empty, or if `list` is not allocated.

## `std.[] list<std.List> ind<integer>`

Returns the element at index `ind` in the array `list` points to. If `list` is not allocated, or `ind` is out of bounds, the result is undefined behaviour.

## `std.getFront list<std.List>`

Returns the first element in the array `list` points to. If `list` is not allocated, or the array is empty, the result is undefined behaviour.

## `std.getLast list<std.List>`

Returns the last element in the array `list` points to. If `list` is not allocated, or the array is empty, the result is undefined behaviour.

## `std.getArrPtr list<std.List>`

Gets the underlying array pointer `list` uses to point to its array as a non-ref value. If `list` is not allocated, the returned array pointer will be null.

## `std.push list<std.List> elem...`

Appends elements to `list`. Each element is implicitly cast into the element type of of `list`.

## `std.pop list<std.List>`

Removes the last element from `list`. If `list` is not allocated or has no elements, the result is undefined behaviour.

Does **not** return the popped element.

## `std.resize list<std.List> len<std.Size>`

Resizes `list` to contain exactly `len` elements.

If `len` is greater than the current length, appended elements will be zero-initialized.

If `len` is less than the current length, surplus elements will be dropped in reverse order of how they are stored.

## `std.reserve list<std.List> len<std.Size>`

Increases the capacity of `list` to be able to contain `len` elements. If `len` is less than or equal to the current capacity, has no effect.

Does not change the length of `list`.

## `std.clear list<std.List>`

Removes all elements from `list`.

## `std.range it<id> list<std.List> body<block>`

Iterates through the elements of `list`, executing `body` for each. The current iteration element can be fetched by surrounding `it` in parenthesis, as `(it)`.

If `list` is empty, performs no iterations.

Causing `list` to be resized, reassigned, or reallocated in `body` results in undefined behaviour.

## `std.rangeRev it<id> list<std.List> body<block>`

Same as `std.range`, but iterates through the elements in reverse order.