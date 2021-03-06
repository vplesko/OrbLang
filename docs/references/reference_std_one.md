---
layout: default
title: One
---
# {{ page.title }}

This section documents definitions found in **std/One.orb**.

`std.One` is a class of smart pointer types, which automatically manage memory for an object stored on the heap, without the need to manually deallocate it.

These types must only be used by compiled values.

## `std.One valTy<type> -> type`

## `std.One valTy<type> defineDrop<bool> -> type`

Defines a `std.One` type pointing to `valTy`, if one wasn't previously defined.

Returns that type.

If `defineDrop` is provided and is `false`, will only declare the drop function of this `std.One` type. This is used when `valTy` has a nested `std.One` element that somehow points to another `valTy`, creating a circular dependency in drop definitions.

You must afterwards define the drop function using `std.defineDrop`.

```
data Chain {
    value:i32
    next:(std.One Chain false)
};

# define the drop function for this std.One
std.defineDrop (std.One Chain);
```

## `std.makeOne valTy<type> -> std.One`

Defines a `std.One` type pointing to `valTy`, if one wasn't previously defined.

Returns a `std.One` pointing to a zero-initialized `valTy` stored on the heap.

## `std.makeOneWith val -> std.One`

Defines a `std.One` type pointing to the type of `val`, if one wasn't previously defined.

Returns a `std.One` pointing to a copy of `val` stored on the heap.

## `std.isNull one<std.One> -> bool`

Returns whether `one` does not currently point to a value on the heap.

## `std.getValTy one<std.One> -> type`

Gets the type pointed to by `one`.

## `std.* one<std.One>`

Returns the value `one` points to as a ref value.

## `std.-> one<std.One> ind`

Indexes the value `one` points to with index `ind` and returns that element as a ref value.

## `std.getPtr one<std.One> -> pointer`

Returns the underlying pointer `one` uses to point to its value as a non-ref value.

If `one` does not point to a value, the returned pointer will be null.

---

```
import "base.orb";
import "std/io.orb";
import "std/One.orb";

data Controller {
    res:String
} (lam (this:Controller::noDrop) () {
    if (!= ([] this res) null) {
        std.println "Releasing resource: " ([] this res);
    };
});

fnc changeResource (ctrl:(Controller *) res:String) () {
    = (-> ctrl res) res;
};

fnc main () () {
    # std.One zero-initialized as non-allocated
    sym ctrl:(std.One Controller);

    if (== (std.getValTy ctrl) Controller) {
        std.println "ctrl has a pointer to Controller.";
    };

    if (std.isNull ctrl) {
        std.println "ctrl is null.";
    };

    # reassigns ctrl to point to a zero-initialized value
    = ctrl (std.makeOne Controller);

    if (! (std.isNull ctrl)) {
        std.println "ctrl is not null.";
    };

    # reassigns ctrl to point to the given value
    = ctrl (std.makeOneWith (make Controller (res "resource0")));

    # reassigns the value pointed to by std.One
    = (std.* ctrl) (make Controller (res "resource1"));

    std.println "ctrl owns: " (std.-> ctrl res);

    # pass the underlying pointer to a function
    # since ctrl and the value weren't reassigned, no dropping happens
    changeResource (std.getPtr ctrl) "resource2";

    # drops the value std.One points to
};
```

Output:

```
ctrl has a pointer to Controller.
ctrl is null.
ctrl is not null.
Releasing resource: resource0
ctrl owns: resource1
Releasing resource: resource2
```