---
layout: default
title: Common
---
# {{ page.title }}

This section documents definitions found in **std/common.orb**.

## `std.Size`

Alias for the type used by the standard library as the container size type.

## `std.malloc elemTy<type> len<unsigned> =elemTy []`

Allocates enough memory to store `len` values of `elemTy`. On success, returns an array pointer pointing to the allocated memory. On failure, null is returned.

## `std.calloc elemTy<type> len<unsigned> =elemTy []`

Has the same behaviour as `std.malloc` with the added initialization of the allocated bytes to zero.

## `std.realloc p elemTy<type> len<unsigned> =elemTy []`

Reallocates the previously allocated unfreed memory `p` points to. This may be done by either expanding or contracting the memory area `p` points to; or by allocating a new area of memory, copying the contents of the previous memory area (or its subset, if reallocating to a smaller size), and then freeing the previously used memory.

If allocation fails, returns null.

If `p` is null, simply allocates new memory.

## `std.getArrPtrDist elemTy<type> arrPtrHi arrPtrLo =std.Size`

Returns the number of `elemTy` values which could be placed in memory between the locations pointed to by `arrPtrLo` and `arrPtrHi`.

## `std.getArrPtrToInd elemTy<type> arrPtr ind<integer> =elemTy []`

Returns an array pointer to `elemTy` to the element with index `ind` of `arrPtr`. If the index is invalid, behaviour may be undefined.

## `std.setElemsToZero elemTy<type> arrPtr len<unsigned> =elemTy []`

Resets the first `len` values of `elemTy` at the location to which `arrPtr` points to their zero state.

Returns `arrPtr` cast to `(elemTy [])`.

## `std.defineDrop stdTy<type>`

Defines the drop function of a `std.One` or `std.List` type.

This is used when such a type had its drop function declared, but not defined.