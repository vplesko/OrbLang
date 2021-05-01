---
layout: default
title: Id, type, and raw
---
# {{ page.title }}

As we already established, `id`, `type`, and `raw` are primitive types. These types (and values of their type) cannot be compiled - you can only use them in evaluation.

Just like with any other type, you can declare symbols of these types, reassign them with new values, and pass them as arguments to functions.

## Id

`id` represents an identifier. Identifier literals are of this type.

Normally, processing an identifier literal results in a lookup of what it represents. To surpress this behaviour, you need to escape the identifier. Then, you will get the value of type `id`.

You can combine two `id` values into a new one with `+`. Unsigned, boolean, and type values can be cast into `id`.

```
eval (sym (baseName \foo));

eval (sym (compositeName (+ baseName (cast id i32))));
```

To perform a lookup on an `id` value, surround it in parenthesis.

```
import "std/io.orb";

fnc main () () {
    sym (x 0) (y 1);

    eval (sym (name \x));
    std.println (name); # prints 0

    = name \y;
    std.println (name); # prints 1
};
```

## Type

`type` represents a type. You can use it wherever a type is required, eg. in `sym` instructions.

```
fnc main () () {
    eval (sym (ty i32));

    sym x:ty;
    sym a:(ty 4);
};
```

## Raw

`raw` is a list that can contain almost anything that can be expressed in Orb. Most commonly, it represents unprocessed Orb code. As we've learned by now, the code we write consists of `raw` values. However, they quickly get processed into something else by the compiler. To surpress this behavour, you need to escape them.

```
    # the escaped code will not be executed
    # it represents a raw value
    eval (sym (r \(std.println myVar)));
```

`()` (or `{}`) represents an empty `raw` value, ie. a `raw` with no elements. Escaping it is not required.

Values inside `raw` retain their ref properties.

It is very important to remember that, regardless of their elements, **`raw` values are considered non-owning and their elements will not be dropped**! Bad usage of `raw` values can result in automatic cleanup not happening, which may manifest as memory leaks or worse.

Just as hazardous is the fact that their careless usage may result in the same value being dropped multiple times. This code will compile, but will result in undefined behaviour:

```
import "base.orb";
import "std/One.orb";

fnc main () () {
    # creates a raw containing a single owning non-ref value
    sym (r::evaluated (base.makeRawWith (std.makeOne i32)));

    # non-ref value is dropped, memory is freed
    [] r 0;

    # non-ref value is dropped again, resulting in double free
    [] r 0;
};
```
{: .code-error}

Great caution must be exercised when using `raw` values! Really, their main intended usage is to write macros.