---
layout: default
title: Operators - non-unary
---
# {{ page.title }}

Used to perform calculations and/or modify operands.

## `= oper0 oper...`

Assigns new values to ref values.

Performs iteratively from right to left. The first iteration assigns the value of the rightmost operand to the one on the left of it and returns the value of that left-hand side operand as a ref value. Each following iteration assigns the value returned by the previous iteration to the next left-hand side operand, until all operands are used. The current value of the leftmost operand is then returned as a ref value.

All operands must be typed. All operands other than the last must be ref values and not of a constant type.

On each iteration, implicitly casts the right-hand side operand to the type of the left-hand side operand.

```
    sym x:i32 y:i32 z:i32;
    # assign to x, y, and z

    = x y z 1;
```

## `op oper0 oper...`

Performs arithmetic or bitwise operations on values.

`op` is one of `+` (addition, concatenation, or combining), `-` (subtraction), `*` (multiplication), `/` (division), `%` (remainder), `&` (bitwise conjunction), `|` (bitwise disjunction), `^` (bitwise exclusive disjunction), `<<` (bitwise shift left), or `>>` (bitwise shift right).

Performs iteratively from left to right. The first iteration performs its calculation on the leftmost operand and the one on the right-hand side of it and returns the result. Each following iteration performs its calculation on the value returned by the previous iteration and the next right-hand side operand, until all operands are used. Returns the value returned by the last iteration.

On each iteration, attempts to implicitly cast the right-hand side operand to the type of the left-hand side operand. If that is not allowed, implicitly casts the left-hand side operand to the type of the right-hand side operand.

If `op` is `/` and the right-hand side operand is an evaluated value, it must not be equal to zero. If the right-hand side operand is a compiled value equal to zero, the compiled program will have undefined behaviour.

If `op` is `<<` and the left-hand side operand is an evaluated value, it must not be negative. If the left-hand side operand is a compiled value and negative, the compiled program will have undefined behaviour.

If `op` is `<<` or `>>` and the right-hand side operand is an evaluated value, it must not be negative. If the right-hand side operand is a compiled value and negative, the compiled program will have undefined behaviour.

If `op` is `+`, the operands must be of one of numeric types, of type `id`, or of type `raw`.

If `op` is `-`, `*`, `/`, `%`, `>>`, or `<<`, the operands must be of a numeric type.

If `op` is `&`, `|`, or `^`, the operands must be of an integer type.

```
    std.println (+ 1 2 3 4);
    std.println (/ 12 3 4);
```

If `op` is `+` and operands are of type `id`, the result is a new `id` guaranteed to be unique to the pairing of those two operand values.

```
    message (+ \foo (cast id i32));
```

If `op` is `+` and operands are of type `raw`, the result is a new `raw` with its list of elements being the concatenation of elements of the left and right-hand side operands. The elements preserve their non-value node properties.

```
    = r (+ r \{ += ,x 1; });
```

`::bare` on `op` when it is `+` and operands are of type `id` results in a new `id` being the concatenation of the identifier values of the left and right-hand side operand. This value is not guaranteed to be unique to the pairing of those two operand values.

## `opComp oper0 oper... =bool`

Performs comparisons on values.

`opComp` is one of `==` (equal), `!=` (not equal), `<` (less than), `<=` (less than or equal), `>` (greater than), or `>=` (greater than or equal).

Performs iteratively from left to right. Each iteration performs its comparison on the next pair of left and right-hand side operands, until all operands are used. If any comparison is not satisfied, stops the instructions and returns `false`. Otherwise, returns `true` after the last iteration.

On each iteration, attempts to implicitly cast the right-hand side operand to the type of the left-hand side operand. If that is not allowed, implicitly casts the left-hand side operand to the type of the right-hand side operand. The implicit casting is preserved for use by the next iteration.

If `op` is `!=`, there must be exactly two operands (it is not variadic).

If `op` is `==` or `!=`, the operands must be of one of numeric, char, pointer, boolean, identifier, type, or callable types.

If `op` is `<`, `<=`, `>`, or `>=`, the operands must be of one of numeric or char types.

```
    message (== a b 0);

    message (< 1 2 3 4);

    message (!= x 0);
```

## `[] base ind<integer>...`

Gets an element of `base` or an element nested within it.

Performs iteratively from left to right. The first iteration gets the element of `base` and the leftmost index and returns the result as a ref value. Each following iteration gets the element of the value returned by the previous iteration and the next index, until all indexes are used. Returns the value returned by the last iteration.

On each iteration, the base must be a `raw`, a tuple, a data type, an array, or an array pointer.

If the base is a `raw` or a tuple, the index must be an evaluated integer. Its value must be a valid element index for the base.

If the base is a data type, the index must be an evaluated integer or an `id`. If it is an integer, its value must be a valid element index for that data type. If it is an `id`, its value must be the name of one of the elements in that data type.

If the base is an array or an array pointer, the index must be an integer. If it is an evaluated value, its value must be a valid element index for that array. If it or the base is a compiled value and its value is not a valid element index for that array, the compiled program will have undefined behaviour.

If the base is an array pointer and an evaluated value, it must not be null.

If the base is an array pointer and the index is not a valid element index for the array pointed to, the compiled program will have undefined behaviour.

If the base is a `raw` and the indexed element is a `raw`, or if the base is a tuple, a data type, or an array, the ref value of the result will be derived from the ref value of the base. If the base is of a constant type, the result will be of a constant type.

If the base is a `raw` and the indexed element is not a `raw`, it will preserve its non-value node properties.

If the base is an array pointer, the element type must not be an undefined type. The result will be a ref value.

If the base is an evaluated array pointer which is a string and the index is an evaluated value, the result will be a non-ref value.

```
    std.println ([] array 0);

    std.println ([] matrix i j);
```

> The base must not an owning non-ref value.