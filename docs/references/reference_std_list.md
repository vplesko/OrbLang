---
layout: default
title: List
---
# {{ page.title }}

This section documents definitions found in **std/List.orb**.

`std.List` is a class of resizable array types, which store their elements on the heap and will automatically deallocate all used memory.

These types must only be used by compiled values.

Length refers to the number of elements currently held by a list. If the list is not allocated, its length is zero.

Capacity refers to the capacity of a list, ie. how many elements can its current memory allocation hold. If the list is not allocated, its capacity is zero. Capacity is always greater than or equal to the length.

## `std.List elemTy<type> -> type`

## `std.List elemTy<type> defineDrop<bool> -> type`

Defines a `std.List` type pointing to an array of `elemTy`, if one wasn't previously defined.

Returns that type.

If `defineDrop` is provided and is `false`, will only declare the drop function of this `std.List` type. This is used when `elemTy` has a nested `std.List` element that somehow points to other `elemTy` values, creating a circular dependency in drop definitions.

You must afterwards define the drop function using `std.defineDrop`.

```
data Tree {
    value:i32
    next:(std.List Tree false)
};

# define the drop function for this std.List
std.defineDrop (std.List Tree);
```

## `std.makeList elemTy<type> len<unsigned> -> std.List`

Defines a `std.List` type pointing to an array of `elemTy`, if one wasn't previously defined.

Returns a `std.List` pointing to a zero-initialized array of `len` amount of `elemTy` stored on the heap.

## `std.makeListWith elemTy<type> elem... -> std.List`

Defines a `std.List` type pointing to an array of `elemTy`, if one wasn't previously defined.

Constructs and returns a `std.List` pointing to an array stored on the heap, containing given elements. The initial length of the array is equal to the number of elements provided.

Each element is implicitly cast into `elemTy`.

## `std.getLen list<std.List> -> std.Size`

Returns the current length of `list`.

## `std.getCap list<std.List> -> std.Size`

Returns the current capacity of `list`.

## `std.isEmpty list<std.List> -> bool`

Returns `true` if the array `list` points to is empty, or if `list` is not allocated.

## `std.getElemTy list<std.List> -> type`

Gets the type of elements of the array pointed to by `list`.

## `std.[] list<std.List> ind<integer>`

Returns the element at index `ind` in the array `list` points to. If `list` is not allocated, or `ind` is out of bounds, the result is undefined behaviour.

## `std.getFront list<std.List>`

Returns the first element in the array `list` points to. If `list` is not allocated, or the array is empty, the result is undefined behaviour.

## `std.getBack list<std.List>`

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

---

```
import "base.orb";
import "std/io.orb";
import "std/List.orb";

fnc printElems (list:(std.List i32)::noDrop) () {
    std.print "list elements:";
    # iterate on all elements of list
    std.range it list {
        std.print ' ' (it);
    };
    std.println;
};

fnc main () () {
    # std.List zero-initialized as non-allocated
    sym list:(std.List i32);

    if (== (std.getElemTy list) i32) {
        std.println "list has an array pointer to i32.";
    };

    if (std.isEmpty list) {
        std.println "list is empty.";
    };

    # reassigns list to point to an array of 10 zero-initialized values
    = list (std.makeList i32 10);

    std.println "list length: " (std.getLen list);
    std.println "list capacity: " (std.getCap list);
    printElems list;

    # reassigns list to point to an array of given values
    = list (std.makeListWith i32 1 2 3 4);

    std.println "list length: " (std.getLen list);
    std.println "Front element of list is " (std.getFront list) ".";
    std.println "Element 1 of list is " (std.[] list 1) ".";
    std.println "Element 2 of list is " (std.[] list 2) ".";
    std.println "Back element of list is " (std.getBack list) ".";

    # reserves capacity for at least 7 elements
    std.reserve list 7;

    std.println "list length: " (std.getLen list);
    std.println "list capacity: " (std.getCap list);

    # appends new values to the list
    std.push list 5 6 7;

    printElems list;

    # removes the back element from the list
    std.pop list;

    printElems list;

    # resizes the list up
    std.resize list 10;

    printElems list;

    # resizes the list down
    std.resize list 2;

    printElems list;

    # removes all elements from the list
    std.clear list;

    std.println "list length: " (std.getLen list);
};
```

Output:

```
list has an array pointer to i32.
list is empty.
list length: 10
list capacity: 10
list elements: 0 0 0 0 0 0 0 0 0 0
list length: 4
Front element of list is 1.
Element 1 of list is 2.
Element 2 of list is 3.
Back element of list is 4.
list length: 4
list capacity: 7
list elements: 1 2 3 4 5 6 7
list elements: 1 2 3 4 5 6
list elements: 1 2 3 4 5 6 0 0 0 0
list elements: 1 2
list length: 0
```